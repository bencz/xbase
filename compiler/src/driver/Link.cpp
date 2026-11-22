#include "driver/Link.h"

#include "codegen/Emit.h"
#include "common/Text.h"
#include "xbrt.h"

#include <llvm/ADT/SmallString.h>
#include <llvm/Object/Archive.h>
#include <llvm/Object/ArchiveWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>


namespace xb {

std::string archiveName(const std::string &name, const llvm::Triple &t) {
  return t.isWindowsMSVCEnvironment() ? name + ".lib" : "lib" + name + ".a";
}

std::string executableName(const std::string &path, const llvm::Triple &t) {
  if (!t.isOSWindows() || llvm::sys::path::extension(path) == ".exe") return path;
  return path + ".exe";
}

bool writeArchive(const std::string &archive, const std::string &object, const llvm::Triple &t, std::string &error) {
  auto buf = llvm::MemoryBuffer::getFile(object, /*IsText=*/false, /*RequiresNullTerminator=*/false);
  if (!buf) {
    error = "cannot read '" + object + "'";
    return false;
  }
  std::string member = llvm::sys::path::filename(archive).str();
  member = llvm::sys::path::stem(member).str() + (t.isWindowsMSVCEnvironment() ? ".obj" : ".o");
  std::vector<llvm::NewArchiveMember> members(1);
  members[0].Buf = std::move(*buf);
  members[0].MemberName = member;
  llvm::Error e = llvm::writeArchive(archive, members, llvm::SymtabWritingMode::NormalSymtab,
                                     llvm::object::Archive::getDefaultKindForTriple(t), /*Deterministic=*/true,
                                     /*Thin=*/false);
  if (e) {
    error = "cannot write '" + archive + "': " + llvm::toString(std::move(e));
    return false;
  }
  return true;
}

std::optional<ModuleFile> findModuleFile(const std::string &name, const std::vector<std::string> &dirs,
                                         const llvm::Triple &t, std::string &error) {
  std::string file = lowerCase(name);
  for (const std::string &dir : dirs)
    for (bool own : {false, true}) {
      llvm::SmallString<256> p(dir);
      if (own) llvm::sys::path::append(p, file);
      llvm::sys::path::append(p, file + ".xbm");
      if (!llvm::sys::fs::exists(p)) continue;
      std::optional<ModuleFile> m = readModuleFile(std::string(p), error);
      if (!m) return std::nullopt;
      if (m->library != upperCase(name)) {
        error = std::string(p) + ": the module file of the library " + m->library + ", not " + upperCase(name);
        return std::nullopt;
      }
      // No target: the runtime of a build for xbc's own default target.
      llvm::Triple mt(m->target.empty() ? targetTriple(TargetConfig{}) : llvm::Triple(m->target));
      if (mt.getArch() != t.getArch() || mt.getOS() != t.getOS() ||
          mt.isWindowsMSVCEnvironment() != t.isWindowsMSVCEnvironment()) {
        error = std::string(p) + ": the library " + m->library + " is compiled for " + mt.str() + ", not " + t.str();
        return std::nullopt;
      }
      if (m->abi != XB_ABI_VERSION) {
        error = std::string(p) + ": compiled for the runtime ABI " + std::to_string(m->abi) + ", this xbc's is " +
                std::to_string(XB_ABI_VERSION) + ": rebuild the library " + m->library;
        return std::nullopt;
      }
      return m;
    }
  error = "cannot find the library " + name + " (" + file + ".xbm; add its folder with -L<dir> or libdirs)";
  return std::nullopt;
}

namespace {

// A relative path a module file gives is from the module file's folder.
std::string fromModuleFile(const ModuleFile &m, const std::string &path) {
  if (llvm::sys::path::is_absolute(path)) return path;
  llvm::SmallString<256> p(llvm::sys::path::parent_path(m.path));
  llvm::sys::path::append(p, path);
  return std::string(p);
}

} // namespace

std::string archivePath(const ModuleFile &m) { return fromModuleFile(m, m.archive); }

bool linkProgram(const std::string &output, const std::vector<std::string> &objects,
                 const std::vector<ModuleFile> &libraries,
                 const ModuleFile &runtime, const std::vector<std::string> &system, std::string &error) {
  if (runtime.linker.empty()) {
    error = runtime.path + ": names no linker";
    return false;
  }
  // A name is looked up in the system's program path; a relative path is from the module file's folder (a toolchain
  // installed with the runtime).
  std::string linker = runtime.linker;
  if (llvm::sys::path::has_parent_path(linker)) linker = fromModuleFile(runtime, linker);
  else {
    auto found = llvm::sys::findProgramByName(linker);
    if (!found) {
      error = "cannot find the linker '" + linker + "' (" + runtime.path + ")";
      return false;
    }
    linker = *found;
  }
  std::vector<std::string> args{linker};
  if (runtime.msvc) {
    args.push_back("/nologo");
    args.push_back("/OUT:" + output);
  } else {
    args.insert(args.end(), runtime.linkFlags.begin(), runtime.linkFlags.end());
    args.push_back("-o");
    args.push_back(output);
  }
  args.insert(args.end(), objects.begin(), objects.end());
  for (const ModuleFile &m : libraries) args.push_back(archivePath(m));
  args.push_back(archivePath(runtime));
  for (const std::string &s : system) {
    if (s[0] == '-' || (runtime.msvc && s[0] == '/')) args.push_back(s);
    else args.push_back(runtime.msvc ? s + ".lib" : "-l" + s);
  }
  if (runtime.msvc) args.insert(args.end(), runtime.linkFlags.begin(), runtime.linkFlags.end());
  std::vector<llvm::StringRef> refs(args.begin(), args.end());
  std::string message;
  int rc = llvm::sys::ExecuteAndWait(linker, refs, std::nullopt, {}, 0, 0, &message);
  if (rc != 0) {
    error = "linking '" + output + "' failed" + (message.empty() ? "" : ": " + message);
    return false;
  }
  return true;
}

} // namespace xb
