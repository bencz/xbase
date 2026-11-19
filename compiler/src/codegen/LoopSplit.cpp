// Loops whose cold paths leave for a generic copy (see LoopSplit.h).
#include "codegen/LoopSplit.h"

#include <llvm/Analysis/BlockFrequencyInfo.h>
#include <llvm/Analysis/BranchProbabilityInfo.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/MDBuilder.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/ProfDataUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/LoopSimplify.h>
#include <llvm/Transforms/Utils/LoopUtils.h>
#include <llvm/Transforms/Utils/SSAUpdater.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

#include "xbrt.h" // constants only (type tags)

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace llvm;

namespace xb {

namespace {

// An edge is cold when its branch weight is below 1/64 of the branch's
// total: the lowering weights its slow paths as unlikely. A form test
// (kFormTest) is weighted for the layout of the path of items alone: an array
// of a compact form takes its other side on every pass, no cold path.
bool coldEdge(const Instruction &term, unsigned succ) {
  if (term.getMetadata(term.getContext().getMDKindID(kFormTest))) return false;
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

// A call that counts: not to an intrinsic or a write barrier (kBarrierAttr).
bool realCall(const Instruction &i) {
  auto *c = dyn_cast<CallBase>(&i);
  if (!c || isa<IntrinsicInst>(c)) return false;
  const Function *callee = c->getCalledFunction();
  return !callee || !callee->hasFnAttribute(kBarrierAttr);
}

// Whether the loop's fast copy would make a call: one reached from the header without a cold edge (a call of the
// program, not a slow path).
bool hotCall(const Loop &loop) {
  std::unordered_set<const BasicBlock *> seen{loop.getHeader()};
  std::vector<const BasicBlock *> work{loop.getHeader()};
  while (!work.empty()) {
    const BasicBlock *b = work.back();
    work.pop_back();
    for (const Instruction &i : *b)
      if (realCall(i)) return true;
    const Instruction *t = b->getTerminator();
    for (unsigned k = 0; k < t->getNumSuccessors(); ++k) {
      const BasicBlock *s = t->getSuccessor(k);
      if (loop.contains(s) && !coldEdge(*t, k) && seen.insert(s).second) work.push_back(s);
    }
  }
  return false;
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

  // What the back edges bring to the headers, taken before a cold edge that is a back edge itself removes it: the
  // loop's and, in a nest, each inner loop's (one with a single latch: another stays generic until its outer loop's
  // pass ends).
  struct Back {
    BasicBlock *header, *latch;
    std::vector<std::pair<PHINode *, Value *>> entries;
  };
  std::vector<Back> backs;
  for (Loop *l : loop.getLoopsInPreorder()) {
    BasicBlock *h = l->getHeader(), *latch = l->getLoopLatch();
    if (!latch) continue;
    Back back{h, latch, {}};
    for (PHINode &phi : h->phis())
      back.entries.push_back({cast<PHINode>(again[&phi]), inCopy(gen, phi.getIncomingValueForBlock(latch))});
    backs.push_back(std::move(back));
  }

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

  // A back edge of the generic copy enters the loop again at the same header, through a preheader of its own (where
  // what is hoisted out of that loop is computed again, after the calls of the generic pass): one slow pass of an
  // inner loop, not the rest of its outer loop's, runs generically.
  for (const Back &back : backs) {
    auto *genHeader = cast<BasicBlock>(gen[back.header]), *genLatch = cast<BasicBlock>(gen[back.latch]);
    auto *againHeader = cast<BasicBlock>(again[back.header]);
    BasicBlock *reenter = BasicBlock::Create(f.getContext(), back.header->getName() + ".reenter", &f, againHeader);
    BranchInst::Create(againHeader, reenter);
    for (auto [phi, v] : back.entries) phi->addIncoming(v, reenter);
    genHeader->removePredecessor(genLatch, /*KeepOneInputPHIs=*/true);
    genLatch->getTerminator()->replaceSuccessorWith(genHeader, reenter);
  }

  // A value of the generic copy, or of the loop again (entered at an inner loop's header from the generic copy), used
  // where another copy's may arrive instead (through a redirected edge) becomes their join; in a copy's own block the
  // copy dominates its uses. The original is entered at its header only, where its values dominate their uses. The
  // pairs are taken first: the updater adds PHIs.
  struct Defs {
    Instruction *orig, *gen, *again;
  };
  std::vector<Defs> defs;
  for (BasicBlock *b : blocks)
    for (Instruction &i : *b)
      if (!i.getType()->isVoidTy())
        defs.push_back({&i, cast<Instruction>(gen[&i]), cast<Instruction>(again[&i])});
  bool nest = !loop.isInnermost();
  for (const Defs &d : defs) {
    SmallVector<Use *, 8> uses;
    for (Instruction *def : {d.gen, nest ? d.again : nullptr}) {
      if (!def) continue;
      for (Use &u : def->uses()) {
        auto *user = cast<Instruction>(u.getUser());
        auto *phi = dyn_cast<PHINode>(user);
        if ((phi ? phi->getIncomingBlock(u) : user->getParent()) != def->getParent()) uses.push_back(&u);
      }
    }
    if (uses.empty()) continue;
    SSAUpdater up;
    up.Initialize(d.orig->getType(), d.orig->getName());
    up.AddAvailableValue(d.orig->getParent(), d.orig);
    up.AddAvailableValue(d.again->getParent(), d.again);
    up.AddAvailableValue(d.gen->getParent(), d.gen);
    for (Use *u : uses) up.RewriteUse(*u);
  }
}

} // namespace

// Each cold edge is an entry into the generic copy, where every value live
// across it needs a join: a loop of more instructions or with more cold edges
// is left as it is, and the two copies of each loop grow a function by at most
// twice its own size.
constexpr uint64_t kMaxLoopInstructions = 3000;
constexpr size_t kMaxColdEdges = 256;

bool splitColdLoopPaths(Function &f) {
  DominatorTree dt(f);
  LoopInfo li(dt);
  // A loop that itself runs on a cold path (its header less frequent than
  // 1/64 of the function's entry: the generic copy of typed continuations,
  // error handling) is not worth its copy.
  BranchProbabilityInfo bpi(f, li);
  BlockFrequencyInfo bfi(f, bpi, li);
  uint64_t entryFreq = bfi.getEntryFreq().getFrequency();
  // The outermost loop of a nest small enough to clone (within the limits and what is left of the budget) whose fast
  // copy would make no call is split whole: the cold edges of its inner loops go to the generic copy of the nest too.
  // A nest that is not (one that calls a function of the program anyway) is left as it is, and its inner loops are
  // looked at.
  std::vector<Loop *> loops;
  uint64_t budget = 2 * f.getInstructionCount(), grown = 0;
  std::function<void(Loop *)> choose = [&](Loop *l) {
    if (bfi.getBlockFreq(l->getHeader()).getFrequency() * 64 < entryFreq) return;
    uint64_t size = 0;
    for (BasicBlock *b : l->blocks()) size += b->size();
    size_t cold = coldEdges(*l).size();
    if (cold > 0 && cold <= kMaxColdEdges && size <= kMaxLoopInstructions && grown + 2 * size <= budget &&
        l->isSafeToClone() && (l->isInnermost() || !hotCall(*l))) {
      loops.push_back(l);
      grown += 2 * size;
      return;
    }
    for (Loop *sub : l->getSubLoops()) choose(sub);
  };
  for (Loop *top : li) choose(top);
  if (loops.empty()) return false;
  // Every candidate is put in shape while the analyses still describe the
  // function; the splits below change it but leave the other candidates' own
  // blocks (and their Loop objects) as they are.
  for (Loop *l : loops) {
    simplifyLoop(l, &dt, &li, nullptr, nullptr, nullptr, /*PreserveLCSSA=*/false);
    formLCSSARecursively(*l, dt, &li, nullptr); // a nest: its inner loops first
  }
  for (Loop *l : loops) split(f, *l, coldEdges(*l));
  return true;
}

namespace {

// A load of the sharing state of the object an item holds (kStateLoad), whose pointer word was loaded before the loop
// (not from STATIC storage, which another thread may write): read in the preheader once the item's type shows a
// pointer to an object (an array, a hash, a cell or a reference).
struct StateLoad {
  LoadInst *state;
  LoadInst *payload;
  int64_t offset;      // of the state byte from the object
  int64_t headerDelta; // of the item's header word from its pointer word
  unsigned typeShift;  // of the type in the header word
};

// The state loads of a loop that makes no call, the only way an object one thread reaches alone becomes shared: then
// what such a load reads on entry it reads on every pass.
std::vector<StateLoad> stateLoads(const Loop &loop, const DominatorTree &dt, const BasicBlock *pre) {
  std::vector<StateLoad> out;
  for (BasicBlock *b : loop.blocks())
    for (Instruction &i : *b)
      if (realCall(i)) return {};
  const DataLayout &dl = pre->getModule()->getDataLayout();
  unsigned stateKind = pre->getContext().getMDKindID(kStateLoad);
  for (BasicBlock *b : loop.blocks())
    for (Instruction &i : *b) {
      auto *s = dyn_cast<LoadInst>(&i);
      MDNode *md = s ? s->getMetadata(stateKind) : nullptr;
      if (!md) continue;
      int64_t offset = 0;
      auto *p = dyn_cast<LoadInst>(GetPointerBaseWithConstantOffset(s->getPointerOperand(), offset, dl));
      if (!p || loop.contains(p) || !dt.dominates(p, pre->getTerminator())) continue;
      if (isa<GlobalVariable>(getUnderlyingObject(p->getPointerOperand()))) continue; // STATIC storage: shared storage
      auto field = [&](unsigned k) { return mdconst::extract<ConstantInt>(md->getOperand(k))->getSExtValue(); };
      out.push_back({s, p, offset, field(0), (unsigned)field(1)});
    }
  return out;
}

// Whether `v` is known in the preheader, or can be computed there from what is (no memory read but a state load of
// `states`, nothing that may fault).
// The state loads it reads go to `used`.
bool computableOnEntry(const Loop &loop, const DominatorTree &dt, const BasicBlock *pre,
                       const std::vector<StateLoad> &states, Value *v, std::unordered_set<const LoadInst *> &used,
                       unsigned depth = 0) {
  auto *i = dyn_cast<Instruction>(v);
  if (!i || !loop.contains(i)) return !i || dt.dominates(i, pre->getTerminator());
  for (const StateLoad &s : states)
    if (s.state == i) {
      used.insert(s.state);
      return true;
    }
  if (depth > 8 || isa<PHINode>(i) || i->mayReadFromMemory() || !isSafeToSpeculativelyExecute(i)) return false;
  for (Value *op : i->operands())
    if (!computableOnEntry(loop, dt, pre, states, op, used, depth + 1)) return false;
  return true;
}

// A branch of the loop whose condition is computable on entry and whose cold successor `cold` is outside the loop, or
// a test of an array's form (kFormTest), whose `cold` successor is the path of a compact form.
struct InvariantExit {
  BranchInst *br;
  unsigned cold;
};

// Also leaves in `states` only the state loads the exits read: the others' items need no test on entry.
std::vector<InvariantExit> invariantColdExits(const Loop &loop, const DominatorTree &dt, const BasicBlock *pre,
                                              std::vector<StateLoad> &states) {
  std::vector<InvariantExit> out;
  std::unordered_set<const LoadInst *> used;
  unsigned formKind = pre->getContext().getMDKindID(kFormTest);
  // The paths of compact forms: the loop that runs when every exit stays takes the path of items at each form test, so
  // an exit on the other side of one counts for nothing (and would send an array of items to the copy).
  std::vector<BasicBlockEdge> compactSides;
  for (BasicBlock *b : loop.blocks())
    if (auto *br = dyn_cast<BranchInst>(b->getTerminator()); br && br->isConditional() && br->getMetadata(formKind))
      compactSides.emplace_back(b, br->getSuccessor(1));
  auto onCompactSide = [&](const BasicBlock *b) {
    return std::any_of(compactSides.begin(), compactSides.end(),
                       [&](const BasicBlockEdge &e) { return dt.dominates(e, b); });
  };
  for (BasicBlock *b : loop.blocks()) {
    auto *br = dyn_cast<BranchInst>(b->getTerminator());
    if (!br || !br->isConditional()) continue;
    if (onCompactSide(b)) continue;
    if (br->getMetadata(formKind)) {
      std::unordered_set<const LoadInst *> reads;
      if (computableOnEntry(loop, dt, pre, states, br->getCondition(), reads)) {
        out.push_back({br, 1});
        used.insert(reads.begin(), reads.end());
      }
      continue;
    }
    for (unsigned k = 0; k < 2; ++k)
      if (!loop.contains(br->getSuccessor(k)) && loop.contains(br->getSuccessor(1 - k)) && coldEdge(*br, k)) {
        std::unordered_set<const LoadInst *> reads;
        if (computableOnEntry(loop, dt, pre, states, br->getCondition(), reads)) {
          out.push_back({br, k});
          used.insert(reads.begin(), reads.end());
        }
        break;
      }
  }
  states.erase(std::remove_if(states.begin(), states.end(), [&](const StateLoad &s) { return !used.count(s.state); }),
               states.end());
  return out;
}

// The loop is cloned exactly. The preheader tests that the items of the state loads hold objects (else the clone runs),
// then reads their states and computes the exits' conditions; the clone runs when any would leave, the loop itself
// otherwise, where the exits become branches to their other side. A value of the loop used past it becomes the join
// of the loop's and the clone's.
void version(Function &f, Loop &loop, BasicBlock *pre, const std::vector<StateLoad> &states,
             const std::vector<InvariantExit> &exits, std::unordered_set<const BasicBlock *> &copies,
             ValueToValueMapTy *mapped = nullptr) {
  std::vector<BasicBlock *> blocks(loop.block_begin(), loop.block_end());
  SmallVector<BasicBlock *, 8> exitBlocks;
  loop.getUniqueExitBlocks(exitBlocks);
  ValueToValueMapTy own;
  ValueToValueMapTy &vmap = mapped ? *mapped : own;
  cloneLoop(f, blocks, vmap, ".exact");
  BasicBlock *header = loop.getHeader();
  auto *copyHeader = cast<BasicBlock>(vmap[header]);
  for (BasicBlock *e : exitBlocks)
    for (PHINode &phi : e->phis()) {
      unsigned n = phi.getNumIncomingValues();
      for (unsigned k = 0; k < n; ++k)
        if (BasicBlock *from = phi.getIncomingBlock(k); loop.contains(from))
          phi.addIncoming(inCopy(vmap, phi.getIncomingValue(k)), cast<BasicBlock>(vmap[from]));
    }

  LLVMContext &ctx = f.getContext();
  MDBuilder md(ctx);
  std::vector<BasicBlock *> toCopy; // the blocks that enter the clone
  BasicBlock *at = pre;
  IRBuilder<> b(pre->getTerminator());
  std::unordered_map<Value *, Value *> onEntry; // a loop value computed on entry
  if (!states.empty()) {
    Value *objects = b.getTrue();
    std::unordered_set<LoadInst *> seen;
    for (const StateLoad &s : states) {
      if (!seen.insert(s.payload).second) continue;
      Align align = commonAlignment(s.payload->getAlign(), (uint64_t)-s.headerDelta); // the item's own alignment
      Value *word = b.CreateAlignedLoad(b.getInt64Ty(),
                                        b.CreateConstGEP1_64(b.getInt8Ty(), s.payload->getPointerOperand(),
                                                             (uint64_t)s.headerDelta),
                                        align, "entry.header");
      Value *type = b.CreateTrunc(b.CreateLShr(word, s.typeShift), b.getInt32Ty());
      Value *object = b.getFalse();
      for (uint32_t t : {XB_T_ARRAY, XB_T_HASH, XB_T_CELL, XB_T_REF})
        object = b.CreateOr(object, b.CreateICmpEQ(type, b.getInt32(t)));
      objects = b.CreateAnd(objects, object);
    }
    BasicBlock *read = BasicBlock::Create(ctx, header->getName() + ".states", &f, header);
    b.CreateCondBr(objects, read, copyHeader, md.createLikelyBranchWeights());
    pre->getTerminator()->eraseFromParent();
    toCopy.push_back(pre);
    at = read;
    b.SetInsertPoint(read);
    for (const StateLoad &s : states) {
      auto *v = b.CreateAlignedLoad(b.getInt8Ty(), b.CreateConstGEP1_64(b.getInt8Ty(), s.payload, (uint64_t)s.offset),
                                    Align(1), "entry.state");
      v->setAtomic(AtomicOrdering::Unordered);
      v->setAAMetadata(s.state->getAAMetadata());
      onEntry[s.state] = v;
    }
  }
  std::function<Value *(Value *)> entryValue = [&](Value *v) -> Value * {
    auto *i = dyn_cast<Instruction>(v);
    if (!i || !loop.contains(i)) return v;
    if (auto it = onEntry.find(v); it != onEntry.end()) return it->second;
    Instruction *c = i->clone();
    for (Use &op : c->operands()) op.set(entryValue(op.get()));
    b.Insert(c);
    onEntry[v] = c;
    return c;
  };
  Value *leaves = nullptr;
  for (const InvariantExit &x : exits) {
    Value *c = b.CreateFreeze(entryValue(x.br->getCondition()));
    if (x.cold == 1) c = b.CreateNot(c);
    leaves = leaves ? b.CreateOr(leaves, c) : c;
  }
  b.CreateCondBr(leaves, copyHeader, header, md.createUnlikelyBranchWeights());
  if (at == pre) pre->getTerminator()->eraseFromParent(); // the old branch, after the test
  toCopy.push_back(at);
  if (at != pre) header->replacePhiUsesWith(pre, at);
  for (PHINode &phi : header->phis()) {
    Value *entry = phi.getIncomingValueForBlock(at);
    for (BasicBlock *from : toCopy) cast<PHINode>(vmap[&phi])->addIncoming(entry, from);
  }

  // The pairs are taken first: the updater adds PHIs, in the loop's blocks too.
  std::vector<std::pair<Instruction *, Instruction *>> defs;
  for (BasicBlock *bb : blocks) {
    copies.insert(cast<BasicBlock>(vmap[bb]));
    for (Instruction &i : *bb)
      if (!i.getType()->isVoidTy()) defs.push_back({&i, cast<Instruction>(vmap[&i])});
  }
  for (auto [i, copy] : defs) {
    SmallVector<Use *, 8> outside;
    for (Use &u : i->uses()) {
      auto *user = cast<Instruction>(u.getUser());
      auto *phi = dyn_cast<PHINode>(user);
      BasicBlock *where = phi ? phi->getIncomingBlock(u) : user->getParent();
      if (!loop.contains(where) && !copies.count(where)) outside.push_back(&u);
    }
    if (outside.empty()) continue;
    SSAUpdater up;
    up.Initialize(i->getType(), i->getName());
    up.AddAvailableValue(i->getParent(), i);
    up.AddAvailableValue(copy->getParent(), copy);
    for (Use *u : outside) up.RewriteUse(*u);
  }

  for (const InvariantExit &x : exits) {
    BasicBlock *from = x.br->getParent(), *gone = x.br->getSuccessor(x.cold), *stay = x.br->getSuccessor(1 - x.cold);
    gone->removePredecessor(from, /*KeepOneInputPHIs=*/true);
    BranchInst::Create(stay, x.br->getIterator());
    x.br->eraseFromParent();
  }
}

// The copies of a loop's blocks that version() made.
std::vector<BasicBlock *> copyBlocks(const Loop &loop, ValueToValueMapTy &vmap) {
  std::vector<BasicBlock *> out;
  for (BasicBlock *b : loop.blocks())
    if (auto it = vmap.find(b); it != vmap.end()) out.push_back(cast<BasicBlock>(it->second));
  return out;
}

// Whether the loop tests an array's form (kFormTest).
bool hasFormTest(const Loop &loop) {
  unsigned formKind = loop.getHeader()->getContext().getMDKindID(kFormTest);
  for (BasicBlock *b : loop.blocks())
    if (b->getTerminator()->getMetadata(formKind)) return true;
  return false;
}

} // namespace

bool unswitchFormTests(Function &f, FunctionAnalysisManager &) {
  std::unordered_set<const BasicBlock *> copies;
  uint64_t budget = f.getInstructionCount(), grown = 0;
  bool changed = false;
  unsigned formKind = f.getContext().getMDKindID(kFormTest);
  while (true) {
    DominatorTree dt(f);
    LoopInfo li(dt);
    Loop *chosen = nullptr;
    BranchInst *test = nullptr;
    for (Loop *l : li.getLoopsInPreorder()) {
      auto *br = dyn_cast<BranchInst>(l->getHeader()->getTerminator());
      bool hoisted = false; // a condition computed in the loop from what it does not change goes out first
      if (!l->isInnermost() || copies.count(l->getHeader()) || !br || !br->isConditional() ||
          !br->getMetadata(formKind) || !l->contains(br->getSuccessor(0)) || !l->contains(br->getSuccessor(1)) ||
          !l->isSafeToClone() ||
          !l->getLoopPreheader() || !l->makeLoopInvariant(br->getCondition(), hoisted))
        continue;
      changed |= hoisted;
      uint64_t size = 0;
      for (BasicBlock *b : l->blocks()) size += b->size();
      if (size > kMaxLoopInstructions || grown + size > budget) continue;
      chosen = l;
      test = br;
      grown += size;
      break;
    }
    if (!chosen) return changed;
    BasicBlock *pre = chosen->getLoopPreheader(), *header = chosen->getHeader();
    ValueToValueMapTy vmap;
    version(f, *chosen, pre, {}, {{test, 1}}, copies, &vmap);
    // The copy runs when the array does not hold items on entry, and the condition the loop does not change says so
    // on every pass: its test goes to the other side alone (the path of a compact form, which tests the rest). Its
    // other form tests are plain branches: the copy is versioned no more.
    auto *copyTest = cast<BranchInst>(cast<BasicBlock>(vmap[header])->getTerminator());
    BasicBlock *compact = copyTest->getSuccessor(1);
    copyTest->getSuccessor(0)->removePredecessor(copyTest->getParent(), /*KeepOneInputPHIs=*/true);
    BranchInst::Create(compact, copyTest->getIterator());
    copyTest->eraseFromParent();
    for (BasicBlock *b : copyBlocks(*chosen, vmap)) b->getTerminator()->setMetadata(formKind, nullptr);
    changed = true;
  }
}

bool versionInvariantColdExits(Function &f, FunctionAnalysisManager &) {
  // One loop at a time, outer loops first, with the analyses made again: a version changes the blocks around the loop.
  // A loop done, and the loops of an exact copy (which keeps the exits), are not looked at again.
  std::unordered_set<const BasicBlock *> done, copies;
  uint64_t budget = f.getInstructionCount(), grown = 0;
  bool changed = false;
  while (true) {
    DominatorTree dt(f);
    LoopInfo li(dt);
    Loop *chosen = nullptr;
    std::vector<StateLoad> states;
    std::vector<InvariantExit> exits;
    for (Loop *l : li.getLoopsInPreorder()) {
      if (!l->isInnermost() || done.count(l->getHeader()) || copies.count(l->getHeader()) || !l->isSafeToClone())
        continue;
      done.insert(l->getHeader());
      BasicBlock *pre = l->getLoopPreheader();
      if (!pre && hasFormTest(*l)) { // entered from several blocks: one is made, which they all go through
        pre = InsertPreheaderForLoop(l, &dt, &li, nullptr, /*PreserveLCSSA=*/false);
        changed |= pre != nullptr;
      }
      uint64_t size = 0;
      for (BasicBlock *b : l->blocks()) size += b->size();
      if (!pre || !isa<BranchInst>(pre->getTerminator()) || size > kMaxLoopInstructions || grown + size > budget)
        continue;
      states = stateLoads(*l, dt, pre);
      exits = invariantColdExits(*l, dt, pre, states);
      if (exits.empty() || exits.size() > kMaxColdEdges) continue;
      chosen = l;
      grown += size;
      break;
    }
    if (!chosen) return changed;
    done.insert(chosen->getHeader());
    version(f, *chosen, chosen->getLoopPreheader(), states, exits, copies);
    changed = true;
  }
}

} // namespace xb
