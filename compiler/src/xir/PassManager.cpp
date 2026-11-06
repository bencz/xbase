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
    {"append-in-place", appendInPlace}, // before promote-variables, which takes the other LOCALs
    {"promote-variables", promoteVariables},
    {"fold-constants", foldConstants},
    {"simplify-cfg", simplifyCFG},
    {"remove-dead-code", removeDeadCode},
};

// Each round can expose work for the others (a folded branch leaves a block
// unreachable, a merged block a constant operand); the pipeline repeats while
// something changes, at most this many times.
constexpr unsigned kMaxRounds = 4;

// Once the pipeline is done, on the code it settled: typed continuations copy
// the function, and the cleanup passes tidy what the copy joined.
constexpr Pass kFinal[] = {
    {"typed-continuations", typedContinuations},
    {"simplify-cfg", simplifyCFG},
    {"remove-dead-code", removeDeadCode},
};

} // namespace

std::string optimize(Module &m) {
  for (auto &f : m.functions) {
    std::string bad;
    // Runs a pass; true when it changed the function (`bad` reports invalid xIR).
    auto run = [&](const Pass &p) {
      if (!p.run(*f)) return false;
      if (std::string report = verify(m, *f); !report.empty() && bad.empty())
        bad = std::string("after ") + p.name + ": " + report;
      return true;
    };
    for (unsigned round = 0; round < kMaxRounds && bad.empty(); ++round) {
      bool changed = false;
      for (const Pass &p : kPipeline)
        if (bad.empty()) changed |= run(p);
      if (!changed) break;
    }
    for (const Pass &p : kFinal)
      if (bad.empty()) run(p);
    if (!bad.empty()) return bad;
  }
  return {};
}

} // namespace xb::xir
