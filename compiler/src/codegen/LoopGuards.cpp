// Tests a loop makes on values it does not change, out of the loop (see LoopGuards.h).
#include "codegen/LoopGuards.h"

#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Transforms/Utils/LoopUtils.h>
#include <llvm/Transforms/Utils/SSAUpdater.h>

#include <vector>

using namespace llvm;

namespace xb {

namespace {

// What a value of the header is on entry, from the preheader: a PHI's incoming value, or the value itself when the
// loop does not compute it. Null for anything else.
Value *onEntry(Loop &loop, BasicBlock *pre, Value *v) {
  if (auto *phi = dyn_cast<PHINode>(v); phi && phi->getParent() == loop.getHeader())
    return phi->getIncomingValueForBlock(pre);
  return loop.isLoopInvariant(v) ? v : nullptr;
}

struct Guard {
  Loop *loop;
  BranchInst *br;
  unsigned leave; // the successor outside the loop
};

// The header's branch, when the header has nothing but PHIs before it, its condition does not change in the loop, and
// one side leaves the loop.
bool guardShape(Loop &loop, Guard &g) {
  BasicBlock *header = loop.getHeader();
  auto *br = dyn_cast<BranchInst>(header->getTerminator());
  if (!br || !br->isConditional() || br->getSuccessor(0) == br->getSuccessor(1) ||
      !loop.isLoopInvariant(br->getCondition()) || &*header->getFirstNonPHIIt() != br)
    return false;
  bool in0 = loop.contains(br->getSuccessor(0)), in1 = loop.contains(br->getSuccessor(1));
  if (in0 == in1) return false;
  g = {&loop, br, in0 ? 1u : 0u};
  return true;
}

// The exit receives from the header only values known on entry.
bool exitKnownOnEntry(const Guard &g, BasicBlock *pre) {
  for (PHINode &phi : g.br->getSuccessor(g.leave)->phis())
    if (!onEntry(*g.loop, pre, phi.getIncomingValueForBlock(g.loop->getHeader()))) return false;
  return true;
}

void hoist(const Guard &g) {
  Loop &loop = *g.loop;
  BasicBlock *header = loop.getHeader(), *pre = loop.getLoopPreheader();
  BasicBlock *exit = g.br->getSuccessor(g.leave), *stay = g.br->getSuccessor(1 - g.leave);
  // The exit, entered from the preheader instead, with the entry values.
  for (PHINode &phi : exit->phis()) phi.addIncoming(onEntry(loop, pre, phi.getIncomingValueForBlock(header)), pre);
  exit->removePredecessor(header, /*KeepOneInputPHIs=*/true);
  BranchInst *test = BranchInst::Create(g.leave == 0 ? exit : header, g.leave == 0 ? header : exit,
                                        g.br->getCondition(), pre->getTerminator()->getIterator());
  test->copyMetadata(*g.br);
  pre->getTerminator()->eraseFromParent();
  BranchInst::Create(stay, g.br->getIterator());
  g.br->eraseFromParent();
  // Uses of the header's PHIs outside the loop, past the exit: the entry value where the preheader leads.
  for (PHINode &phi : header->phis()) {
    std::vector<Use *> outside;
    for (Use &u : phi.uses()) {
      auto *user = cast<Instruction>(u.getUser());
      auto *userPhi = dyn_cast<PHINode>(user);
      if (!loop.contains(userPhi ? userPhi->getIncomingBlock(u) : user->getParent())) outside.push_back(&u);
    }
    if (outside.empty()) continue;
    SSAUpdater up;
    up.Initialize(phi.getType(), phi.getName());
    up.AddAvailableValue(header, &phi);
    up.AddAvailableValue(pre, phi.getIncomingValueForBlock(pre));
    for (Use *u : outside) up.RewriteUse(*u);
  }
}

} // namespace

bool hoistLoopGuards(Function &f, FunctionAnalysisManager &fam) {
  auto &dt = fam.getResult<DominatorTreeAnalysis>(f);
  auto &li = fam.getResult<LoopAnalysis>(f);
  bool changed = false;
  std::vector<Guard> guards;
  for (Loop *loop : li.getLoopsInPreorder()) {
    Guard g;
    if (!guardShape(*loop, g)) continue;
    BasicBlock *pre = loop->getLoopPreheader();
    if (!pre) { // made only for a loop that has a guard
      pre = InsertPreheaderForLoop(loop, &dt, &li, nullptr, false);
      if (!pre) continue;
      changed = true;
    }
    if (exitKnownOnEntry(g, pre)) guards.push_back(g);
  }
  for (const Guard &g : guards) hoist(g);
  return changed || !guards.empty();
}

} // namespace xb
