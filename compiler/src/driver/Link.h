// A library's archive and module file, and the link step of a program: docs/projects.md.
#pragma once

#include "driver/Project.h"

#include <llvm/TargetParser/Triple.h>

#include <optional>
#include <string>
#include <vector>

namespace xb {

// The file names of the target's conventions: lib<name>.a (<name>.lib with the MSVC tools), <name>.exe on Windows.
std::string archiveName(const std::string &name, const llvm::Triple &t);
std::string executableName(const std::string &path, const llvm::Triple &t);

// Writes `archive`, whose one member is the object file `object`, in the target's archive format.
bool writeArchive(const std::string &archive, const std::string &object, const llvm::Triple &t, std::string &error);

// The module file of library `name` (any letter case) in `dirs`, in order: <dir>/<name>.xbm, then
// <dir>/<name>/<name>.xbm; its ABI and target checked against this compiler's and `t`.
std::optional<ModuleFile> findModuleFile(const std::string &name, const std::vector<std::string> &dirs,
                                         const llvm::Triple &t, std::string &error);

// The archive a module file names (a path from the module file's folder).
std::string archivePath(const ModuleFile &m);

// The program `output`: the objects, the libraries' archives (each before those it uses), the runtime's, then the
// system libraries they all name, through the linker the runtime's module file names.
bool linkProgram(const std::string &output, const std::vector<std::string> &objects,
                 const std::vector<ModuleFile> &libraries, const ModuleFile &runtime,
                 const std::vector<std::string> &system, std::string &error);

} // namespace xb
