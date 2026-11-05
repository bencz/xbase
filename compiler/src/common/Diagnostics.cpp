#include "common/Diagnostics.h"

#include <cstdio>

namespace xb {

void Diagnostics::report(const char *kind, SourceLoc loc, const std::string &msg) {
  const std::string &f = fileName(loc.file);
  if (!f.empty() && loc.line)
    std::fprintf(stderr, "%s:%u:%u: %s: %s\n", f.c_str(), loc.line, loc.col, kind, msg.c_str());
  else if (!f.empty())
    std::fprintf(stderr, "%s: %s: %s\n", f.c_str(), kind, msg.c_str());
  else
    std::fprintf(stderr, "xbc: %s: %s\n", kind, msg.c_str());
}

void Diagnostics::error(SourceLoc loc, const std::string &msg) {
  if (limitReached()) return;
  ++errors_;
  report("error", loc, msg);
  if (limitReached()) std::fprintf(stderr, "xbc: too many errors, stopping\n");
}

void Diagnostics::warning(SourceLoc loc, const std::string &msg) {
  if (!limitReached()) report("warning", loc, msg);
}

void Diagnostics::note(SourceLoc loc, const std::string &msg) {
  if (!limitReached()) report("note", loc, msg);
}

} // namespace xb
