#pragma once

#include <string>
#include <vector>

namespace xb {

// A source of the runtime written in xBase that the compiler carries (compiler/embed_sources.cmake): calls of its
// public functions (`functions`, upper-cased) may run their bodies in place (docs/xir.md, Inlining).
struct EmbeddedSource {
  const char *name;
  std::string text;
  std::vector<std::string> functions;
};

const std::vector<EmbeddedSource> &runtimeLibrarySources();

} // namespace xb
