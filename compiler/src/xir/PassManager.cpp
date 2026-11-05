// The xIR pass pipeline: a table of passes run in order, with the verifier
// after each one.
#include "xir/PassManager.h"

#include "xir/Verifier.h"
#include "xir/passes/Passes.h"

namespace xb::xir {

namespace {

struct Pass {
  const char *name;
  bool (*run)(Function &f);
};

constexpr Pass kPipeline[] = {
    {"fold-constants", foldConstants},
    {"simplify-cfg", simplifyCFG},
    {"remove-dead-code", removeDeadCode},
};

// Each round can expose work for the others (a folded branch leaves a block
// unreachable, a merged block a constant operand); the pipeline repeats while
// something changes, at most this many times.
constexpr unsigned kMaxRounds = 4;

} // namespace

std::string optimize(Module &m) {
  for (auto &f : m.functions) {
    for (unsigned round = 0; round < kMaxRounds; ++round) {
      bool changed = false;
      for (const Pass &p : kPipeline) {
        if (!p.run(*f)) continue;
        changed = true;
        if (std::string bad = verify(m, *f); !bad.empty()) return std::string("after ") + p.name + ": " + bad;
      }
      if (!changed) break;
    }
  }
  return {};
}

} // namespace xb::xir
