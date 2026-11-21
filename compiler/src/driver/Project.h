// Project files (.xbprj) and module files (.xbm): docs/projects.md.
#pragma once

#include "xbrt.h"

#include <llvm/TargetParser/Triple.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace xb {

// One `key = value` line of a project or module file; `qualifier` is what follows the key's dot (`system.windows`).
struct KeyLine {
  std::string key, qualifier, value;
  unsigned line = 0;
};

// The settings of a file, in order (a UTF-8 byte order mark before them skipped); nullopt (and `error`) when it cannot
// be read or a line is not `key = value`.
std::optional<std::vector<KeyLine>> readKeyFile(const std::string &path, std::string &error);

// The words of a value, separated by blanks; double quotes keep blanks in a word (and are removed). Nullopt for a quote
// left open.
std::optional<std::vector<std::string>> splitWords(const std::string &value);

// The names of the target's platform families (windows, linux, darwin, bsd, unix): a project's qualifiers, and the
// __PLATFORM__ names every source starts with.
std::vector<std::string> platformFamilies(const llvm::Triple &t);

// A project (.xbprj): a program or a library, built by `xbc <file>.xbprj` in one step.
struct Project {
  std::string name;                  // the program's or library's
  bool library = false;              // type = library
  std::vector<std::string> sources;  // paths, relative ones from the project's folder
  std::vector<std::string> includes; // likewise
  std::vector<std::string> libDirs;  // likewise
  std::vector<std::string> defines, libs, system;
};

// The options of a project file (`options`, which may name the target, so they are read before the rest).
std::optional<std::vector<std::string>> projectOptions(const std::string &path, const std::vector<KeyLine> &lines,
                                                       std::string &error);
// The project of a file's settings, for target `t`: the settings qualified with another platform left out.
std::optional<Project> makeProject(const std::string &path, const std::vector<KeyLine> &lines, const llvm::Triple &t,
                                   std::string &error);

// A module file (.xbm): what a compiled library is to the programs that use it, beside its archive.
struct ModuleFile {
  std::string path;                    // where it was read
  std::string library;                 // upper-cased name
  unsigned abi = 0;                    // XB_ABI_VERSION it was compiled for
  std::string target;                  // its triple
  std::string archive;                 // file name, beside the module file
  uint64_t stamp = 0;                  // of its sources: a program compiled with it requires the same (0: none)
  unsigned level = 1;                  // its modules' (XB_LEVEL_PROGRAM in xbrt.h): 1 + the highest of `uses`'
  std::vector<std::string> uses;       // libraries it uses (upper-cased)
  std::vector<std::string> system;     // system libraries: names, or linker arguments starting with '-'
  std::vector<std::string> declarations; // its public functions (upper-cased)
  std::vector<unsigned> declarationLines; // and the line of each (diagnostics)
  // The runtime's module file only: how its programs are linked.
  std::string linker;                  // the C compiler driver (gnu style) or link (msvc style)
  bool msvc = false;
  std::vector<std::string> linkFlags;
};

// A module file's settings (format XB_MODULE_FILE_FORMAT); its names (library, functions, libraries used) are names.
std::optional<ModuleFile> readModuleFile(const std::string &path, std::string &error);
bool writeModuleFile(const std::string &path, const ModuleFile &m, std::string &error);

} // namespace xb
