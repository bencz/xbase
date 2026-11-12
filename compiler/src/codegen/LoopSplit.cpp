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

// A clone of the loop's blocks, entered only from the other copies: its PHIs drop what came from outside the loop.
void cloneLoop(Function &f, const std::vector<BasicBlock *> &blocks, ValueToValueMapTy &vmap, const char *suffix) {
  std::vector<BasicBlock *> copies;
  for (BasicBlock *b : blocks) {
    BasicBlock *c = CloneBasicBlock(b, vmap, suffix, &f);
    vmap[b] = c;
    copies.push_back(c);
  }
  remapInstructionsInBlocks(copies, vmap);
  std::unordered_set<const BasicBlock *> cloned(copies.begin(), copies.end());
  for (BasicBlock *c : copies)
    for (PHINode &phi : c->phis())
      for (unsigned k = phi.getNumIncomingValues(); k-- > 0;)
        if (!cloned.count(phi.getIncomingBlock(k))) phi.removeIncomingValue(k, false);
}

// What a value of the loop is in a copy (a value from outside the loop is itself).
Value *inCopy(ValueToValueMapTy &vmap, Value *v) {
  auto it = vmap.find(v);
  return it != vmap.end() ? static_cast<Value *>(it->second) : v;
}

// The loop (in simplified and LCSSA form) is cloned twice: a generic copy, where the cold edges of the original go,
// and the loop again, where the generic copy's back edge goes, whose cold edges go to the generic copy as well. The
// original keeps what it knows of the values that enter it; the loop again, entered from the generic copy, runs fast
// paths as well after a slow one. SSAUpdater joins the values of the copies where several may arrive.
void split(Function &f, Loop &loop, const std::vector<ColdEdge> &cold) {
  std::vector<BasicBlock *> blocks(loop.block_begin(), loop.block_end());
  SmallVector<BasicBlock *, 8> exits;
  loop.getUniqueExitBlocks(exits);
  ValueToValueMapTy gen, again;
  cloneLoop(f, blocks, gen, ".generic");
  cloneLoop(f, blocks, again, ".again");

  // The exits receive the copies' values too.
  for (BasicBlock *e : exits)
    for (PHINode &phi : e->phis()) {
      unsigned n = phi.getNumIncomingValues();
      for (unsigned k = 0; k < n; ++k) {
        BasicBlock *from = phi.getIncomingBlock(k);
        if (!loop.contains(from)) continue;
        Value *v = phi.getIncomingValue(k);
        phi.addIncoming(inCopy(gen, v), cast<BasicBlock>(gen[from]));
        phi.addIncoming(inCopy(again, v), cast<BasicBlock>(again[from]));
      }
    }

  // What the back edge brings to the header, taken before a cold edge that is the back edge itself removes it.
  BasicBlock *header = loop.getHeader(), *latch = loop.getLoopLatch();
  std::vector<std::pair<PHINode *, Value *>> againEntries;
  for (PHINode &phi : header->phis())
    againEntries.push_back({cast<PHINode>(again[&phi]), inCopy(gen, phi.getIncomingValueForBlock(latch))});

  for (const ColdEdge &e : cold) {
    BasicBlock *s = e.from->getTerminator()->getSuccessor(e.succ);
    auto *genS = cast<BasicBlock>(gen[s]);
    auto *againFrom = cast<BasicBlock>(again[e.from]);
    auto *againS = cast<BasicBlock>(again[s]);
    for (PHINode &phi : s->phis()) {
      Value *v = phi.getIncomingValueForBlock(e.from);
      auto *genPhi = cast<PHINode>(gen[&phi]);
      genPhi->addIncoming(v, e.from);
      genPhi->addIncoming(inCopy(again, v), againFrom);
    }
    s->removePredecessor(e.from, /*KeepOneInputPHIs=*/true);
    againS->removePredecessor(againFrom, /*KeepOneInputPHIs=*/true);
    e.from->getTerminator()->setSuccessor(e.succ, genS);
    againFrom->getTerminator()->setSuccessor(e.succ, genS);
  }

  // The generic copy's back edge enters the loop again, through a preheader of its own (where what is hoisted out of
  // that loop is computed again, after the calls of the generic iteration).
  auto *genHeader = cast<BasicBlock>(gen[header]), *genLatch = cast<BasicBlock>(gen[latch]);
  auto *againHeader = cast<BasicBlock>(again[header]);
  BasicBlock *reenter = BasicBlock::Create(f.getContext(), header->getName() + ".reenter", &f, againHeader);
  BranchInst::Create(againHeader, reenter);
  for (auto [phi, v] : againEntries) phi->addIncoming(v, reenter);
  genHeader->removePredecessor(genLatch, /*KeepOneInputPHIs=*/true);
  genLatch->getTerminator()->replaceSuccessorWith(genHeader, reenter);

  // A value of the generic copy used where the original's or the loop again's may arrive instead (through a
  // redirected edge) becomes their join; in the copy's own block the copy dominates its uses. The loop again is
  // entered at its header only, where its values dominate their uses as in the original. The pairs are taken first:
  // the updater adds PHIs.
  struct Defs {
    Instruction *orig, *gen, *again;
  };
  std::vector<Defs> defs;
  for (BasicBlock *b : blocks)
    for (Instruction &i : *b)
      if (!i.getType()->isVoidTy())
        defs.push_back({&i, cast<Instruction>(gen[&i]), cast<Instruction>(again[&i])});
  for (const Defs &d : defs) {
    if (d.gen->use_empty()) continue;
    SSAUpdater up;
    up.Initialize(d.orig->getType(), d.orig->getName());
    up.AddAvailableValue(d.orig->getParent(), d.orig);
    up.AddAvailableValue(d.again->getParent(), d.again);
    up.AddAvailableValue(d.gen->getParent(), d.gen);
    SmallVector<Use *, 8> uses;
    for (Use &u : d.gen->uses()) uses.push_back(&u);
    for (Use *u : uses) {
      auto *user = cast<Instruction>(u->getUser());
      auto *phi = dyn_cast<PHINode>(user);
      BasicBlock *at = phi ? phi->getIncomingBlock(*u) : user->getParent();
      if (at != d.gen->getParent()) up.RewriteUse(*u);
    }
  }
}

} // namespace

// Each cold edge is an entry into the generic copy, where every value live
// across it needs a join: a loop of more instructions or with more cold edges
// is left as it is, and the two copies of each loop grow a function by at most
// twice its own size.
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
  uint64_t budget = 2 * f.getInstructionCount(), grown = 0;
  bool changed = false;
  for (Loop *l : loops) {
    uint64_t size = 0;
    for (BasicBlock *b : l->blocks()) size += b->size();
    std::vector<ColdEdge> cold = coldEdges(*l);
    if (cold.empty() || cold.size() > kMaxColdEdges || size > kMaxLoopInstructions || grown + 2 * size > budget)
      continue;
    split(f, *l, cold);
    grown += 2 * size;
    changed = true;
  }
  return changed;
}

} // namespace xb
