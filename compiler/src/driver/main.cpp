// xbc - the xbase compiler driver.
#include "codegen/CodeGen.h"
#include "codegen/Emit.h"
#include "common/Diagnostics.h"
#include "common/Text.h"
#include "driver/Link.h"
#include "driver/Project.h"
#include "driver/RuntimeLibrary.h"
#include "opt/Fold.h"
#include "parser/Parser.h"
#include "preprocessor/Preprocessor.h"
#include "sema/Calls.h"
#include "sema/Classes.h"
#include "sema/Sema.h"
#include "xir/Gen.h"
#include "xir/PassManager.h"
#include "xir/Printer.h"
#include "xir/Verifier.h"
#include "xbrt.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/GlobPattern.h>
#include <llvm/Support/MemoryBuffer.h>
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
#include <functional>
#include <set>
#include <string>
#include <vector>

#ifndef XB_INCLUDE_FROM_BIN
#error "XB_INCLUDE_FROM_BIN (where the standard headers are from xbc's folder) comes from the top-level CMakeLists.txt"
#endif
#ifndef XB_MODULES_FROM_BIN
#error "XB_MODULES_FROM_BIN (where the module files are from xbc's folder) comes from the top-level CMakeLists.txt"
#endif
#ifndef XB_RUNTIME_MODULE
#error "XB_RUNTIME_MODULE (the runtime's module file's name) comes from the top-level CMakeLists.txt"
#endif
#ifndef XB_DEFAULT_INCLUDE_DIR
#define XB_DEFAULT_INCLUDE_DIR ""
#endif
#ifndef XB_BUILD_MODULE_DIR
#define XB_BUILD_MODULE_DIR ""
#endif

namespace {

const char *kUsage =
    "usage: xbc [options] <file.prg>...\n"
    "       xbc [options] <project.xbprj>\n"
    "\n"
    "  Every source is a module; all of them go into one object, where calls between them are direct.\n"
    "  A name with * or ? stands for the files it matches. A project file builds a program or a library\n"
    "  in one step (docs/projects.md).\n"
    "\n"
    "  -o <file>          the program (default: the first source's name), with -c the object (default:\n"
    "                     the first source's name with .o), with --library the archive\n"
    "  -c                 write the object only, without linking the program (the runtime and the libraries)\n"
    "  --library=<name>   compile an xbase library: its archive and its module file <name>.xbm\n"
    "  --lib=<name>       use the xbase library <name> (found by its module file <name>.xbm)\n"
    "  -L<dir>            add a folder to look for module files in\n"
    "  -l<name>           link the system library <name> too\n"
    "  --out-dir=<dir>    where a project's program, library or object goes (default: the current folder)\n"
    "  -I<dir>            add an include directory\n"
    "  -D<name>[=<val>]   #define <name> [<val>]\n"
    "  -u                 do not load std.ch\n"
    "  -u<file>           use <file> instead of std.ch\n"
    "  -E                 preprocess only, write the result to stdout\n"
    "  -p                 also write each preprocessed source to <file>.ppo\n"
    "  -Wno-undeclared    no warning for a name nothing declares (a memory variable at run time)\n"
    "  -Werror=undeclared such a name is an error\n"
    "  -O0 | -O1 | -O2 | -O3\n"
    "                     optimization level (default -O2)\n"
    "  --target=<triple>  cross-compile for <triple> (default: host)\n"
    "  --cpu=<name>       target CPU (default: generic, runs on any CPU of the target); native: this\n"
    "                     machine's CPU and features (the program may not run on other machines)\n"
    "  --features=<list>  target features, e.g. +sse4.2,-avx\n"
    "  --static           non-PIC code\n"
    "  --runtime          compile modules of the runtime library: their public functions are the\n"
    "                     runtime's (xbf_NAME, declared NATIVE in rtl.ch); implies --no-entry\n"
    "  --no-entry         do not emit the program entry (xb_app_main), and write an object: the modules\n"
    "                     are linked into a program whose entry another object has\n"
    "  --depfile=<file>   write a Make-style dependency file for the object\n"
    "  --dump-ir=<file>   debug: also write the LLVM module as text\n"
    "  --dump-xir=<file>  debug: also write the module's xIR as text\n"
    "  --dump-xir-types   debug: with --dump-xir, each item value's inferred types too\n"
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

// A folder of the install, by the path from the binary's folder to it (`fromBin`), then `buildDir`, the build tree's
// (empty when xbc holds no path of the build: XB_SOURCE_INCLUDE off).
std::vector<std::string> installDirs(const char *argv0, const char *fromBin, const char *buildDir) {
  std::vector<std::string> dirs;
  std::string exe = llvm::sys::fs::getMainExecutable(argv0, (void *)&installDirs);
  if (!exe.empty()) {
    llvm::SmallString<256> p(llvm::sys::path::parent_path(exe));
    llvm::sys::path::append(p, fromBin);
    llvm::sys::path::remove_dots(p, /*remove_dot_dot=*/true);
    dirs.push_back(std::string(p));
  }
  if (*buildDir) dirs.push_back(buildDir);
  return dirs;
}

// The standard headers: where the install puts them (XB_INCLUDE_FROM_BIN), or the source tree when running from a
// build directory (XB_DEFAULT_INCLUDE_DIR).
std::vector<std::string> defaultIncludeDirs(const char *argv0) {
  return installDirs(argv0, XB_INCLUDE_FROM_BIN, XB_DEFAULT_INCLUDE_DIR);
}

// The module files of the runtime and of the libraries installed with xbc (XB_MODULES_FROM_BIN), or the build
// tree's runtime (XB_BUILD_MODULE_DIR), after the -L folders.
std::vector<std::string> defaultModuleDirs(const char *argv0) {
  return installDirs(argv0, XB_MODULES_FROM_BIN, XB_BUILD_MODULE_DIR);
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
  auto fold = [&](const std::string &t) { return foldCase ? xb::lowerCase(t) : t; };
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

// The files a preprocessor read, added to `filesRead` (the dependency file's list) once each.
void addFilesRead(std::vector<std::string> &filesRead, const xb::Preprocessor &pp) {
  for (const std::string &f : pp.filesRead())
    if (std::find(filesRead.begin(), filesRead.end(), f) == filesRead.end()) filesRead.push_back(f);
}

// The syntax tree of a preprocessed source, its names resolved and checked; null after an error.
// `declared`: declarations the module gets before its own (the public functions of the libraries it uses).
std::unique_ptr<xb::Module> analyze(std::vector<xb::Token> tokens, const std::string &path, xb::Diagnostics &diag,
                                    bool runtimeLibrary, xb::Sema::Undeclared undeclared,
                                    const std::vector<xb::ExternDecl> &declared = {}) {
  unsigned errors = diag.errorCount();
  xb::Parser parser(std::move(tokens), diag);
  auto ast = parser.parseModule(moduleNameFor(path));
  ast->file = llvm::sys::path::filename(path).str();
  if (diag.errorCount() != errors) return nullptr;
  ast->externs.insert(ast->externs.begin(), declared.begin(), declared.end());
  xb::ClassResolver(diag).run(*ast);
  if (diag.errorCount() != errors) return nullptr;
  xb::CallResolver(diag, xb::Folder::canEvaluate, runtimeLibrary).run(*ast);
  if (diag.errorCount() != errors) return nullptr;
  xb::Folder().run(*ast); // before Sema: STATIC initializers are checked once folded
  xb::Sema(diag, undeclared).run(*ast);
  if (diag.errorCount() != errors) return nullptr;
  return ast;
}

// FNV-1a 64 of a source's text: the stamp a carried runtime source's module and the runtime's own share; a library's
// goes on through its sources (`h` the hash so far).
constexpr uint64_t kSourceHashBasis = 0xcbf29ce484222325u;
uint64_t sourceHash(std::string_view text, uint64_t h = kSourceHashBasis) {
  for (char c : text) {
    h ^= static_cast<unsigned char>(c);
    h *= 0x100000001b3u;
  }
  return h;
}

// Whether the module calls a function of the carried runtime sources (by name, as a function of the runtime) or sends
// a message one of their classes declares.
bool callsRuntimeLibrary(const xb::xir::Module &m) {
  auto in = [](const std::vector<std::string> &names, const std::string &name) {
    return std::find(names.begin(), names.end(), name) != names.end();
  };
  for (auto &f : m.functions)
    for (auto &b : f->blocks)
      for (auto &i : b->insts)
        for (const xb::EmbeddedSource &s : xb::runtimeLibrarySources())
          if ((i->op == xb::xir::Op::Call && i->callee.native && in(s.functions, i->callee.name)) ||
              (i->op == xb::xir::Op::Send && in(s.messages, i->message)))
            return true;
  return false;
}

// The runtime's carried sources, compiled as modules of the runtime are (with std.ch, found in the include
// directories, and the target's defines only) and through the passes that run before the final ones, for calls of
// their functions to inline. Without std.ch there is nothing to inline (empty, no error); a source that does not
// compile is an internal error (`failed`). The std.ch read goes into `filesRead`.
std::vector<std::unique_ptr<xb::xir::Module>> loadRuntimeLibrary(const std::vector<std::string> &includeDirs,
                                                                const std::vector<std::string> &predefined,
                                                                std::vector<std::string> &filesRead, bool &failed) {
  std::vector<std::unique_ptr<xb::xir::Module>> out;
  failed = false;
  for (const xb::EmbeddedSource &s : xb::runtimeLibrarySources()) {
    xb::Diagnostics diag;
    xb::Preprocessor pp(diag);
    for (const std::string &d : includeDirs) pp.addIncludeDir(d);
    for (const std::string &d : predefined) pp.defineFromCommandLine(d);
    std::string stdPath = pp.findInclude("std.ch", "");
    if (stdPath.empty()) return {};
    pp.processFile(stdPath);
    addFilesRead(filesRead, pp);
    pp.processText(s.name, s.text);
    auto ast = diag.errorCount() ? nullptr : analyze(pp.takeOutput(), s.name, diag, true, xb::Sema::Undeclared::Ignore);
    if (!ast) {
      std::fprintf(stderr, "xbc: internal error: the carried runtime source %s does not compile\n", s.name);
      failed = true;
      return {};
    }
    auto m = xb::xir::Generator(true).run(*ast);
    std::string bad = xb::xir::verify(*m);
    if (bad.empty()) bad = xb::xir::optimize(*m, false);
    if (!bad.empty()) {
      std::fprintf(stderr, "xbc: internal error: invalid xIR in the carried runtime source %s: %s\n", s.name,
                   bad.c_str());
      failed = true;
      return {};
    }
    for (auto &f : m->functions) f->carried = true;
    m->sourceHash = sourceHash(s.text);
    out.push_back(std::move(m));
  }
  return out;
}

// The function that starts the program: a public MAIN in any module, or else
// the first function (not an INIT or EXIT procedure) of the first source that
// has one.
const xb::xir::Function *startFunction(const std::vector<Source> &sources) {
  for (const Source &s : sources)
    for (auto &f : s.xir->functions)
      if (f->kind == xb::xir::Function::Kind::Program && !f->internal && f->name == "MAIN") return f.get();
  for (const Source &s : sources)
    if (const xb::xir::Function *f = xb::xir::firstProgramFunction(*s.xir)) return f;
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
  for (const std::string &family : xb::platformFamilies(t)) d.push_back("__PLATFORM__" + family + "=1");
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

// A library's public function cannot have the name of a declared one (a runtime function's that is not RESERVED, or
// another library's): every program using the library gets its declaration beside that one, which conflicts.
void checkLibraryNames(const std::vector<Source> &sources, xb::Diagnostics &diag) {
  for (const Source &s : sources)
    for (auto &f : s.ast->funcs) {
      if (f->isStatic || f->member != xb::FuncDecl::Member::None) continue;
      for (const xb::ExternDecl &d : s.ast->externs)
        if (d.name == f->name) {
          diag.error(f->loc, "a library's public function cannot be named like the declared function '" + d.name +
                                 "' (programs using the library would get both declarations)");
          diag.note(d.loc, "the declaration is here");
          break;
        }
    }
}

// Links the program `output` from `objects`, the libraries and the runtime (whose module file and archive go into
// `depends`, for the dependency file).
bool linkStep(const std::string &output, const std::vector<std::string> &objects,
              const std::vector<xb::ModuleFile> &libraries, const std::vector<std::string> &system,
              const std::vector<std::string> &moduleDirs, const llvm::Triple &triple,
              std::vector<std::string> &depends) {
  std::string err;
  std::optional<xb::ModuleFile> runtime = xb::findModuleFile(XB_RUNTIME_MODULE, moduleDirs, triple, err);
  if (!runtime) {
    std::fprintf(stderr, "xbc: %s\n", err.c_str());
    return false;
  }
  depends.push_back(runtime->path);
  depends.push_back(xb::archivePath(*runtime));
  // System libraries after every archive, each once, in its last place (where the last archive that needs it is).
  std::vector<std::string> all;
  for (const xb::ModuleFile &l : libraries) all.insert(all.end(), l.system.begin(), l.system.end());
  all.insert(all.end(), system.begin(), system.end());
  all.insert(all.end(), runtime->system.begin(), runtime->system.end());
  std::vector<std::string> once;
  for (size_t k = 0; k < all.size(); ++k)
    if (std::find(all.begin() + (std::ptrdiff_t)k + 1, all.end(), all[k]) == all.end()) once.push_back(all[k]);
  if (!xb::linkProgram(output, objects, libraries, *runtime, once, err)) {
    std::fprintf(stderr, "xbc: %s\n", err.c_str());
    return false;
  }
  return true;
}

// An object file given to link (`.o`, `.obj`), not a source.
bool isObjectFile(const std::string &path) {
  llvm::StringRef ext = llvm::sys::path::extension(path);
  return ext.equals_insensitive(".o") || ext.equals_insensitive(".obj");
}

} // namespace

static int compile(int argc, char **argv) {
  xb::TargetConfig tcfg;
  std::string output, irDump, xirDump, depFile, stdFile = "std.ch", libraryName, outDir;
  bool xirTypes = false, compileOnly = false;
  std::vector<std::string> inputs, includeDirs, defines, libs, libDirs, system;
  bool emitEntry = true, ppOnly = false, ppFile = false, useStd = true, customStd = false, runtimeLibrary = false;
  xb::Sema::Undeclared undeclared = xb::Sema::Undeclared::Warn;

  // A project file: its options come first, so that the command line's override them.
  std::vector<std::string> args(argv + 1, argv + argc);
  std::string projectPath;
  for (size_t i = 0; i < args.size(); ++i) {
    bool operand = i > 0 && (args[i - 1] == "-o" || args[i - 1] == "-I");
    if (operand || args[i][0] == '-' || !llvm::sys::path::extension(args[i]).equals_insensitive(".xbprj")) continue;
    if (!projectPath.empty()) {
      std::fputs("xbc: one project file at a time\n", stderr);
      return 2;
    }
    projectPath = args[i];
    args.erase(args.begin() + (std::ptrdiff_t)i--);
  }
  std::vector<xb::KeyLine> projectLines;
  if (!projectPath.empty()) {
    std::string err;
    auto lines = xb::readKeyFile(projectPath, err);
    if (!lines) {
      std::fprintf(stderr, "xbc: %s\n", err.c_str());
      return 1;
    }
    projectLines = std::move(*lines);
    std::optional<std::vector<std::string>> options = xb::projectOptions(projectPath, projectLines, err);
    if (!options) {
      std::fprintf(stderr, "xbc: %s\n", err.c_str());
      return 1;
    }
    args.insert(args.begin(), options->begin(), options->end());
  }

  for (size_t i = 0; i < args.size(); ++i) {
    const char *a = args[i].c_str();
    bool next = i + 1 < args.size();
    if (!std::strcmp(a, "-o") && next) output = args[++i];
    else if (startsWith(a, "-I") && a[2]) includeDirs.push_back(a + 2);
    else if (!std::strcmp(a, "-I") && next) includeDirs.push_back(args[++i]);
    else if (startsWith(a, "-L") && a[2]) libDirs.push_back(a + 2);
    else if (startsWith(a, "-l") && a[2]) system.push_back(a + 2);
    else if (startsWith(a, "--lib=") && a[6]) libs.push_back(a + 6);
    else if (startsWith(a, "--library=") && a[10]) libraryName = a + 10;
    else if (!std::strcmp(a, "-c")) compileOnly = true;
    else if (startsWith(a, "--out-dir=") && a[10]) outDir = a + 10;
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
    else if (!std::strcmp(a, "-Wno-undeclared")) undeclared = xb::Sema::Undeclared::Ignore;
    else if (!std::strcmp(a, "-Werror=undeclared")) undeclared = xb::Sema::Undeclared::Error;
    else if (!std::strcmp(a, "--runtime")) {
      runtimeLibrary = true;
      emitEntry = false;
    }
    else if (startsWith(a, "--dump-ir=")) irDump = a + 10;
    else if (startsWith(a, "--dump-xir=")) xirDump = a + 11;
    else if (std::strcmp(a, "--dump-xir-types") == 0) xirTypes = true;
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
  llvm::Triple triple = xb::targetTriple(tcfg);
  std::string projectName;
  if (!projectPath.empty()) {
    std::string err;
    std::optional<xb::Project> project = xb::makeProject(projectPath, projectLines, triple, err);
    if (!project) {
      std::fprintf(stderr, "xbc: %s\n", err.c_str());
      return 1;
    }
    for (const std::string &s : project->sources)
      if (!expandInput(s, inputs)) return 2;
    includeDirs.insert(includeDirs.end(), project->includes.begin(), project->includes.end());
    defines.insert(defines.begin(), project->defines.begin(), project->defines.end());
    libs.insert(libs.end(), project->libs.begin(), project->libs.end());
    libDirs.insert(libDirs.end(), project->libDirs.begin(), project->libDirs.end());
    system.insert(system.end(), project->system.begin(), project->system.end());
    projectName = project->name;
    if (project->library && libraryName.empty()) libraryName = project->name;
  }
  if (!libraryName.empty() && (compileOnly || runtimeLibrary)) {
    std::fputs("xbc: --library makes a library: no -c, no --runtime\n", stderr);
    return 2;
  }
  // A library's files are named in lower case (lib<name>.a, <name>.xbm), its symbols in upper case
  // (XB_XBLIB_FUNC_PREFIX).
  std::string libraryFile = xb::lowerCase(libraryName);
  if (!libraryName.empty()) {
    if (!xb::isIdentifier(libraryName)) {
      std::fprintf(stderr, "xbc: '%s' is no library name (a name: letters, digits and _, not a digit first)\n",
                   libraryName.c_str());
      return 2;
    }
    libraryName = xb::upperCase(libraryName);
    emitEntry = false;
  }
  // A program is linked unless -c asks for its object; modules without the entry (--no-entry, --runtime) go into an
  // object, a library into its archive.
  bool link = !compileOnly && emitEntry;
  inputs = distinctFiles(inputs);
  std::vector<std::string> objects; // linked with the program
  for (const std::string &in : inputs)
    if (isObjectFile(in)) objects.push_back(in);
  inputs.erase(std::remove_if(inputs.begin(), inputs.end(), isObjectFile), inputs.end());
  if (!objects.empty() && !link) {
    std::fputs("xbc: object files are linked into a program: not with -c, --no-entry, --runtime or --library\n",
               stderr);
    return 2;
  }
  if (inputs.empty() && objects.empty()) {
    std::fputs(kUsage, stderr);
    return 2;
  }
  if (output.empty()) {
    const std::string &first = inputs.empty() ? objects.front() : inputs.front();
    std::string name = projectName.empty() ? llvm::sys::path::stem(first).str() : projectName;
    llvm::SmallString<256> p(outDir);
    llvm::sys::path::append(p, !libraryName.empty() ? xb::archiveName(libraryFile, triple)
                               : link              ? xb::executableName(name, triple)
                                                   : name + ".o");
    // An object goes beside its first source, a project's into the output folder.
    bool beside = !link && libraryName.empty() && projectName.empty();
    output = beside ? withExtension(inputs.front(), ".o") : std::string(p);
  }

  // The libraries the program uses, each before the ones it uses (as the linker wants them), and the declarations of
  // their public functions, which every source gets after std.ch.
  std::vector<std::string> moduleDirs = libDirs;
  for (const std::string &d : defaultModuleDirs(argv[0])) moduleDirs.push_back(d);
  std::vector<xb::ModuleFile> libraries;
  std::set<std::string> visiting;
  std::string libraryError;
  std::function<bool(const std::string &)> use = [&](const std::string &name) {
    if (!visiting.insert(xb::upperCase(name)).second) return true;
    std::optional<xb::ModuleFile> m = xb::findModuleFile(name, moduleDirs, triple, libraryError);
    if (!m) return false;
    for (const std::string &u : m->uses)
      if (!use(u)) return false;
    libraries.push_back(std::move(*m));
    return true;
  };
  for (const std::string &l : libs)
    if (!use(l)) {
      std::fprintf(stderr, "xbc: %s\n", libraryError.c_str());
      return 1;
    }
  std::reverse(libraries.begin(), libraries.end());
  // What the output depends on besides the files the preprocessor reads: the project file, the libraries' module files
  // and archives (and the runtime's, when it links).
  std::vector<std::string> depends;
  if (!projectPath.empty()) depends.push_back(projectPath);
  for (const xb::ModuleFile &m : libraries) {
    depends.push_back(m.path);
    depends.push_back(xb::archivePath(m));
  }
  auto writeDeps = [&](const std::vector<std::string> &filesRead) {
    std::vector<std::string> all = filesRead;
    all.insert(all.end(), depends.begin(), depends.end());
    if (depFile.empty() || writeDepFile(depFile, output, all)) return true;
    std::fprintf(stderr, "xbc: cannot write '%s'\n", depFile.c_str());
    return false;
  };
  auto makeOutDir = [&] { // the output's folder
    std::string dir = llvm::sys::path::parent_path(output).str();
    if (dir.empty()) return true;
    std::error_code ec = llvm::sys::fs::create_directories(dir);
    if (ec) std::fprintf(stderr, "xbc: cannot create '%s': %s\n", dir.c_str(), ec.message().c_str());
    return !ec;
  };
  if (inputs.empty()) // objects alone: linked
    return makeOutDir() && linkStep(output, objects, libraries, system, moduleDirs, triple, depends) && writeDeps({})
               ? 0
               : 1;
  xb::Diagnostics diag;
  std::vector<xb::ExternDecl> libraryDeclarations; // EXTERN FUNCTION NAME( ... ) LIBRARY LIB, at the module file
  unsigned level = 1;                              // a library's: 1 + the highest of the ones it uses
  for (const xb::ModuleFile &m : libraries) {
    level = std::max(level, m.level + 1);
    uint32_t file = diag.addFile(m.path);
    for (size_t k = 0; k < m.declarations.size(); ++k) {
      xb::ExternDecl d;
      d.loc = {file, m.declarationLines[k], 1};
      d.name = m.declarations[k];
      d.maxArgs = xb::ExternDecl::kUnlimited;
      d.library = m.library;
      libraryDeclarations.push_back(std::move(d));
    }
  }

  std::vector<std::string> predefined = targetDefines(triple);
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
    addFilesRead(filesRead, pp);
    if (diag.errorCount() != errors) continue;

    if (ppOnly || ppFile) {
      std::string text = pp.render(tokens, prelude);
      if (ppOnly) {
        std::fputs(text.c_str(), stdout);
        continue;
      }
      std::ofstream ppo(withExtension(input, ".ppo"), std::ios::binary);
      ppo << text;
    }

    auto ast = analyze(std::move(tokens), input, diag, runtimeLibrary, undeclared, libraryDeclarations);
    if (ast) sources.push_back({input, std::move(ast), nullptr});
  }
  if (ppOnly) return diag.errorCount() ? 1 : 0;
  checkDuplicates(sources, diag);
  if (!libraryName.empty()) checkLibraryNames(sources, diag);
  if (diag.errorCount()) return 1;

  std::string xirText;
  std::vector<std::unique_ptr<xb::xir::Module>> library; // loaded once a module calls one of its functions
  bool libraryLoaded = false;
  // A library's stamp: every file its sources read (themselves, std.ch, their includes) and the defines, in order.
  uint64_t libraryStamp = kSourceHashBasis;
  if (!libraryName.empty()) {
    for (const std::string &f : filesRead) {
      auto text = llvm::MemoryBuffer::getFile(f, /*IsText=*/false, /*RequiresNullTerminator=*/false);
      if (!text) {
        std::fprintf(stderr, "xbc: cannot read '%s'\n", f.c_str());
        return 1;
      }
      libraryStamp = sourceHash((*text)->getBuffer(), sourceHash(llvm::sys::path::filename(f), libraryStamp));
    }
    for (const std::string &d : defines) libraryStamp = sourceHash(d + "\n", libraryStamp);
  }
  std::vector<std::string> publicFunctions; // a library's
  for (Source &s : sources) {
    s.xir = xb::xir::Generator(runtimeLibrary).run(*s.ast);
    if (runtimeLibrary) { // its stamp: a program that ran a body of it in place requires the same text
      auto text = llvm::MemoryBuffer::getFile(s.path, /*IsText=*/false, /*RequiresNullTerminator=*/false);
      if (!text) {
        std::fprintf(stderr, "xbc: cannot read '%s'\n", s.path.c_str());
        return 1;
      }
      s.xir->sourceHash = sourceHash((*text)->getBuffer());
    }
    for (auto &f : s.xir->functions)
      if (f->kind == xb::xir::Function::Kind::Program && !f->internal) publicFunctions.push_back(f->name);
    if (std::string bad = xb::xir::verify(*s.xir); !bad.empty()) {
      std::fprintf(stderr, "xbc: internal error: invalid xIR: %s\n", bad.c_str());
      return 1;
    }
    if (tcfg.optLevel > 0 && !runtimeLibrary && !libraryLoaded && callsRuntimeLibrary(*s.xir)) {
      std::vector<std::string> dirs = includeDirs;
      for (const std::string &d : defaultIncludeDirs(argv[0])) dirs.push_back(d);
      bool failed;
      library = loadRuntimeLibrary(dirs, predefined, filesRead, failed);
      if (failed) return 1;
      libraryLoaded = true;
    }
    for (auto &l : library) s.xir->library.push_back(l.get());
    if (tcfg.optLevel > 0) {
      if (std::string bad = xb::xir::optimize(*s.xir); !bad.empty()) {
        std::fprintf(stderr, "xbc: internal error: invalid xIR %s\n", bad.c_str());
        return 1;
      }
    }
    if (!xirDump.empty()) xirText += xb::xir::print(*s.xir, xirTypes);
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
  if (!libraryName.empty())
    cg.setLibrary(libraryName, {publicFunctions.begin(), publicFunctions.end()}, libraryStamp, level);
  std::vector<std::pair<std::string, uint64_t>> stamps;
  for (const xb::ModuleFile &m : libraries) stamps.emplace_back(m.library, m.stamp);
  cg.requireLibraries(std::move(stamps));
  for (const Source &s : sources) cg.run(*s.xir);
  if (!cg.finish(emitEntry ? startFunction(sources) : nullptr)) return 1;

  // A function whose optimization makes the compilation slow: the program's author may split it.
  std::set<std::string> warned;
  for (auto [f, size] : xb::optimizeModule(mod, *tm, tcfg.optLevel)) {
    const xb::xir::Function *xf = cg.sourceOf(f);
    if (!xf || xf->blocks.empty() || xf->entry()->insts.empty() || !warned.insert(xf->stackName).second) continue;
    diag.warning(xf->entry()->insts.front()->loc,
                 "'" + xf->stackName + "' makes the compilation slow (" + std::to_string(size) +
                     " instructions to optimize): smaller functions compile faster");
  }

  if (!irDump.empty() && !xb::dumpIR(mod, irDump, err)) {
    std::fprintf(stderr, "xbc: %s\n", err.c_str());
    return 1;
  }
  if (!makeOutDir()) return 1;
  // A program or a library goes through an object of its own, removed once it is linked or archived.
  std::string object = output;
  if (link || !libraryName.empty()) {
    llvm::SmallString<256> tmp;
    if (std::error_code ec = llvm::sys::fs::createTemporaryFile("xbc", triple.isOSWindows() ? "obj" : "o", tmp)) {
      std::fprintf(stderr, "xbc: cannot create a temporary object file: %s\n", ec.message().c_str());
      return 1;
    }
    object = std::string(tmp);
  }
  struct RemoveObject {
    const std::string &object, &output;
    ~RemoveObject() {
      if (object != output) llvm::sys::fs::remove(object);
    }
  } removeObject{object, output};
  if (!xb::emitObject(mod, *tm, object, err)) {
    std::fprintf(stderr, "xbc: %s\n", err.c_str());
    return 1;
  }
  if (!libraryName.empty()) {
    xb::ModuleFile m;
    m.library = libraryName;
    m.abi = XB_ABI_VERSION;
    m.target = triple.str();
    m.archive = llvm::sys::path::filename(output).str();
    m.stamp = libraryStamp;
    m.level = level;
    for (const xb::ModuleFile &l : libraries) m.uses.push_back(l.library);
    m.system = system;
    m.declarations = publicFunctions;
    llvm::SmallString<256> xbm(llvm::sys::path::parent_path(output));
    llvm::sys::path::append(xbm, libraryFile + ".xbm");
    if (!xb::writeArchive(output, object, triple, err) || !xb::writeModuleFile(std::string(xbm), m, err)) {
      std::fprintf(stderr, "xbc: %s\n", err.c_str());
      return 1;
    }
  }
  objects.insert(objects.begin(), object);
  if (link && !linkStep(output, objects, libraries, system, moduleDirs, triple, depends)) return 1;
  return writeDeps(filesRead) ? 0 : 1;
}

int main(int argc, char **argv) {
  int rc = 1;
  llvm::thread worker(std::optional<unsigned>(kCompilerStackSize), [&] { rc = compile(argc, argv); });
  worker.join();
  return rc;
}
