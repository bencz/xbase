// Loops whose cold paths leave for a generic copy (see LoopSplit.h).
#include "codegen/LoopSplit.h"

#include <llvm/Analysis/BlockFrequencyInfo.h>
#include <llvm/Analysis/BranchProbabilityInfo.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/ProfDataUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/LoopSimplify.h>
#include <llvm/Transforms/Utils/LoopUtils.h>
#include <llvm/Transforms/Utils/SSAUpdater.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

#include <unordered_set>
#include <vector>

using namespace llvm;

namespace xb {

namespace {

// An edge is cold when its branch weight is below 1/64 of the branch's
// total: the lowering weights its slow paths as unlikely.
bool coldEdge(const Instruction &term, unsigned succ) {
  SmallVector<uint32_t, 4> weights;
  if (!extractBranchWeights(term, weights) || weights.size() != term.getNumSuccessors()) return false;
  uint64_t total = 0;
  for (uint32_t w : weights) total += w;
  return (uint64_t)weights[succ] * 64 < total;
}

struct ColdEdge {
  BasicBlock *from;
  unsigned succ;
};

// The cold edges between blocks of the loop (a branch whose two successors
// are one block is left alone).
std::vector<ColdEdge> coldEdges(const Loop &loop) {
  std::vector<ColdEdge> out;
  for (BasicBlock *b : loop.blocks()) {
    Instruction *t = b->getTerminator();
    if (t->getNumSuccessors() == 2 && t->getSuccessor(0) == t->getSuccessor(1)) continue;
    for (unsigned k = 0; k < t->getNumSuccessors(); ++k)
      if (loop.contains(t->getSuccessor(k)) && coldEdge(*t, k)) out.push_back({b, k});
  }
  return out;
}

// The loop (in simplified and LCSSA form) is cloned; the cold edges of the
// original go to the copy, whose values SSAUpdater joins with the original's
// where the edges enter it.
void split(Function &f, Loop &loop, const std::vector<ColdEdge> &cold) {
  std::vector<BasicBlock *> blocks(loop.block_begin(), loop.block_end());
  SmallVector<BasicBlock *, 8> exits;
  loop.getUniqueExitBlocks(exits);
  ValueToValueMapTy vmap;
  std::vector<BasicBlock *> copies;
  for (BasicBlock *b : blocks) {
    BasicBlock *c = CloneBasicBlock(b, vmap, ".generic", &f);
    vmap[b] = c;
    copies.push_back(c);
  }
  remapInstructionsInBlocks(copies, vmap);
  std::unordered_set<const BasicBlock *> inCopy(copies.begin(), copies.end());

  // The copy is entered only through the redirected edges: its PHIs drop what
  // came from outside the loop.
  for (BasicBlock *c : copies)
    for (PHINode &phi : c->phis())
      for (unsigned k = phi.getNumIncomingValues(); k-- > 0;)
        if (!inCopy.count(phi.getIncomingBlock(k))) phi.removeIncomingValue(k, false);

  // The exits receive the copy's values too.
  for (BasicBlock *e : exits)
    for (PHINode &phi : e->phis()) {
      unsigned n = phi.getNumIncomingValues();
      for (unsigned k = 0; k < n; ++k) {
        BasicBlock *from = phi.getIncomingBlock(k);
        if (!loop.contains(from)) continue;
        Value *v = phi.getIncomingValue(k);
        auto it = vmap.find(v);
        phi.addIncoming(it != vmap.end() ? static_cast<Value *>(it->second) : v, cast<BasicBlock>(vmap[from]));
      }
    }

  for (const ColdEdge &e : cold) {
    Instruction *t = e.from->getTerminator();
    BasicBlock *s = t->getSuccessor(e.succ);
    for (PHINode &phi : s->phis())
      cast<PHINode>(vmap[&phi])->addIncoming(phi.getIncomingValueForBlock(e.from), e.from);
    s->removePredecessor(e.from, /*KeepOneInputPHIs=*/true);
    t->setSuccessor(e.succ, cast<BasicBlock>(vmap[s]));
  }

  // A value of the copy used where the original's may also arrive (through a
  // redirected edge) becomes the join of both; in the copy's own block the
  // copy dominates its uses. The pairs are taken first: the updater adds PHIs.
  std::vector<std::pair<Instruction *, Instruction *>> pairs;
  for (BasicBlock *b : blocks)
    for (Instruction &i : *b)
      if (!i.getType()->isVoidTy()) pairs.push_back({&i, cast<Instruction>(vmap[&i])});
  for (auto [orig, c] : pairs) {
    if (c->use_empty()) continue;
    SSAUpdater up;
    up.Initialize(orig->getType(), orig->getName());
    up.AddAvailableValue(orig->getParent(), orig);
    up.AddAvailableValue(c->getParent(), c);
    SmallVector<Use *, 8> uses;
    for (Use &u : c->uses()) uses.push_back(&u);
    for (Use *u : uses) {
      auto *user = cast<Instruction>(u->getUser());
      auto *phi = dyn_cast<PHINode>(user);
      BasicBlock *at = phi ? phi->getIncomingBlock(*u) : user->getParent();
      if (at != c->getParent()) up.RewriteUse(*u);
    }
  }
}

} // namespace

// Each cold edge is an entry into the copy, where every value live across it
// needs a join: a loop of more instructions or with more cold edges is left as
// it is, and the copies grow a function by at most its own size.
constexpr uint64_t kMaxLoopInstructions = 2000;
constexpr size_t kMaxColdEdges = 64;

bool splitColdLoopPaths(Function &f) {
  DominatorTree dt(f);
  LoopInfo li(dt);
  // A loop that itself runs on a cold path (its header less frequent than
  // 1/64 of the function's entry: the generic copy of typed continuations,
  // error handling) is not worth its copy.
  BranchProbabilityInfo bpi(f, li);
  BlockFrequencyInfo bfi(f, bpi, li);
  uint64_t entryFreq = bfi.getEntryFreq().getFrequency();
  std::vector<Loop *> loops;
  for (Loop *l : li.getLoopsInPreorder())
    if (l->isInnermost() && l->isSafeToClone() && !coldEdges(*l).empty() &&
        bfi.getBlockFreq(l->getHeader()).getFrequency() * 64 >= entryFreq)
      loops.push_back(l);
  if (loops.empty()) return false;
  // Every candidate is put in shape while the analyses still describe the
  // function; the splits below change it but leave the other candidates' own
  // blocks (and their Loop objects) as they are.
  for (Loop *l : loops) {
    simplifyLoop(l, &dt, &li, nullptr, nullptr, nullptr, /*PreserveLCSSA=*/false);
    formLCSSA(*l, dt, &li, nullptr);
  }
  uint64_t budget = f.getInstructionCount(), grown = 0;
  bool changed = false;
  for (Loop *l : loops) {
    uint64_t size = 0;
    for (BasicBlock *b : l->blocks()) size += b->size();
    std::vector<ColdEdge> cold = coldEdges(*l);
    if (cold.empty() || cold.size() > kMaxColdEdges || size > kMaxLoopInstructions || grown + size > budget) continue;
    split(f, *l, cold);
    grown += size;
    changed = true;
  }
  return changed;
}

} // namespace xb
