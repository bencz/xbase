#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace xb {

struct SourceLoc {
  uint32_t file = 0;   // index into Diagnostics' file table (0 = none)
  uint32_t line = 0;
  uint32_t col = 0;
};

class Diagnostics {
public:
  Diagnostics() { files_.emplace_back(); }

  uint32_t addFile(const std::string &name) {
    files_.push_back(name);
    return (uint32_t)files_.size() - 1;
  }
  const std::string &fileName(uint32_t id) const { return files_[id < files_.size() ? id : 0]; }

  void error(SourceLoc loc, const std::string &msg);
  void warning(SourceLoc loc, const std::string &msg);
  void note(SourceLoc loc, const std::string &msg);

  unsigned errorCount() const { return errors_; }
  // After kMaxErrors errors nothing more is reported; stages stop early.
  bool limitReached() const { return errors_ >= kMaxErrors; }
  static constexpr unsigned kMaxErrors = 100;

private:
  void report(const char *kind, SourceLoc loc, const std::string &msg);
  std::vector<std::string> files_;
  unsigned errors_ = 0;
};

} // namespace xb
