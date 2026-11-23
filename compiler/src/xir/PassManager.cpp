// The xIR pass pipeline: a table of passes run in order, with the verifier
// after each one.
#include "xir/PassManager.h"

#include "xir/Types.h"
#include "xir/Verifier.h"
#include "xir/passes/Passes.h"

#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace xb::xir {

namespace {

struct Pass {
  const char *name;
  bool (*run)(Module &m, Function &f);
};

// A pass that needs only the function.
template <bool (*P)(Function &)> bool onFunction(Module &, Function &f) { return P(f); }

constexpr Pass kPipeline[] = {
    {"inline-functions", inlineFunctions},             // first: the variables they free promote-variables takes
    {"fuse-generators", onFunction<fuseGenerators>},   // after inline-functions, which shows the enumerator made
    {"inline-blocks", onFunction<inlineBlocks>},       // after inline-functions, which may pass a block to an Eval
    {"append-in-place", onFunction<appendInPlace>},    // before promote-variables, which takes the other LOCALs
    {"promote-variables", onFunction<promoteVariables>},
    {"fold-constants", onFunction<foldConstants>},
    {"resolve-sends", resolveSends},                   // after promote-variables: the objects' classes flow
    {"resolve-scalar-sends", resolveScalarSends},     // after promote-variables: the values' types flow
    {"speculate-sends", speculateSends},               // after the two above: what they leave unresolved
    {"simplify-cfg", onFunction<simplifyCFG>},
    {"remove-dead-code", onFunction<removeDeadCode>},
    {"unshare-variables", onFunction<unshareVariables>}, // after remove-dead-code: captures gone with dead code
};

// Each round can expose work for the others (a folded branch leaves a block unreachable, a merged block a constant
// operand, a resolved send a call to inline, an inlined body a generator to fuse); the pipeline repeats while something
// changes, at most this many times (a chain of Enumerable's methods takes one round a level).
constexpr unsigned kMaxRounds = 8;

// Once the pipeline is done on every function, on the code it settled (a body
// inlined elsewhere is copied before): typed continuations copy the function,
// and the cleanup passes tidy what the copy joined.
constexpr Pass kFinal[] = {
    {"typed-continuations", onFunction<typedContinuations>},
    {"split-appends", onFunction<splitAppends>},  // after the checks, which tell the pieces' types
    {"peel-for-types", onFunction<peelForTypes>}, // after the checks, which narrow what the loops carry
    {"version-each", onFunction<versionEachLoops>},
    {"simplify-cfg", onFunction<simplifyCFG>},
    {"remove-dead-code", onFunction<removeDeadCode>},
};

// The module's functions, each after the functions and codeblocks it calls, makes or forwards to (post-order of the
// call graph; a cycle is entered once): an inlined body is then the callee's optimized one, its own calls inlined
// already, and the inliner's size limit holds for what it copies.
std::vector<Function *> calleesFirst(const Module &m) {
  std::vector<Function *> order;
  std::unordered_set<const Function *> seen;
  std::vector<std::pair<Function *, std::vector<Function *>>> stack;
  auto targets = [](const Function &f) {
    std::vector<Function *> out;
    for (auto &b : f.blocks)
      for (auto &i : b->insts) {
        if (i->callee.function) out.push_back(i->callee.function);
        if (i->op == Op::MakeBlock && i->blockFn) out.push_back(i->blockFn);
      }
    return out;
  };
  // The classes' methods first: a send resolved to one of them then calls (or inlines) it optimized.
  std::vector<Function *> roots;
  for (const Module::Class &c : m.classes)
    for (const Module::Class::Method &mt : c.methods)
      if (mt.fn) roots.push_back(mt.fn);
  for (auto &f : m.functions) roots.push_back(f.get());
  for (Function *root : roots) {
    if (!seen.insert(root).second) continue;
    stack.push_back({root, targets(*root)});
    while (!stack.empty()) {
      auto &[f, next] = stack.back();
      if (next.empty()) {
        order.push_back(f);
        stack.pop_back();
        continue;
      }
      Function *g = next.back();
      next.pop_back();
      if (seen.insert(g).second) stack.push_back({g, targets(*g)});
    }
  }
  return order;
}

} // namespace

std::string optimize(Module &m, bool final) {
  std::string bad;
  // Runs a pass on a function; true when it changed it (`bad` reports invalid xIR).
  auto run = [&](const Pass &p, Function &f) {
    if (!p.run(m, f)) return false;
    if (std::string report = verify(m, f); !report.empty() && bad.empty())
      bad = std::string("after ") + p.name + ": " + report;
    return true;
  };
  if (dropReferenceBodies(m))
    for (auto &f : m.functions)
      if (std::string report = verify(m, *f); !report.empty() && bad.empty())
        bad = "after drop-reference-bodies: " + report;
  if (foldConstantStatics(m))
    for (auto &f : m.functions)
      if (std::string report = verify(m, *f); !report.empty() && bad.empty())
        bad = "after fold-constant-statics: " + report;
  auto rounds = [&](Function &f) {
    for (unsigned round = 0; round < kMaxRounds && bad.empty(); ++round) {
      bool changed = false;
      for (const Pass &p : kPipeline)
        if (bad.empty()) changed |= run(p, f);
      if (!changed) break;
    }
  };
  // Calls whose arguments have types a typed copy takes go to it (made and optimized at once, before the caller uses
  // what it returns); the caller's pipeline runs again on what its callees now tell of their results.
  TypedCopies copies;
  std::function<void(Function &)> optimizeCopy = [&](Function &c) {
    if (std::string report = verify(m, c); !report.empty() && bad.empty())
      bad = "after specialize-calls: " + report;
    rounds(c);
    if (bad.empty() && specializeCalls(m, c, copies, optimizeCopy)) rounds(c);
    settleReturnTypes(c);
  };
  for (Function *f : calleesFirst(m)) {
    rounds(*f);
    if (bad.empty() && specializeCalls(m, *f, copies, optimizeCopy)) rounds(*f);
    // What its callers, optimized after it, know of its result (a function of a cycle entered before it is not
    // narrowed yet, so it gives every type there).
    f->returnTypes = returnTypes(*f);
  }
  if (bad.empty() && refineCopies(m, copies))
    for (auto &f : m.functions)
      if (std::string report = verify(m, *f); !report.empty() && bad.empty())
        bad = "after refining the typed copies: " + report;
  if (bad.empty() && dropCopiedFunctions(m, copies))
    for (auto &f : m.functions)
      if (std::string report = verify(m, *f); !report.empty() && bad.empty())
        bad = "after dropping copied functions: " + report;
  if (final) // on the functions inlining copied in too (optimized where they come from)
    for (auto &f : m.functions)
      for (const Pass &p : kFinal)
        if (bad.empty()) run(p, *f);
  return bad;
}

} // namespace xb::xir
