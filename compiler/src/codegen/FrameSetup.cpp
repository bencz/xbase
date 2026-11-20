// A function's frame set up where it is needed, not on entry (see FrameSetup.h).
#include "codegen/FrameSetup.h"
#include "codegen/Branches.h"

#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Local.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

#include <algorithm>
#include <unordered_set>
#include <vector>

using namespace llvm;

namespace xb {

namespace {

// The values computed from the frame's address (its slots' and fields' addresses), which touch nothing themselves.
std::unordered_set<const Value *> derivedFrom(AllocaInst *frame) {
  std::unordered_set<const Value *> out{frame};
  std::vector<const Value *> work{frame};
  while (!work.empty()) {
    const Value *v = work.back();
    work.pop_back();
    for (const User *u : v->users())
      if ((isa<GetElementPtrInst>(u) || isa<CastInst>(u) || isa<PHINode>(u) || isa<SelectInst>(u)) &&
          out.insert(u).second)
        work.push_back(u);
  }
  return out;
}

// The context's chain: its base (the context), the offset of the field the link writes, and the caller's frame.
struct Chain {
  const Value *ctx = nullptr;
  int64_t offset = 0;
  const Value *prev = nullptr;
};

// Whether `i` reads or writes the context's chain (an inlined body links a frame of its own over the function's),
// the unlink aside (a store of the caller's frame, which the chain holds when nothing was linked).
bool touchesChain(const Instruction &i, const Chain &chain, const DataLayout &dl) {
  const Value *p = nullptr;
  if (auto *l = dyn_cast<LoadInst>(&i)) p = l->getPointerOperand();
  if (auto *s = dyn_cast<StoreInst>(&i)) p = s->getValueOperand() == chain.prev ? nullptr : s->getPointerOperand();
  if (!p) return false;
  int64_t offset = 0;
  return GetPointerBaseWithConstantOffset(p, offset, dl) == chain.ctx && offset == chain.offset;
}

// Whether `i` needs the frame set up: it calls a function (anything may collect, raise an error or walk the chain;
// intrinsics aside, unless they touch the frame), reads, writes or lets out the frame's memory, or touches the chain.
bool needsFrame(const Instruction &i, const std::unordered_set<const Value *> &frame, const Chain &chain,
                const DataLayout &dl) {
  if (touchesChain(i, chain, dl)) return true;
  if (auto *call = dyn_cast<CallBase>(&i)) {
    if (!isa<IntrinsicInst>(call)) return true;
    for (const Value *a : call->args()) // an intrinsic that reaches the frame (a memset, a masked access, ...)
      if (frame.count(a)) return true;
    return false;
  }
  if (isa<LoadInst>(i) || isa<StoreInst>(i) || isa<AtomicRMWInst>(i) || isa<AtomicCmpXchgInst>(i))
    for (const Value *op : i.operands())
      if (frame.count(op)) return true;
  return false;
}

// The largest part of a function before its setups that is made a small entry of its own (instructions).
constexpr size_t kMaxLeafEntry = 48;

// Whether `b` tests the flag before a setup (`setBlocks`): false where the part before any setup runs, which takes only
// the setup's side there.
bool testsFlag(BasicBlock *b, const std::unordered_set<const BasicBlock *> &setBlocks) {
  return any_of(successors(b), [&](BasicBlock *n) { return setBlocks.count(n) != 0; });
}

// The blocks from which a return is reached without a setup.
std::unordered_set<const BasicBlock *> waysToReturn(Function &f,
                                                    const std::unordered_set<const BasicBlock *> &setBlocks) {
  std::unordered_set<const BasicBlock *> toReturn;
  std::vector<BasicBlock *> work;
  for (BasicBlock &b : f)
    if (isa<ReturnInst>(b.getTerminator())) {
      toReturn.insert(&b);
      work.push_back(&b);
    }
  while (!work.empty()) {
    BasicBlock *b = work.back();
    work.pop_back();
    for (BasicBlock *p : predecessors(b))
      if (!setBlocks.count(p) && !testsFlag(p, setBlocks) && toReturn.insert(p).second) work.push_back(p);
  }
  return toReturn;
}

// A small entry of its own: when the part of the function that runs before any setup (`sets`) on its ways to a return
// reads memory but writes only the result (through the result's argument, unless it returns the result in registers)
// and its own locals, and can return, the function becomes that part, whose ways into a setup, or into code that
// returns only after one, call a copy of the whole function instead, with the same arguments (nothing the part did has
// been seen: the copy does it again), and is marked to be inlined (alwaysinline): its callers then take a leaf's early
// return in place, without a call (`fib`'s `n < 2`, `Ack`'s `m == 0`).
void splitLeafEntry(Function &f, const std::vector<BasicBlock *> &sets,
                    const std::unordered_set<const BasicBlock *> &unlinkBlocks) {
  if (sets.empty()) return;
  const Value *result = f.getReturnType()->isVoidTy() ? f.getArg(1) : nullptr;
  std::unordered_set<const BasicBlock *> setBlocks(sets.begin(), sets.end()), part;
  std::unordered_set<const BasicBlock *> toReturn = waysToReturn(f, setBlocks);
  if (!toReturn.count(&f.getEntryBlock())) return;
  std::vector<BasicBlock *> work{&f.getEntryBlock()};
  part.insert(&f.getEntryBlock());
  std::vector<std::pair<BasicBlock *, BasicBlock *>> leaves; // the part's edges to code that returns after a setup
  size_t size = 0;
  bool returns = false;
  while (!work.empty()) {
    BasicBlock *b = work.back();
    work.pop_back();
    for (Instruction &i : *b) {
      if (++size > kMaxLeafEntry) return;
      if (isa<ReturnInst>(i)) returns = true;
      if (isa<CallBase>(i) && !isa<IntrinsicInst>(i)) return;
      // What the part may write: the result, the function's own locals (the copy has its own).
      auto writable = [&](const Value *p) {
        const Value *o = getUnderlyingObject(p);
        return o == result || isa<AllocaInst>(o);
      };
      if (auto *m = dyn_cast<MemIntrinsic>(&i)) {
        if (!writable(m->getDest())) return;
      } else if (auto *st = dyn_cast<StoreInst>(&i)) {
        if (!writable(st->getPointerOperand())) return;
      } else if (i.mayWriteToMemory() && !isa<AllocaInst>(i)) {
        return;
      }
    }
    for (BasicBlock *next : successors(b)) { // a test of the flag is not on a way to a return: the part ends before
      if (unlinkBlocks.count(next)) continue; // the unlink of a frame linked, which the part never does
      if (!toReturn.count(next)) {
        if (std::find(leaves.begin(), leaves.end(), std::pair{b, next}) == leaves.end()) leaves.push_back({b, next});
      } else if (part.insert(next).second) {
        work.push_back(next);
      }
    }
  }
  if (!returns) return;
  ValueToValueMapTy vmap;
  Function *whole = CloneFunction(&f, vmap);
  whole->setName(f.getName() + ".whole");
  whole->setLinkage(GlobalValue::InternalLinkage);
  std::vector<Value *> args;
  for (Argument &a : f.args()) args.push_back(&a);
  auto callWhole = [&](BasicBlock *before) {
    BasicBlock *call = BasicBlock::Create(f.getContext(), "whole", &f, before);
    IRBuilder<> b(call);
    CallInst *c = b.CreateCall(whole, args);
    c->setCallingConv(whole->getCallingConv());
    c->setTailCall();
    if (f.getReturnType()->isVoidTy()) b.CreateRetVoid();
    else b.CreateRet(c);
    return call;
  };
  for (BasicBlock *set : sets) {
    std::vector<BasicBlock *> preds(pred_begin(set), pred_end(set));
    if (preds.empty()) continue;
    BasicBlock *call = callWhole(set);
    for (BasicBlock *p : preds) p->getTerminator()->replaceSuccessorWith(set, call);
  }
  for (auto [from, to] : leaves) {
    BasicBlock *call = callWhole(to);
    Instruction *t = from->getTerminator();
    for (unsigned k = 0; k < t->getNumSuccessors(); ++k)
      if (t->getSuccessor(k) == to) {
        to->removePredecessor(from, /*KeepOneInputPHIs=*/true);
        t->setSuccessor(k, call);
      }
  }
  removeUnreachableBlocks(f);
  f.removeFnAttr(Attribute::NoInline);
  f.addFnAttr(Attribute::AlwaysInline);
}

} // namespace

bool sinkFrameSetup(Function &f, FunctionAnalysisManager &) {
  if (f.isDeclaration()) return false;
  BasicBlock &entry = f.getEntryBlock();
  // The link: the frame (an alloca) stored into the context's chain, after its previous frame, an argument (a direct
  // entry's caller's frame).
  StoreInst *link = nullptr;
  AllocaInst *frame = nullptr;
  for (Instruction &i : entry)
    if (auto *s = dyn_cast<StoreInst>(&i))
      if (auto *a = dyn_cast<AllocaInst>(s->getValueOperand()); a && a->getParent() == &entry) {
        if (link) return false;
        link = s;
        frame = a;
      }
  if (!link) return false;
  std::unordered_set<const Value *> derived = derivedFrom(frame);
  const DataLayout &dl = f.getParent()->getDataLayout();
  Chain chain;
  chain.ctx = GetPointerBaseWithConstantOffset(link->getPointerOperand(), chain.offset, dl);
  if (!chain.ctx) return false;
  StoreInst *keep = nullptr; // the store of the caller's frame into the frame's link
  for (Instruction &i : entry)
    if (auto *s = dyn_cast<StoreInst>(&i); s && s->getPointerOperand() == frame) {
      chain.prev = s->getValueOperand();
      keep = s;
    }
  if (!chain.prev || !isa<Argument>(chain.prev)) return false;

  // The stack's test: the entry's branch on a comparison of the frame's address, to a block that calls the runtime
  // (xb_stack_deep) and goes on or to one that raises the error and leaves.
  auto *test = condBranch(entry.getTerminator());
  if (!test) return false;
  // The side of the test that calls the runtime (the simplification may have turned the condition around).
  auto callsRuntime = [](const BasicBlock *b) {
    return std::any_of(b->begin(), b->end(), [](const Instruction &i) {
      return isa<CallBase>(i) && !isa<IntrinsicInst>(i);
    });
  };
  unsigned belowSide = callsRuntime(test->getSuccessor(0)) ? 0 : 1;
  BasicBlock *below = test->getSuccessor(belowSide), *join = test->getSuccessor(1 - belowSide);
  auto *belowBr = condBranch(below->getTerminator());
  if (below == join || !callsRuntime(below) || !below->getSinglePredecessor() || !belowBr ||
      (belowBr->getSuccessor(0) != join && belowBr->getSuccessor(1) != join) || join->hasNPredecessorsOrMore(3))
    return false;
  BasicBlock *raise = belowBr->getSuccessor(0) == join ? belowBr->getSuccessor(1) : belowBr->getSuccessor(0);
  Instruction *raiseBr = raise->getTerminator();
  if (raise == join || !raise->getSinglePredecessor() || !isUncondBranch(raiseBr)) return false;
  BasicBlock *leave = raiseBr->getSuccessor(0);
  for (PHINode &phi : leave->phis()) // the values the error's way out brings must be known everywhere
    if (auto *i = dyn_cast<Instruction>(phi.getIncomingValueForBlock(raise)); i && i->getParent() != &entry)
      return false;
  for (Instruction &i : *below)
    for (const Value *op : i.operands())
      if (auto *d = dyn_cast<Instruction>(op); d && d->getParent() != below && d->getParent() != &entry) return false;

  // The setup: the entry's writes of the frame, the link, the test's condition, and what only they use.
  std::unordered_set<Instruction *> setup{link};
  for (Instruction &i : entry)
    if (auto *s = dyn_cast<StoreInst>(&i); s && derived.count(s->getPointerOperand())) setup.insert(&i);
    else if (auto *m = dyn_cast<MemSetInst>(&i); m && derived.count(m->getDest())) setup.insert(&i);
  std::vector<Instruction *> work(setup.begin(), setup.end());
  if (auto *c = dyn_cast<Instruction>(test->getCondition())) {
    if (!c->hasOneUse()) return false; // the test's alone, as it goes with the setup
    work.push_back(c);
    setup.insert(c);
  }
  while (!work.empty()) {
    Instruction *i = work.back();
    work.pop_back();
    for (Value *op : i->operands()) {
      auto *d = dyn_cast<Instruction>(op);
      if (!d || d->getParent() != &entry || setup.count(d) || isa<AllocaInst>(d) || isa<PHINode>(d)) continue;
      bool onlySetup = true;
      for (User *u : d->users())
        onlySetup &= setup.count(cast<Instruction>(u)) != 0;
      if (!onlySetup) continue;
      setup.insert(d);
      work.push_back(d);
    }
  }
  // The first instruction that needs the frame in each block (the entry's setup and the stack's blocks aside), but in
  // a block another such block dominates (the setup there is done already). An entry that needs it itself (a mark of
  // PRIVATEs, a call) keeps it: no path could do without.
  std::vector<Instruction *> needs;
  DominatorTree dt(f);
  for (BasicBlock &b : f) {
    if (&b == below || &b == raise) continue;
    for (Instruction &i : b)
      if (&b == &entry && !setup.count(&i) && &i != test && needsFrame(i, derived, chain, dl)) return false;
    for (Instruction &i : b)
      if (!setup.count(&i) && &i != test && needsFrame(i, derived, chain, dl)) {
        needs.push_back(&i);
        break;
      }
  }
  std::vector<Instruction *> first;
  for (Instruction *n : needs)
    if (std::none_of(needs.begin(), needs.end(), [&](Instruction *o) {
          return o != n && dt.dominates(o->getParent(), n->getParent()) && o->getParent() != n->getParent();
        }))
      first.push_back(n);
  needs.swap(first);

  // The flag, false on entry.
  IRBuilder<> eb(&entry, entry.getFirstInsertionPt());
  AllocaInst *linked = eb.CreateAlloca(eb.getInt1Ty(), nullptr, "frame.linked");
  eb.SetInsertPoint(test);
  eb.CreateStore(eb.getFalse(), linked);
  std::vector<Instruction *> order; // the setup in the entry's order
  for (Instruction &i : entry)
    if (setup.count(&i)) order.push_back(&i);

  std::vector<BasicBlock *> sets;
  for (Instruction *need : needs) {
    BasicBlock *at = need->getParent();
    BasicBlock *rest = SplitBlock(at, need);
    BasicBlock *set = BasicBlock::Create(f.getContext(), "frame.setup", &f, rest);
    sets.push_back(set);
    at->getTerminator()->eraseFromParent();
    IRBuilder<> b(at);
    b.CreateCondBr(b.CreateLoad(b.getInt1Ty(), linked, "frame.is.linked"), rest, set);
    ValueToValueMapTy vmap;
    b.SetInsertPoint(set);
    for (Instruction *i : order) {
      Instruction *c = i->clone();
      RemapInstruction(c, vmap, RF_IgnoreMissingLocals);
      b.Insert(c);
      vmap[i] = c;
    }
    b.CreateStore(b.getTrue(), linked);
    BasicBlock *belowCopy = CloneBasicBlock(below, vmap, ".frame", &f);
    BasicBlock *raiseCopy = CloneBasicBlock(raise, vmap, ".frame", &f);
    for (Instruction &i : *belowCopy) RemapInstruction(&i, vmap, RF_IgnoreMissingLocals);
    for (Instruction &i : *raiseCopy) RemapInstruction(&i, vmap, RF_IgnoreMissingLocals);
    auto *bt = cast<CondBranch>(belowCopy->getTerminator());
    for (unsigned k = 0; k < bt->getNumSuccessors(); ++k)
      bt->setSuccessor(k, bt->getSuccessor(k) == join ? rest : raiseCopy);
    for (PHINode &phi : leave->phis()) phi.addIncoming(phi.getIncomingValueForBlock(raise), raiseCopy);
    CondBranch *check = b.CreateCondBr(vmap[test->getCondition()], belowCopy, rest);
    if (belowSide) check->swapSuccessors();
    check->setMetadata(LLVMContext::MD_prof, test->getMetadata(LLVMContext::MD_prof));
  }

  // The entry goes on without the setup or the test.
  MDNode *linkTbaa = keep->getMetadata(LLVMContext::MD_tbaa); // the frame's link's memory class, for the unlink below
  createBranch(join, test->getIterator());
  below->removePredecessor(&entry);
  test->eraseFromParent();
  for (auto it = order.rbegin(); it != order.rend(); ++it) (*it)->eraseFromParent();
  for (PHINode &phi : leave->phis()) phi.removeIncomingValue(raise, /*DeletePHIIfEmpty=*/false);
  join->removePredecessor(below);
  below->eraseFromParent();
  raise->eraseFromParent();

  // When some way returns without a setup (an early return), only a frame linked is unlinked: that way leaves the chain
  // as it found it and needs nothing kept for the way out (no register saved, no stack), and the caller's frame is read
  // back from the frame's link: no register keeps it meanwhile. (Where every way sets the frame up, the flag would
  // only stay live to the exit, through the loops.)
  std::vector<StoreInst *> unlinks;
  std::unordered_set<const BasicBlock *> setBlocks(sets.begin(), sets.end());
  if (waysToReturn(f, setBlocks).count(&entry))
    for (BasicBlock &b : f)
      for (Instruction &i : b)
        if (auto *s = dyn_cast<StoreInst>(&i); s && s->getValueOperand() == chain.prev) {
          int64_t offset = 0;
          if (GetPointerBaseWithConstantOffset(s->getPointerOperand(), offset, dl) == chain.ctx &&
              offset == chain.offset)
            unlinks.push_back(s);
        }
  std::unordered_set<const BasicBlock *> unlinkBlocks;
  for (StoreInst *s : unlinks) {
    IRBuilder<> b(s);
    Instruction *then = SplitBlockAndInsertIfThen(b.CreateLoad(b.getInt1Ty(), linked, "frame.is.linked"),
                                                  s->getIterator(), /*Unreachable=*/false);
    s->moveBefore(then->getIterator());
    b.SetInsertPoint(s);
    LoadInst *prev = b.CreateLoad(chain.prev->getType(), frame, "prev");
    prev->setMetadata(LLVMContext::MD_tbaa, linkTbaa);
    s->setOperand(0, prev);
    unlinkBlocks.insert(then->getParent());
  }

  splitLeafEntry(f, sets, unlinkBlocks);
  return true;
}

} // namespace xb
