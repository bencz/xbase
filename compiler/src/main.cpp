// xbc - the xbase compiler driver.
#include "CodeGen.h"
#include "Diagnostics.h"
#include "Emit.h"
#include "Parser.h"
#include "Preprocessor.h"
#include "Sema.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#ifndef XB_DEFAULT_INCLUDE_DIR
#define XB_DEFAULT_INCLUDE_DIR ""
#endif

namespace {

const char *kUsage =
    "usage: xbc [options] <file.prg>\n"
    "\n"
    "  -o <file>          output object file (default: <file>.o)\n"
    "  -I<dir>            add an include directory\n"
    "  -D<name>[=<val>]   #define <name> [<val>]\n"
    "  -u                 do not load std.ch\n"
    "  -u<file>           use <file> instead of std.ch\n"
    "  -E                 preprocess only, write the result to stdout\n"
    "  -p                 also write the preprocessed source to <file>.ppo\n"
    "  -O0 | -O1 | -O2 | -O3\n"
    "                     optimization level (default -O2)\n"
    "  --target=<triple>  cross-compile for <triple> (default: host)\n"
    "  --cpu=<name>       target CPU (default: generic)\n"
    "  --features=<list>  target features, e.g. +sse4.2,-avx\n"
    "  --static           non-PIC code\n"
    "  --no-entry         do not emit the program entry (xb_app_main)\n"
    "  --depfile=<file>   write a Make-style dependency file for the object\n"
    "  --dump-ir=<file>   debug: also write the LLVM module as text\n"
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

} // namespace

int main(int argc, char **argv) {
  xb::TargetConfig tcfg;
  std::string input, output, irDump, depFile, stdFile = "std.ch";
  std::vector<std::string> includeDirs, defines;
  bool emitEntry = true, ppOnly = false, ppFile = false, useStd = true, customStd = false;

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
    else if (startsWith(a, "--dump-ir=")) irDump = a + 10;
    else if (startsWith(a, "--depfile=")) depFile = a + 10;
    else if (!std::strcmp(a, "--print-targets")) {
      xb::initializeTargets();
      xb::printTargets();
      return 0;
    } else if (!std::strcmp(a, "--version")) {
      std::puts("xbc 0.1.0 (xbase compiler)");
      return 0;
    } else if (!std::strcmp(a, "-h") || !std::strcmp(a, "--help")) {
      std::fputs(kUsage, stdout);
      return 0;
    } else if (a[0] == '-') {
      std::fprintf(stderr, "xbc: unknown option '%s'\n%s", a, kUsage);
      return 2;
    } else if (input.empty()) {
      input = a;
    } else {
      std::fprintf(stderr, "xbc: only one input file is supported for now\n");
      return 2;
    }
  }
  if (input.empty()) {
    std::fputs(kUsage, stderr);
    return 2;
  }
  if (output.empty()) output = withExtension(input, ".o");

  xb::Diagnostics diag;

  xb::Preprocessor pp(diag);
  for (const std::string &d : includeDirs) pp.addIncludeDir(d);
  for (const std::string &d : defaultIncludeDirs(argv[0])) pp.addIncludeDir(d);
  for (const std::string &d : defines) pp.defineFromCommandLine(d);
  if (useStd) {
    // An explicit -u<file> is a path like any other file argument.
    std::string path = customStd && llvm::sys::fs::exists(stdFile) ? stdFile : pp.findInclude(stdFile, "");
    if (path.empty()) {
      std::fprintf(stderr, "xbc: cannot find '%s' (use -I<dir>, or -u to compile without it)\n", stdFile.c_str());
      return 1;
    }
    pp.processFile(path);
  }
  pp.processFile(input);
  std::vector<xb::Token> tokens = pp.takeOutput();
  if (diag.errorCount()) return 1;

  if (ppOnly || ppFile) {
    std::string text = xb::Preprocessor::render(tokens);
    if (ppOnly) {
      std::fputs(text.c_str(), stdout);
      return 0;
    }
    std::ofstream ppo(withExtension(input, ".ppo"), std::ios::binary);
    ppo << text;
  }

  xb::Parser parser(std::move(tokens), diag);
  auto ast = parser.parseModule(moduleNameFor(input));
  if (diag.errorCount()) return 1;
  xb::Sema sema(diag);
  sema.run(*ast);
  if (diag.errorCount()) return 1;

  xb::initializeTargets();
  std::string err;
  auto tm = xb::createTargetMachine(tcfg, err);
  if (!tm) {
    std::fprintf(stderr, "xbc: %s\n", err.c_str());
    return 1;
  }
  llvm::LLVMContext llctx;
  llvm::Module mod(ast->name, llctx);
  mod.setTargetTriple(tm->getTargetTriple());
  mod.setDataLayout(tm->createDataLayout());
  mod.setSourceFileName(input);

  xb::CodeGen cg(llctx, mod, diag);
  if (!cg.run(*ast, emitEntry)) return 1;

  xb::optimizeModule(mod, *tm, tcfg.optLevel);

  if (!irDump.empty() && !xb::dumpIR(mod, irDump, err)) {
    std::fprintf(stderr, "xbc: %s\n", err.c_str());
    return 1;
  }
  if (!xb::emitObject(mod, *tm, output, err)) {
    std::fprintf(stderr, "xbc: %s\n", err.c_str());
    return 1;
  }
  if (!depFile.empty() && !writeDepFile(depFile, output, pp.filesRead())) {
    std::fprintf(stderr, "xbc: cannot write '%s'\n", depFile.c_str());
    return 1;
  }
  return 0;
}
