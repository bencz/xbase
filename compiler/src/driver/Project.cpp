#include "driver/Project.h"

#include "common/Text.h"

#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <fstream>

namespace xb {

namespace {

// A relative path of a project file is from the project's folder.
std::string fromProject(const std::string &project, const std::string &path) {
  if (llvm::sys::path::is_absolute(path)) return path;
  llvm::SmallString<256> p(llvm::sys::path::parent_path(project));
  llvm::sys::path::append(p, path);
  return std::string(p);
}

std::string where(const std::string &path, const KeyLine &l) { return path + ":" + std::to_string(l.line) + ": "; }

bool isBSD(const llvm::Triple &t) { return t.isOSFreeBSD() || t.isOSNetBSD() || t.isOSOpenBSD() || t.isOSDragonFly(); }

struct Family {
  const char *name;
  bool (*of)(const llvm::Triple &);
};
const Family kFamilies[] = {
    {"WINDOWS", [](const llvm::Triple &t) { return t.isOSWindows(); }},
    {"LINUX", [](const llvm::Triple &t) { return t.isOSLinux(); }},
    {"DARWIN", [](const llvm::Triple &t) { return t.isOSDarwin(); }},
    {"BSD", isBSD},
    {"UNIX", [](const llvm::Triple &t) {
       return t.isOSLinux() || t.isOSDarwin() || isBSD(t) || t.isOSSolaris() || t.isOSAIX();
     }},
};

// The settings of a project file.
const char *const kProjectKeys[] = {"NAME", "TYPE", "SOURCES", "INCLUDES", "LIBDIRS", "DEFINES", "LIBS", "SYSTEM",
                                    "OPTIONS"};

} // namespace

std::optional<std::vector<KeyLine>> readKeyFile(const std::string &path, std::string &error) {
  auto buf = llvm::MemoryBuffer::getFile(path, /*IsText=*/true);
  if (!buf) {
    error = "cannot read '" + path + "'";
    return std::nullopt;
  }
  std::vector<KeyLine> out;
  llvm::StringRef text = (*buf)->getBuffer();
  text.consume_front("\xEF\xBB\xBF");
  unsigned number = 0;
  while (!text.empty()) {
    auto [raw, rest] = text.split('\n');
    text = rest;
    llvm::StringRef line = raw.trim();
    ++number;
    if (line.empty() || line.front() == '#') continue;
    auto [key, value] = line.split('=');
    auto [name, qualifier] = key.trim().split('.');
    KeyLine l{upperCase(name), upperCase(qualifier), value.trim().str(), number};
    bool valid = line.contains('=') && !l.key.empty() && (!key.contains('.') || !l.qualifier.empty());
    for (char c : l.key + l.qualifier) valid = valid && isIdentChar(c);
    if (!valid) {
      error = path + ":" + std::to_string(number) + ": expected 'key = value' or 'key.platform = value'";
      return std::nullopt;
    }
    out.push_back(std::move(l));
  }
  return out;
}

std::optional<std::vector<std::string>> splitWords(const std::string &value) {
  std::vector<std::string> words;
  std::string word;
  bool quoted = false, any = false;
  for (char c : value) {
    if (c == '"') {
      quoted = !quoted;
      any = true;
    } else if (!quoted && std::isspace((unsigned char)c)) {
      if (any) words.push_back(word);
      word.clear();
      any = false;
    } else {
      word += c;
      any = true;
    }
  }
  if (quoted) return std::nullopt;
  if (any) words.push_back(word);
  return words;
}

std::vector<std::string> platformFamilies(const llvm::Triple &t) {
  std::vector<std::string> f;
  for (const Family &family : kFamilies)
    if (family.of(t)) f.push_back(family.name);
  return f;
}

std::optional<std::vector<std::string>> projectOptions(const std::string &path, const std::vector<KeyLine> &lines,
                                                       std::string &error) {
  std::vector<std::string> out;
  for (const KeyLine &l : lines) {
    if (l.key != "OPTIONS") continue;
    if (!l.qualifier.empty()) {
      error = where(path, l) + "options apply to every platform (they may name the target)";
      return std::nullopt;
    }
    std::optional<std::vector<std::string>> words = splitWords(l.value);
    if (!words) {
      error = where(path, l) + "a quote is left open";
      return std::nullopt;
    }
    out.insert(out.end(), words->begin(), words->end());
  }
  return out;
}

std::optional<Project> makeProject(const std::string &path, const std::vector<KeyLine> &lines, const llvm::Triple &t,
                                   std::string &error) {
  Project p;
  p.name = llvm::sys::path::stem(path).str();
  std::vector<std::string> families = platformFamilies(t);
  for (const KeyLine &l : lines) {
    if (std::find(std::begin(kProjectKeys), std::end(kProjectKeys), l.key) == std::end(kProjectKeys)) {
      error = where(path, l) + "unknown setting '" + l.key + "'";
      return std::nullopt;
    }
    if (l.key == "OPTIONS") continue; // projectOptions
    if (!l.qualifier.empty()) {
      bool known = false;
      std::string names;
      for (const Family &family : kFamilies) {
        known = known || l.qualifier == family.name;
        names += (names.empty() ? "" : ", ") + lowerCase(family.name);
      }
      if (!known) {
        error = where(path, l) + "unknown platform '" + l.qualifier + "' (" + names + ")";
        return std::nullopt;
      }
      if (std::find(families.begin(), families.end(), l.qualifier) == families.end()) continue; // another platform's
    }
    std::optional<std::vector<std::string>> words = splitWords(l.value);
    if (!words) {
      error = where(path, l) + "a quote is left open";
      return std::nullopt;
    }
    auto paths = [&](std::vector<std::string> &to) {
      for (const std::string &w : *words) to.push_back(fromProject(path, w));
    };
    auto append = [&](std::vector<std::string> &to) { to.insert(to.end(), words->begin(), words->end()); };
    if (l.key == "NAME") {
      // A file name in the output folder: no folders, nothing a file's name cannot hold.
      std::string n = words->size() == 1 ? (*words)[0] : std::string();
      if (n.empty() || n == "." || n == ".." || n.find_first_of("/\\:*?\"<>|") != std::string::npos) {
        error = where(path, l) + "name is one file name (no folder)";
        return std::nullopt;
      }
      p.name = n;
    } else if (l.key == "TYPE") {
      std::string type = upperCase(l.value);
      if (type != "PROGRAM" && type != "LIBRARY") {
        error = where(path, l) + "type is program or library";
        return std::nullopt;
      }
      p.library = type == "LIBRARY";
    } else if (l.key == "SOURCES") paths(p.sources);
    else if (l.key == "INCLUDES") paths(p.includes);
    else if (l.key == "LIBDIRS") paths(p.libDirs);
    else if (l.key == "DEFINES") append(p.defines);
    else if (l.key == "LIBS") append(p.libs);
    else append(p.system);
  }
  if (p.sources.empty()) {
    error = path + ": no sources";
    return std::nullopt;
  }
  return p;
}

std::optional<ModuleFile> readModuleFile(const std::string &path, std::string &error) {
  auto lines = readKeyFile(path, error);
  if (!lines) return std::nullopt;
  ModuleFile m;
  m.path = path;
  bool format = false;
  for (const KeyLine &l : *lines) {
    auto number = [&](uint64_t &to, int base) {
      const char *b = l.value.data(), *e = b + l.value.size();
      auto [at, ec] = std::from_chars(b, e, to, base);
      return ec == std::errc() && at == e && b != e;
    };
    std::optional<std::vector<std::string>> words = splitWords(l.value);
    auto names = [&](std::vector<std::string> &to) {
      for (const std::string &w : *words) {
        if (!isIdentifier(w)) return false;
        to.push_back(upperCase(w));
      }
      return true;
    };
    uint64_t n = 0;
    bool ok = l.qualifier.empty() && words, known = true;
    if (!ok) {
      // a platform or an open quote: invalid, whatever the key
    } else if (l.key == "FORMAT") {
      if (!number(n, 10) || n != XB_MODULE_FILE_FORMAT) {
        error = where(path, l) + "a module file of another format: rebuild the library with this xbc";
        return std::nullopt;
      }
      format = true;
    } else if (l.key == "LIBRARY") {
      ok = isIdentifier(l.value);
      m.library = upperCase(l.value);
    } else if (l.key == "ABI") {
      ok = number(n, 10) && n <= UINT32_MAX;
      m.abi = (unsigned)n;
    } else if (l.key == "LEVEL") {
      ok = number(n, 10) && n >= 1 && n < XB_LEVEL_PROGRAM;
      m.level = (unsigned)n;
    } else if (l.key == "TARGET") m.target = l.value;
    else if (l.key == "ARCHIVE") ok = !(m.archive = l.value).empty();
    else if (l.key == "STAMP") ok = number(m.stamp, 16);
    else if (l.key == "USES") ok = names(m.uses);
    else if (l.key == "FUNCTION") {
      ok = names(m.declarations);
      m.declarationLines.resize(m.declarations.size(), l.line);
    }
    else if (l.key == "SYSTEM") m.system.insert(m.system.end(), words->begin(), words->end());
    else if (l.key == "LINKER") m.linker = l.value;
    else if (l.key == "STYLE") {
      ok = l.value == "gnu" || l.value == "msvc";
      m.msvc = l.value == "msvc";
    } else if (l.key == "FLAGS") m.linkFlags.insert(m.linkFlags.end(), words->begin(), words->end());
    else known = false;
    if (!known) {
      error = where(path, l) + "unknown setting '" + l.key + "'";
      return std::nullopt;
    }
    if (!ok) {
      error = where(path, l) + "invalid " + lowerCase(l.key) + (l.qualifier.empty() ? "" : " (no platform here)");
      return std::nullopt;
    }
  }
  if (!format || m.library.empty() || m.archive.empty()) {
    error = path + ": not a module file (format, library and archive are required)";
    return std::nullopt;
  }
  return m;
}

bool writeModuleFile(const std::string &path, const ModuleFile &m, std::string &error) {
  std::ofstream f(path, std::ios::binary);
  char stamp[17];
  std::snprintf(stamp, sizeof stamp, "%016llx", static_cast<unsigned long long>(m.stamp));
  f << "# The module file of the xbase library " << m.library << ", written by xbc (docs/projects.md).\n"
    << "format = " << XB_MODULE_FILE_FORMAT << "\nlibrary = " << m.library << "\nabi = " << m.abi
    << "\nlevel = " << m.level << "\ntarget = " << m.target << "\narchive = " << m.archive << "\nstamp = " << stamp
    << "\n";
  auto list = [&](const char *key, const std::vector<std::string> &words) {
    if (words.empty()) return;
    f << key << " =";
    for (const std::string &w : words) f << (w.find_first_of(" \t") == std::string::npos ? " " + w : " \"" + w + "\"");
    f << "\n";
  };
  list("uses", m.uses);
  list("system", m.system);
  for (const std::string &d : m.declarations) f << "function = " << d << "\n";
  if (!f) error = "cannot write '" + path + "'";
  return (bool)f;
}

} // namespace xb
