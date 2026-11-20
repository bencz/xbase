// Fewer polls on loops' back edges (see LoopPolls.h).
#include "codegen/LoopPolls.h"
#include "codegen/Branches.h"

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/MDBuilder.h>
#include <llvm/IR/PatternMatch.h>
#include <llvm/Transforms/Utils/LoopUtils.h>
#include <llvm/Transforms/Utils/SSAUpdater.h>
#include <llvm/Transforms/Utils/ScalarEvolutionExpander.h>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace llvm;
using namespace llvm::PatternMatch;

namespace xb {

namespace {

// Passes of a loop between two polls: a safepoint still comes within microseconds, and the poll costs a few
// instructions once in that many passes.
constexpr uint64_t kPassesPerPoll = 1024;

// A test of the pending requests: a branch on whether a load marked `kind` is non-zero, to the slow path (a call to
// the runtime that serves the requests) or on.
struct PendingTest {
  CondBranch *br;
  LoadInst *load;
  BasicBlock *slow, *cont;
};

// The successor a branch on `value != 0` (or, inverted, `value == 0`) takes when the value is non-zero.
bool nonZeroSuccessor(const CondBranch *br, Value *&value, unsigned &taken) {
  auto *cmp = br ? dyn_cast<ICmpInst>(br->getCondition()) : nullptr;
  if (!cmp || !cmp->isEquality() || !match(cmp->getOperand(1), m_Zero())) return false;
  value = cmp->getOperand(0);
  taken = cmp->getPredicate() == ICmpInst::ICMP_NE ? 0 : 1;
  return true;
}

bool pendingTest(Instruction *term, const char *kind, PendingTest &t) {
  auto *br = condBranch(term);
  Value *v = nullptr;
  unsigned taken = 0;
  if (!nonZeroSuccessor(br, v, taken)) return false;
  auto *load = dyn_cast<LoadInst>(v);
  if (!load || !load->getMetadata(kind)) return false;
  t = {br, load, br->getSuccessor(taken), br->getSuccessor(1 - taken)};
  return true;
}

// The test's successors as its branch has them now: a preheader inserted for another loop after the test was found may
// have split the edge to one (an outer loop's poll going on into an inner loop's header).
void refresh(PendingTest &t) {
  Value *v = nullptr;
  unsigned taken = 0;
  nonZeroSuccessor(t.br, v, taken);
  t.slow = t.br->getSuccessor(taken);
  t.cont = t.br->getSuccessor(1 - taken);
}

// Whether `v` can be used at the end of block `at`.
bool availableAt(const DominatorTree &dt, Value *v, BasicBlock *at) {
  auto *i = dyn_cast<Instruction>(v);
  return !i || dt.dominates(i, at->getTerminator());
}

// A counted loop's exit test: the branch of `test` on a comparison of an induction variable `x` (of constant `step`)
// with a value `end` the loop does not change; the loop goes on (to successor `stay`) while `x pred end`: `ne`, or
// `<` or `<=` (signed or not) going up, `>` or `>=` going down. With a stop in place of `end`, the loop goes on while
// `x pred stop`, past it on an order's (`<=`: one step past), then tells the exit from a poll by `x pred end`.
struct Counted {
  BasicBlock *test;
  Value *x, *end;
  ICmpInst::Predicate pred;
  unsigned stay;
  int64_t step;
  const SCEV *start;
};

bool countedExit(Loop &loop, LoopInfo &li, ScalarEvolution &se, BasicBlock *b, Counted &c) {
  if (li.getLoopFor(b) != &loop) return false;
  auto *br = condBranch(b->getTerminator());
  if (!br || loop.contains(br->getSuccessor(0)) == loop.contains(br->getSuccessor(1)))
    return false;
  auto *cmp = dyn_cast<ICmpInst>(br->getCondition());
  if (!cmp || !cmp->getOperand(0)->getType()->isIntegerTy()) return false;
  unsigned stay = loop.contains(br->getSuccessor(0)) ? 0u : 1u;
  for (unsigned k = 0; k < 2; ++k) {
    if (!loop.isLoopInvariant(cmp->getOperand(1 - k))) continue;
    auto *ar = dyn_cast<SCEVAddRecExpr>(se.getSCEV(cmp->getOperand(k)));
    if (!ar || ar->getLoop() != &loop || !ar->isAffine()) continue;
    auto *step = dyn_cast<SCEVConstant>(ar->getStepRecurrence(se));
    if (!step || step->getAPInt().getSignificantBits() > 32 || step->getAPInt().isZero()) continue;
    int64_t by = step->getAPInt().getSExtValue();
    uint64_t stride = static_cast<uint64_t>(by < 0 ? -by : by);
    unsigned bits = cmp->getOperand(0)->getType()->getIntegerBitWidth();
    if (bits < 2 || bits > 64 || kPassesPerPoll * stride >= uint64_t{1} << (bits - 1)) continue; // the stride fits
    ICmpInst::Predicate pred = k == 0 ? cmp->getPredicate() : cmp->getSwappedPredicate();
    if (stay == 1) pred = ICmpInst::getInversePredicate(pred);
    bool up = pred == ICmpInst::ICMP_SLT || pred == ICmpInst::ICMP_ULT || pred == ICmpInst::ICMP_SLE ||
              pred == ICmpInst::ICMP_ULE;
    bool down = pred == ICmpInst::ICMP_SGT || pred == ICmpInst::ICMP_UGT || pred == ICmpInst::ICMP_SGE ||
                pred == ICmpInst::ICMP_UGE;
    bool counts = pred == ICmpInst::ICMP_NE || (by > 0 && up) || (by < 0 && down);
    if (!counts) continue;
    c = {b, cmp->getOperand(k), cmp->getOperand(1 - k), pred, stay, by, ar->getStart()};
    return true;
  }
  return false;
}

// The stop after `x`: kPassesPerPoll steps ahead, or `end` when the loop ends first. The distance is unsigned: for
// `ne`, the variable reaches `end` after (end - x) / step passes, wrapping as the loop's own arithmetic does, when the
// step divides the distance (otherwise never); for an order, only while x is still on its side of `end`.
Value *nextStop(IRBuilder<> &b, const Counted &c, Value *x) {
  Type *ty = x->getType();
  uint64_t stride = static_cast<uint64_t>(c.step < 0 ? -c.step : c.step);
  Value *dist = c.step > 0 ? b.CreateSub(c.end, x) : b.CreateSub(x, c.end);
  Value *far = b.CreateICmpUGT(dist, ConstantInt::get(ty, kPassesPerPoll * stride));
  if (c.pred != ICmpInst::ICMP_NE)
    far = b.CreateAnd(far, b.CreateICmp(c.pred, x, c.end));
  else if (stride > 1) // a distance the steps do not divide: the variable never meets `end`, it keeps its stops
    far = b.CreateOr(far, b.CreateICmpNE(b.CreateURem(dist, ConstantInt::get(ty, stride)), ConstantInt::get(ty, 0)));
  Value *ahead = b.CreateAdd(x, ConstantInt::getSigned(ty, static_cast<int64_t>(kPassesPerPoll) * c.step));
  return b.CreateSelect(far, ahead, c.end, "poll.stop");
}

void removeTest(const PendingTest &t) {
  createBranch(t.cont, t.br->getIterator());
  t.slow->removePredecessor(t.br->getParent(), /*KeepOneInputPHIs=*/true); // others may still enter it
  auto *cmp = cast<Instruction>(t.br->getCondition());
  t.br->eraseFromParent();
  if (cmp->use_empty()) cmp->eraseFromParent();
  if (t.load->use_empty()) t.load->eraseFromParent(); // an atomic load is not dead to the optimizer
}

// Whether running the block of an exit test again, with the values its PHIs had, changes nothing: no call, no atomic
// or volatile access, and stores only when it loads nothing (they store again what they stored, the frame's homes of
// the loop's variables).
bool rerunnable(const BasicBlock &test) {
  bool loads = false, stores = false;
  for (const Instruction &i : test) {
    if (auto *ld = dyn_cast<LoadInst>(&i)) {
      if (!ld->isSimple()) return false;
      loads = true;
    } else if (auto *st = dyn_cast<StoreInst>(&i)) {
      if (!st->isSimple()) return false;
      stores = true;
    } else if (i.mayHaveSideEffects() || isa<CallBase>(i)) {
      return false;
    }
  }
  return !(loads && stores);
}

// What a poll's slow path receives at the end of a pass, as the header has it at the start of the next: the value
// itself when it comes from outside the loop, else the header's PHI that the back edge from `latch` gives it to.
Value *atHeader(Loop &loop, BasicBlock *latch, Value *v) {
  if (loop.isLoopInvariant(v)) return v;
  for (PHINode &phi : loop.getHeader()->phis())
    if (phi.getBasicBlockIndex(latch) >= 0 && phi.getIncomingValueForBlock(latch) == v) return &phi;
  return nullptr;
}

// The uses, on the way from a poll's slow path on, of values the loop computes after its header: the new poll enters
// that path from the header, where each such value is the one its PHI carries to the next pass (atHeader). False when
// one has no such PHI, is the header's own (the next pass's there), or is an inner loop's (whose own polls may rewrite
// its uses: each loop rewrites only its own). The way stops at the header: past it, a pass computes its values anew
// before any use (each value's definition dominates its uses), as on any other pass.
bool carriedPast(Loop &loop, LoopInfo &li, const PendingTest &poll,
                 std::vector<std::pair<Instruction *, Use *>> &uses) {
  std::unordered_set<const BasicBlock *> reached{poll.slow};
  std::vector<BasicBlock *> work{poll.slow};
  while (!work.empty()) {
    BasicBlock *b = work.back();
    work.pop_back();
    for (BasicBlock *s : successors(b))
      if (s != loop.getHeader() && reached.insert(s).second) work.push_back(s);
  }
  BasicBlock *latch = poll.br->getParent();
  for (BasicBlock *b : loop.blocks())
    for (Instruction &i : *b)
      for (Use &u : i.uses()) {
        auto *user = cast<Instruction>(u.getUser());
        auto *phi = dyn_cast<PHINode>(user);
        BasicBlock *at = phi ? phi->getIncomingBlock(u) : user->getParent();
        if (loop.contains(at) || !reached.count(at)) continue;
        // The header's values are the next pass's at the new poll: the path wants the pass before's.
        if (b == loop.getHeader() || li.getLoopFor(b) != &loop || !atHeader(loop, latch, &i)) return false;
        uses.push_back({&i, &u});
      }
  return true;
}

// The counted loop polls when its variable reaches the stop, in the branch of its exit test; when a request is pending,
// it goes where `poll` would have gone. The test is the header's (then with the header's values, those of the pass
// before: atHeader), or the one of the block `poll` goes on to, which its block dominates (a loop LLVM rotated: then
// with the values the pass has there).
void stripMine(Function &f, Loop &loop, const Counted &c, const PendingTest &poll, BasicBlock *pre, Value *start,
               const std::vector<std::pair<Instruction *, Use *>> &carried) {
  LLVMContext &ctx = f.getContext();
  auto *br = cast<CondBranch>(c.test->getTerminator());
  BasicBlock *stay = br->getSuccessor(c.stay), *exit = br->getSuccessor(1 - c.stay);
  BasicBlock *header = loop.getHeader(), *latch = poll.br->getParent();
  bool atTop = c.test == header;
  auto passed = [&](Value *v) { return atTop ? atHeader(loop, latch, v) : v; };
  IRBuilder<> b(pre->getTerminator());
  Value *stop0 = nextStop(b, c, start);

  BasicBlock *reached = BasicBlock::Create(ctx, "poll.reached", &f, stay);
  BasicBlock *check = BasicBlock::Create(ctx, "poll", &f, stay);
  BasicBlock *rearm = BasicBlock::Create(ctx, "poll.rearm", &f, stay);
  b.SetInsertPoint(rearm);
  Value *stop1 = nextStop(b, c, c.x);
  b.CreateBr(c.test);
  for (PHINode &phi : c.test->phis()) phi.addIncoming(&phi, rearm);

  // The stop on each way back to the header and into the test.
  PHINode *stop = PHINode::Create(c.x->getType(), 2, "poll.stop", header->begin());
  SSAUpdater up;
  up.Initialize(c.x->getType(), "poll.stop");
  up.AddAvailableValue(header, stop);
  up.AddAvailableValue(rearm, stop1);
  Value *stopAtTest = atTop ? stop : up.GetValueInMiddleOfBlock(c.test);
  b.SetInsertPoint(br);
  br->setCondition(b.CreateICmp(c.pred, c.x, stopAtTest));
  br->setSuccessor(0, stay);
  br->setSuccessor(1, reached);
  MDBuilder md(ctx);
  br->setMetadata(LLVMContext::MD_prof, md.createBranchWeights(63, 1));
  for (PHINode &phi : exit->phis()) phi.replaceIncomingBlockWith(c.test, reached);
  b.SetInsertPoint(reached);
  b.CreateCondBr(b.CreateICmp(c.pred, c.x, c.end), check, exit);

  b.SetInsertPoint(check);
  LoadInst *pending = b.CreateAlignedLoad(poll.load->getType(), poll.load->getPointerOperand(), poll.load->getAlign(),
                                          "pending");
  pending->setAtomic(poll.load->getOrdering(), poll.load->getSyncScopeID());
  pending->copyMetadata(*poll.load);
  b.CreateCondBr(b.CreateICmpNE(pending, ConstantInt::get(pending->getType(), 0)), poll.slow, rearm,
                 md.createUnlikelyBranchWeights());
  for (PHINode &phi : poll.slow->phis()) phi.addIncoming(passed(phi.getIncomingValueForBlock(latch)), check);
  for (auto [value, use] : carried) {
    SSAUpdater carry;
    carry.Initialize(value->getType(), value->getName());
    carry.AddAvailableValue(value->getParent(), value);
    carry.AddAvailableValue(check, atHeader(loop, latch, value));
    carry.RewriteUse(*use);
  }

  std::vector<BasicBlock *> preds(pred_begin(header), pred_end(header));
  for (BasicBlock *from : preds) stop->addIncoming(from == pre ? stop0 : up.GetValueAtEndOfBlock(from), from);
}

// A loop that is not counted (or whose poll goes on elsewhere than to its header: a loop LLVM rotated) polls once every
// kPassesPerPoll passes of `poll`: a count in a register goes down on it, and only when it runs out does the loop load
// the pending requests.
void countDown(Function &f, Loop &loop, const PendingTest &poll) {
  LLVMContext &ctx = f.getContext();
  BasicBlock *header = loop.getHeader(), *at = poll.br->getParent(), *cont = poll.cont;
  Type *ty = Type::getInt32Ty(ctx);
  PHINode *left = PHINode::Create(ty, 2, "poll.left", header->begin());
  BasicBlock *check = BasicBlock::Create(ctx, "poll", &f, cont);
  IRBuilder<> b(poll.br);
  Value *next = b.CreateSub(left, ConstantInt::get(ty, 1), "poll.left");
  MDBuilder md(ctx);
  b.CreateCondBr(b.CreateICmpEQ(next, ConstantInt::get(ty, 0)), check, cont, md.createBranchWeights(1, 63));
  // The test moves to its own block on the way on: where it went on to and the slow path receive from it what they
  // did from its block.
  poll.br->removeFromParent();
  poll.br->insertInto(check, check->end());
  cast<Instruction>(poll.br->getCondition())->moveBefore(poll.br->getIterator());
  poll.load->moveBefore(cast<Instruction>(poll.br->getCondition())->getIterator());
  for (PHINode &phi : cont->phis())
    if (&phi != left) phi.addIncoming(phi.getIncomingValueForBlock(at), check);
  for (PHINode &phi : poll.slow->phis()) phi.replaceIncomingBlockWith(at, check);
  // The count on each way back to the header: one less after the test's block, anew after a poll and from outside.
  SSAUpdater up;
  up.Initialize(ty, "poll.left");
  up.AddAvailableValue(header, left);
  up.AddAvailableValue(at, next);
  up.AddAvailableValue(check, ConstantInt::get(ty, kPassesPerPoll));
  std::vector<BasicBlock *> preds(pred_begin(header), pred_end(header));
  for (BasicBlock *from : preds)
    left->addIncoming(loop.contains(from) ? up.GetValueAtEndOfBlock(from) : ConstantInt::get(ty, kPassesPerPoll), from);
}

} // namespace

bool skipRepeatedTests(Function &f) {
  auto calls = [](const Instruction &i) { return isa<CallBase>(i) && !isa<IntrinsicInst>(i); };
  // The slow path of each test, which serves the requests (one call) and goes on to the test's way on only when the
  // call found none to unwind for (its result 0; else it goes elsewhere): the test passed then too.
  std::unordered_map<const BasicBlock *, const BasicBlock *> served;
  for (BasicBlock &b : f) {
    PendingTest t;
    for (const char *kind : {kSafepointMetadata, kPollMetadata}) {
      if (!pendingTest(b.getTerminator(), kind, t) || t.slow == t.cont || t.slow->getSinglePredecessor() != &b ||
          std::count_if(t.slow->begin(), t.slow->end(), calls) != 1)
        continue;
      Value *v = nullptr;
      unsigned taken = 0;
      auto *br = condBranch(t.slow->getTerminator());
      if (nonZeroSuccessor(br, v, taken) && isa<CallBase>(v) && br->getSuccessor(1 - taken) == t.cont &&
          br->getSuccessor(taken) != t.cont)
        served[t.slow] = t.cont;
    }
  }
  // Whether no call ran since a test that found nothing pending: at the end of each block along each edge (a test's
  // way on, and its slow path's: yes); the greatest fixed point, from yes everywhere but at the function's entry. A way
  // back (to a block no later in reverse postorder) carries none: a test a loop's passes go through serves the next
  // pass, and goes only when an earlier test of the same pass is kept (so no cycle runs without a test, and a test
  // that goes never stands for another that goes).
  std::unordered_map<const BasicBlock *, size_t> order;
  for (BasicBlock *b : ReversePostOrderTraversal<Function *>(&f)) order.emplace(b, order.size());
  std::unordered_map<const BasicBlock *, bool> atStart;
  auto edge = [&](BasicBlock *from, BasicBlock *to) {
    auto o = order.find(to), p = order.find(from);
    if (o == order.end() || p == order.end() || o->second <= p->second) return false;
    PendingTest t;
    for (const char *kind : {kSafepointMetadata, kPollMetadata})
      if (pendingTest(from->getTerminator(), kind, t) && t.load->getParent() == from &&
          std::none_of(t.load->getIterator(), from->end(), calls))
        return to == t.cont && t.slow != t.cont;
    if (auto s = served.find(from); s != served.end()) return to == s->second;
    bool clean = atStart.at(from);
    for (const Instruction &i : *from) clean = clean && !calls(i);
    return clean;
  };
  for (BasicBlock &b : f) atStart[&b] = &b != &f.getEntryBlock();
  for (bool changed = true; changed;) {
    changed = false;
    for (BasicBlock &b : f) {
      if (!atStart[&b]) continue;
      for (BasicBlock *p : predecessors(&b))
        if (!edge(p, &b)) {
          atStart[&b] = false;
          changed = true;
          break;
        }
    }
  }
  bool changed = false;
  for (BasicBlock &b : f) {
    PendingTest t;
    if (!pendingTest(b.getTerminator(), kSafepointMetadata, t) || t.load->getParent() != &b ||
        std::any_of(b.begin(), t.load->getIterator(), calls))
      continue;
    if (atStart[&b]) { // nothing can be pending here
      t.load->replaceAllUsesWith(ConstantInt::get(t.load->getType(), 0));
      t.load->eraseFromParent();
      changed = true;
      continue;
    }
    // The ways in that ran no call find nothing pending; the others load the requests on their way.
    std::vector<BasicBlock *> preds(pred_begin(&b), pred_end(&b));
    if (std::none_of(preds.begin(), preds.end(), [&](BasicBlock *p) { return edge(p, &b); }) ||
        std::find(preds.begin(), preds.end(), &b) != preds.end())
      continue;
    PHINode *phi = PHINode::Create(t.load->getType(), (unsigned)preds.size(), "pending", b.begin());
    std::unordered_map<BasicBlock *, Value *> from;
    for (BasicBlock *p : preds) {
      Value *&v = from[p]; // one value per block, also for several edges from it
      if (!v && edge(p, &b)) v = ConstantInt::get(t.load->getType(), 0);
      if (!v) {
        Instruction *load = t.load->clone();
        load->insertBefore(p->getTerminator()->getIterator());
        v = load;
      }
      phi->addIncoming(v, p);
    }
    t.load->replaceAllUsesWith(phi);
    t.load->eraseFromParent();
    changed = true;
  }
  return changed;
}

bool placeLoopPolls(Function &f, FunctionAnalysisManager &fam) {
  auto &dt = fam.getResult<DominatorTreeAnalysis>(f);
  auto &li = fam.getResult<LoopAnalysis>(f);
  auto &se = fam.getResult<ScalarEvolutionAnalysis>(f);

  // Decided first, while the analyses describe the function; the changes below keep each loop's own blocks.
  struct Plan {
    Loop *loop = nullptr;
    std::vector<PendingTest> polls;
    bool served = false;
    Counted counted{};
    std::vector<std::pair<Instruction *, Use *>> carried;
    bool countDown = false;
    BasicBlock *pre = nullptr; // strip mining: the preheader the loop's stop starts in
    Value *start = nullptr;
  };
  std::vector<Plan> plans;
  bool changed = false;
  for (Loop *loop : li.getLoopsInPreorder()) {
    Plan p;
    p.loop = loop;
    SmallVector<BasicBlock *, 4> latches;
    loop->getLoopLatches(latches);
    for (BasicBlock *b : loop->blocks()) {
      PendingTest t;
      if (li.getLoopFor(b) == loop && pendingTest(b->getTerminator(), kPollMetadata, t)) p.polls.push_back(t);
      if (!p.served && pendingTest(b->getTerminator(), kSafepointMetadata, t)) {
        p.served = true;
        for (BasicBlock *l : latches) p.served = p.served && dt.dominates(b, l);
      }
    }
    if (p.polls.empty()) continue;
    if (p.served) {
      plans.push_back(std::move(p));
      continue;
    }
    // One poll. A counted loop's exit test, which computes nothing else that its running again (after a poll) would
    // repeat, polls; another loop counts its passes down.
    BasicBlock *header = loop->getHeader();
    const PendingTest &poll = p.polls[0];
    // Only an innermost loop polls less than on every pass: an outer loop's pass runs its inner loops, whose polls
    // start anew on each entry (a loop shorter than the stride has none), so its own poll bounds the passes between
    // two of them.
    if (!loop->isInnermost() || p.polls.size() != 1 || loop->contains(poll.slow) || !loop->contains(poll.cont))
      continue;
    // The exit test that polls: the header's, which `poll` goes on to (the values its slow path receives are the
    // pass before's: carried to the header), or the one of the block `poll` goes on to, which its block dominates
    // (the pass's own values reach it).
    BasicBlock *test = poll.cont;
    bool ok = countedExit(*loop, li, se, test, p.counted) && rerunnable(*test) &&
              availableAt(dt, poll.load->getPointerOperand(), test);
    if (test == header) {
      ok = ok && carriedPast(*loop, li, poll, p.carried);
      for (PHINode &phi : poll.slow->phis())
        ok = ok && atHeader(*loop, poll.br->getParent(), phi.getIncomingValueForBlock(poll.br->getParent()));
    } else { // the test sees every value of the variable (`ne` must meet the stop): it runs on every pass
      ok = ok && dt.dominates(poll.br->getParent(), test);
      for (BasicBlock *l : latches) ok = ok && dt.dominates(test, l);
    }
    BasicBlock *pre = ok ? loop->getLoopPreheader() : nullptr;
    if (ok && !pre) {
      pre = InsertPreheaderForLoop(loop, &dt, &li, nullptr, false);
      changed = true;
    }
    SCEVExpander expander(se, "poll");
    if (pre && expander.isSafeToExpandAt(p.counted.start, pre->getTerminator())) {
      p.start = expander.expandCodeFor(p.counted.start, p.counted.x->getType(), pre->getTerminator());
      p.pre = pre;
      plans.push_back(std::move(p));
      continue;
    }
    bool alone = true; // the test's load and comparison serve the test only
    for (Instruction *i : {cast<Instruction>(poll.load), cast<Instruction>(poll.br->getCondition())})
      alone = alone && i->hasOneUse() && i->getParent() == poll.br->getParent();
    if (alone) {
      p.countDown = true;
      plans.push_back(std::move(p));
    }
  }

  for (Plan &p : plans) {
    for (PendingTest &t : p.polls) refresh(t);
    if (p.countDown) {
      countDown(f, *p.loop, p.polls[0]);
      continue;
    }
    // The changes made for loops before may have given its header another way in from outside (the slow path of one
    // they poll on reaches it): the plan no longer holds, and the loop keeps polling on every pass.
    if (!p.served && p.loop->getLoopPreheader() != p.pre) continue;
    if (!p.served) stripMine(f, *p.loop, p.counted, p.polls[0], p.pre, p.start, p.carried);
    for (const PendingTest &t : p.polls) removeTest(t);
  }
  return changed || !plans.empty();
}

} // namespace xb
