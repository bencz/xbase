// xbc - the xbase compiler driver.
#include "codegen/CodeGen.h"
#include "codegen/Emit.h"
#include "common/Diagnostics.h"
#include "opt/Fold.h"
#include "parser/Parser.h"
#include "preprocessor/Preprocessor.h"
#include "sema/Calls.h"
#include "sema/Sema.h"
#include "xir/Gen.h"
#include "xir/PassManager.h"
#include "xir/Printer.h"
#include "xir/Verifier.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/GlobPattern.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/thread.h>
#include <llvm/TargetParser/Triple.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <memory>
#include <optional>
#include <unordered_map>
#include <fstream>
#include <string>
#include <vector>

#ifndef XB_DEFAULT_INCLUDE_DIR
#define XB_DEFAULT_INCLUDE_DIR ""
#endif

namespace {

const char *kUsage =
    "usage: xbc [options] <file.prg>...\n"
    "\n"
    "  Every source is a module; all of them go into one object, where calls between them are direct.\n"
    "  A name with * or ? stands for the files it matches.\n"
    "\n"
    "  -o <file>          output object file (default: the first source's name with .o)\n"
    "  -I<dir>            add an include directory\n"
    "  -D<name>[=<val>]   #define <name> [<val>]\n"
    "  -u                 do not load std.ch\n"
    "  -u<file>           use <file> instead of std.ch\n"
    "  -E                 preprocess only, write the result to stdout\n"
    "  -p                 also write each preprocessed source to <file>.ppo\n"
    "  -O0 | -O1 | -O2 | -O3\n"
    "                     optimization level (default -O2)\n"
    "  --target=<triple>  cross-compile for <triple> (default: host)\n"
    "  --cpu=<name>       target CPU (default: generic, runs on any CPU of the target); native: this\n"
    "                     machine's CPU and features (the program may not run on other machines)\n"
    "  --features=<list>  target features, e.g. +sse4.2,-avx\n"
    "  --static           non-PIC code\n"
    "  --runtime          compile modules of the runtime library: their public functions are the\n"
    "                     runtime's (xbf_NAME, declared NATIVE in rtl.ch); implies --no-entry\n"
    "  --no-entry         do not emit the program entry (xb_app_main): the modules are linked into\n"
    "                     a program whose entry another object has\n"
    "  --depfile=<file>   write a Make-style dependency file for the object\n"
    "  --dump-ir=<file>   debug: also write the LLVM module as text\n"
    "  --dump-xir=<file>  debug: also write the module's xIR as text\n"
    "  --print-targets    list the targets this xbc can generate code for\n"
    "  --version\n";

std::string moduleNameFor(const std::string &path) {
  std::string base = llvm::sys::path::stem(path).str();
  for (char &c : base) c = std::isalnum((unsigned char)c) ? (char)std::toupper((unsigned char)c) : '_';
  if (base.empty() || std::isdigit((unsigned char)base[0])) base = "_" + base;
  return base;
}

std::string withExtension(const std::string &path, const char *ext) {
  llvm::SmallString<256> p(path);
  llvm::sys::path::replace_extension(p, ext);
  return std::string(p);
}

bool startsWith(const char *s, const char *p) { return std::strncmp(s, p, std::strlen(p)) == 0; }

std::string makeEscape(const std::string &path) {
  std::string out;
  for (char c : path) {
    if (c == ' ' || c == '#') out += '\\';
    if (c == '$') out += '$';
    out += c;
  }
  return out;
}

// "object: inputs..." so build systems rebuild when std.ch or an include changes.
bool writeDepFile(const std::string &path, const std::string &target, const std::vector<std::string> &inputs) {
  std::ofstream f(path, std::ios::binary);
  if (!f) return false;
  f << makeEscape(target) << ':';
  for (const std::string &in : inputs) f << " \\\n  " << makeEscape(in);
  f << '\n';
  return (bool)f;
}

// Directory with the standard headers: next to the binary (<prefix>/include)
// or the source tree when running from a build directory.
std::vector<std::string> defaultIncludeDirs(const char *argv0) {
  std::vector<std::string> dirs;
  std::string exe = llvm::sys::fs::getMainExecutable(argv0, (void *)&defaultIncludeDirs);
  if (!exe.empty()) {
    llvm::SmallString<256> p(llvm::sys::path::parent_path(llvm::sys::path::parent_path(exe)));
    llvm::sys::path::append(p, "include");
    dirs.push_back(std::string(p));
  }
  if (*XB_DEFAULT_INCLUDE_DIR) dirs.push_back(XB_DEFAULT_INCLUDE_DIR);
  return dirs;
}

// A command-line source with `*` or `?` in its file name that names no file
// stands for the files of its directory it matches, sorted by name, leaving
// out names that start with a dot unless the pattern does (the shell of some
// platforms does not expand it). Only `*` and `?` are special. File names
// compare without case on Windows.
bool expandInput(const std::string &arg, std::vector<std::string> &inputs) {
  std::string name = llvm::sys::path::filename(arg).str();
  if (name.find_first_of("*?") == std::string::npos || llvm::sys::fs::exists(arg)) {
    inputs.push_back(arg);
    return true;
  }
  constexpr bool foldCase = llvm::sys::path::is_style_windows(llvm::sys::path::Style::native);
  auto fold = [&](std::string t) {
    if (foldCase)
      for (char &c : t) c = (char)std::tolower((unsigned char)c);
    return t;
  };
  std::string glob;
  for (char c : fold(name)) {
    if (std::strchr("[]{}\\", c)) glob += '\\'; // literal for the matcher
    glob += c;
  }
  auto pattern = llvm::GlobPattern::create(glob);
  if (!pattern) {
    llvm::consumeError(pattern.takeError());
    std::fprintf(stderr, "xbc: invalid file pattern '%s'\n", arg.c_str());
    return false;
  }
  std::string dir = llvm::sys::path::parent_path(arg).str();
  std::vector<std::string> found;
  std::error_code ec;
  for (llvm::sys::fs::directory_iterator it(dir.empty() ? "." : dir, ec), end; it != end && !ec; it.increment(ec)) {
    std::string file = llvm::sys::path::filename(it->path()).str();
    if (file[0] == '.' && name[0] != '.') continue;
    if (it->type() != llvm::sys::fs::file_type::directory_file && pattern->match(fold(file)))
      found.push_back(dir.empty() ? file : it->path());
  }
  if (found.empty()) {
    std::fprintf(stderr, "xbc: no file matches '%s'\n", arg.c_str());
    return false;
  }
  std::sort(found.begin(), found.end());
  inputs.insert(inputs.end(), found.begin(), found.end());
  return true;
}

// The stages walk the syntax tree recursively; a statement may hold a chain of
// tens of thousands of operators (the preprocessor's limit on a statement's
// tokens bounds it), so the compiler runs on a thread with this much stack.
constexpr unsigned kCompilerStackSize = 256u << 20;

// One source of the program, through the front end.
struct Source {
  std::string path;
  std::unique_ptr<xb::Module> ast;
  std::unique_ptr<xb::xir::Module> xir;
};

// The function that starts the program: a public MAIN in any module, or else
// the first function (not an INIT or EXIT procedure) of the first source that
// has one.
const xb::xir::Function *startFunction(const std::vector<Source> &sources) {
  for (const Source &s : sources)
    for (auto &f : s.xir->functions)
      if (f->kind == xb::xir::Function::Kind::Program && !f->internal && f->name == "MAIN") return f.get();
  for (const Source &s : sources)
    for (auto &f : s.xir->functions)
      if (f->kind == xb::xir::Function::Kind::Program && !f->startup) return f.get();
  return nullptr;
}

// The sources without repetitions (the same file named twice, for example
// `main.prg *.prg`), in their first order.
std::vector<std::string> distinctFiles(const std::vector<std::string> &inputs) {
  std::vector<std::string> out;
  std::vector<llvm::sys::fs::UniqueID> seen;
  for (const std::string &in : inputs) {
    llvm::sys::fs::UniqueID id;
    if (!llvm::sys::fs::getUniqueID(in, id)) {
      if (std::find(seen.begin(), seen.end(), id) != seen.end()) continue;
      seen.push_back(id);
    }
    out.push_back(in); // a file that cannot be read is reported when it is processed
  }
  return out;
}

// The compiler's version as one number, major * 10000 + minor * 100 + patch (0.1.0 is 100).
constexpr unsigned kVersionNumber = XB_VERSION_MAJOR * 10000 + XB_VERSION_MINOR * 100 + XB_VERSION_PATCH;

// The names every source starts with defined, from the target: the compiler, the operating system family, the word
// size, the byte order and the CPU family. A -D option given for one of them replaces it; #undef removes it.
std::vector<std::string> targetDefines(const llvm::Triple &t) {
  std::vector<std::string> d{"__XBASE__=" + std::to_string(kVersionNumber)};
  if (t.isOSWindows()) d.push_back("__PLATFORM__WINDOWS=1");
  if (t.isOSLinux()) d.push_back("__PLATFORM__LINUX=1");
  if (t.isOSDarwin()) d.push_back("__PLATFORM__DARWIN=1");
  bool bsd = t.isOSFreeBSD() || t.isOSNetBSD() || t.isOSOpenBSD() || t.isOSDragonFly();
  if (bsd) d.push_back("__PLATFORM__BSD=1");
  if (t.isOSLinux() || t.isOSDarwin() || bsd || t.isOSSolaris() || t.isOSAIX()) d.push_back("__PLATFORM__UNIX=1");
  if (t.isArch64Bit()) d.push_back("__ARCH64BIT__=1");
  else if (t.isArch32Bit()) d.push_back("__ARCH32BIT__=1");
  d.push_back(t.isLittleEndian() ? "__LITTLE_ENDIAN__=1" : "__BIG_ENDIAN__=1");
  d.push_back("__XBASE_CPU__=\"" + llvm::Triple::getArchTypeName(t.getArch()).str() + "\"");
  return d;
}

// Two modules cannot both define a public function of one name.
void checkDuplicates(const std::vector<Source> &sources, xb::Diagnostics &diag) {
  std::unordered_map<std::string, const xb::FuncDecl *> seen;
  for (const Source &s : sources)
    for (auto &f : s.ast->funcs) {
      if (f->isStatic) continue;
      auto [it, fresh] = seen.emplace(f->name, f.get());
      if (fresh) continue;
      diag.error(f->loc, "function '" + f->name + "' is already defined in another module");
      diag.note(it->second->loc, "previous definition is here");
    }
}

} // namespace

static int compile(int argc, char **argv) {
  xb::TargetConfig tcfg;
  std::string output, irDump, xirDump, depFile, stdFile = "std.ch";
  std::vector<std::string> inputs, includeDirs, defines;
  bool emitEntry = true, ppOnly = false, ppFile = false, useStd = true, customStd = false, runtimeLibrary = false;

  for (int i = 1; i < argc; ++i) {
    const char *a = argv[i];
    if (!std::strcmp(a, "-o") && i + 1 < argc) output = argv[++i];
    else if (startsWith(a, "-I") && a[2]) includeDirs.push_back(a + 2);
    else if (!std::strcmp(a, "-I") && i + 1 < argc) includeDirs.push_back(argv[++i]);
    else if (startsWith(a, "-D") && a[2]) defines.push_back(a + 2);
    else if (!std::strcmp(a, "-u")) useStd = false;
    else if (startsWith(a, "-u") && a[2]) {
      stdFile = a + 2;
      customStd = true;
    }
    else if (!std::strcmp(a, "-E")) ppOnly = true;
    else if (!std::strcmp(a, "-p")) ppFile = true;
    else if (a[0] == '-' && a[1] == 'O' && a[2] >= '0' && a[2] <= '3' && !a[3]) tcfg.optLevel = (unsigned)(a[2] - '0');
    else if (startsWith(a, "--target=")) tcfg.triple = a + 9;
    else if (startsWith(a, "--cpu=")) tcfg.cpu = a + 6;
    else if (startsWith(a, "--features=")) tcfg.features = a + 11;
    else if (!std::strcmp(a, "--static")) tcfg.pic = false;
    else if (!std::strcmp(a, "--no-entry")) emitEntry = false;
    else if (!std::strcmp(a, "--runtime")) {
      runtimeLibrary = true;
      emitEntry = false;
    }
    else if (startsWith(a, "--dump-ir=")) irDump = a + 10;
    else if (startsWith(a, "--dump-xir=")) xirDump = a + 11;
    else if (startsWith(a, "--depfile=")) depFile = a + 10;
    else if (!std::strcmp(a, "--print-targets")) {
      xb::initializeTargets();
      xb::printTargets();
      return 0;
    } else if (!std::strcmp(a, "--version")) {
      std::printf("xbc %d.%d.%d (xbase compiler)\n", XB_VERSION_MAJOR, XB_VERSION_MINOR, XB_VERSION_PATCH);
      return 0;
    } else if (!std::strcmp(a, "-h") || !std::strcmp(a, "--help")) {
      std::fputs(kUsage, stdout);
      return 0;
    } else if (a[0] == '-') {
      std::fprintf(stderr, "xbc: unknown option '%s'\n%s", a, kUsage);
      return 2;
    } else if (!expandInput(a, inputs)) {
      return 2;
    }
  }
  inputs = distinctFiles(inputs);
  if (inputs.empty()) {
    std::fputs(kUsage, stderr);
    return 2;
  }
  if (output.empty()) output = withExtension(inputs.front(), ".o");

  std::vector<std::string> predefined = targetDefines(xb::targetTriple(tcfg));
  xb::Diagnostics diag;
  std::string stdPath;
  std::vector<std::string> filesRead;
  std::vector<Source> sources;
  for (const std::string &input : inputs) {
    // Each source is preprocessed on its own: its #defines and rules do not
    // reach the next one.
    xb::Preprocessor pp(diag);
    for (const std::string &d : includeDirs) pp.addIncludeDir(d);
    for (const std::string &d : defaultIncludeDirs(argv[0])) pp.addIncludeDir(d);
    for (const std::string &d : predefined) pp.defineFromCommandLine(d);
    for (const std::string &d : defines) pp.defineFromCommandLine(d);
    if (useStd) {
      // An explicit -u<file> is a path like any other file argument.
      if (stdPath.empty())
        stdPath = customStd && llvm::sys::fs::exists(stdFile) ? stdFile : pp.findInclude(stdFile, "");
      if (stdPath.empty()) {
        std::fprintf(stderr, "xbc: cannot find '%s' (use -I<dir>, or -u to compile without it)\n", stdFile.c_str());
        return 1;
      }
      pp.processFile(stdPath);
    }
    // What std.ch (and the files it includes, such as rtl.ch) produces is the
    // program's prelude: parsed, but not part of the preprocessed listing.
    size_t prelude = pp.outputSize();
    unsigned errors = diag.errorCount();
    pp.processFile(input);
    std::vector<xb::Token> tokens = pp.takeOutput();
    for (const std::string &f : pp.filesRead())
      if (std::find(filesRead.begin(), filesRead.end(), f) == filesRead.end()) filesRead.push_back(f);
    if (diag.errorCount() != errors) continue;

    if (ppOnly || ppFile) {
      std::string text = xb::Preprocessor::render(tokens, prelude);
      if (ppOnly) {
        std::fputs(text.c_str(), stdout);
        continue;
      }
      std::ofstream ppo(withExtension(input, ".ppo"), std::ios::binary);
      ppo << text;
    }

    xb::Parser parser(std::move(tokens), diag);
    auto ast = parser.parseModule(moduleNameFor(input));
    if (diag.errorCount() != errors) continue;
    xb::CallResolver(diag, xb::Folder::canEvaluate, runtimeLibrary).run(*ast);
    if (diag.errorCount() != errors) continue;
    xb::Folder().run(*ast); // before Sema: STATIC initializers are checked once folded
    xb::Sema(diag).run(*ast);
    if (diag.errorCount() != errors) continue;
    sources.push_back({input, std::move(ast), nullptr});
  }
  if (ppOnly) return diag.errorCount() ? 1 : 0;
  checkDuplicates(sources, diag);
  if (diag.errorCount()) return 1;

  std::string xirText;
  for (Source &s : sources) {
    s.xir = xb::xir::Generator(runtimeLibrary).run(*s.ast);
    if (std::string bad = xb::xir::verify(*s.xir); !bad.empty()) {
      std::fprintf(stderr, "xbc: internal error: invalid xIR: %s\n", bad.c_str());
      return 1;
    }
    if (tcfg.optLevel > 0) {
      if (std::string bad = xb::xir::optimize(*s.xir); !bad.empty()) {
        std::fprintf(stderr, "xbc: internal error: invalid xIR %s\n", bad.c_str());
        return 1;
      }
    }
    if (!xirDump.empty()) xirText += xb::xir::print(*s.xir);
  }
  if (!xirDump.empty()) {
    std::ofstream out(xirDump, std::ios::binary);
    out << xirText;
    if (!out) {
      std::fprintf(stderr, "xbc: cannot write '%s'\n", xirDump.c_str());
      return 1;
    }
  }

  xb::initializeTargets();
  std::string err;
  auto tm = xb::createTargetMachine(tcfg, err);
  if (!tm) {
    std::fprintf(stderr, "xbc: %s\n", err.c_str());
    return 1;
  }
  llvm::LLVMContext llctx;
  llvm::Module mod(sources.front().ast->name, llctx);
  mod.setTargetTriple(tm->getTargetTriple());
  mod.setDataLayout(tm->createDataLayout());
  mod.setSourceFileName(sources.front().path);

  xb::CodeGen cg(llctx, mod, diag);
  cg.setRuntimeLibrary(runtimeLibrary);
  for (const Source &s : sources) cg.run(*s.xir);
  if (!cg.finish(emitEntry ? startFunction(sources) : nullptr)) return 1;

  xb::optimizeModule(mod, *tm, tcfg.optLevel);

  if (!irDump.empty() && !xb::dumpIR(mod, irDump, err)) {
    std::fprintf(stderr, "xbc: %s\n", err.c_str());
    return 1;
  }
  if (!xb::emitObject(mod, *tm, output, err)) {
    std::fprintf(stderr, "xbc: %s\n", err.c_str());
    return 1;
  }
  if (!depFile.empty() && !writeDepFile(depFile, output, filesRead)) {
    std::fprintf(stderr, "xbc: cannot write '%s'\n", depFile.c_str());
    return 1;
  }
  return 0;
}

int main(int argc, char **argv) {
  int rc = 1;
  llvm::thread worker(std::optional<unsigned>(kCompilerStackSize), [&] { rc = compile(argc, argv); });
  worker.join();
  return rc;
}
