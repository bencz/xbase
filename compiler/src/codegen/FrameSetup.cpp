// A function's frame set up where it is needed, not on entry (see FrameSetup.h).
#include "codegen/FrameSetup.h"

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

// A small entry of its own: when the part of the function that runs before any setup (`sets`) reads memory but writes
// only the result (through the result's argument) and can return, the function becomes that part, whose ways into a
// setup call a copy of the whole function instead, with the same arguments (nothing the part did has been seen: the
// copy does it again), and is marked to be inlined (alwaysinline): its callers then take a leaf's early return in
// place, without a call (`fib`'s `n < 2`).
void splitLeafEntry(Function &f, const std::vector<BasicBlock *> &sets, const Chain &chain) {
  if (sets.empty() || f.arg_size() < 3) return;
  const Value *result = f.getArg(1);
  std::unordered_set<const BasicBlock *> setBlocks(sets.begin(), sets.end()), part;
  std::vector<BasicBlock *> work{&f.getEntryBlock()};
  part.insert(&f.getEntryBlock());
  size_t size = 0;
  bool returns = false;
  while (!work.empty()) {
    BasicBlock *b = work.back();
    work.pop_back();
    for (Instruction &i : *b) {
      if (++size > kMaxLeafEntry) return;
      if (isa<ReturnInst>(i)) returns = true;
      if (isa<CallBase>(i) && !isa<IntrinsicInst>(i)) return;
      // What the part may write: the result, the function's own locals (the copy has its own), the chain's unlink.
      auto writable = [&](const Value *p) {
        const Value *o = getUnderlyingObject(p);
        return o == result || isa<AllocaInst>(o);
      };
      if (auto *m = dyn_cast<MemIntrinsic>(&i)) {
        if (!writable(m->getDest())) return;
      } else if (auto *st = dyn_cast<StoreInst>(&i)) {
        if (!writable(st->getPointerOperand()) && st->getValueOperand() != chain.prev) return;
      } else if (i.mayWriteToMemory() && !isa<AllocaInst>(i)) {
        return;
      }
    }
    // At a test of the flag, false in the part, only the setup's side is taken: the part ends there.
    bool test = any_of(successors(b), [&](BasicBlock *n) { return setBlocks.count(n) != 0; });
    for (BasicBlock *next : successors(b))
      if (!test && part.insert(next).second) work.push_back(next);
  }
  if (!returns) return;
  ValueToValueMapTy vmap;
  Function *whole = CloneFunction(&f, vmap);
  whole->setName(f.getName() + ".whole");
  whole->setLinkage(GlobalValue::InternalLinkage);
  std::vector<Value *> args;
  for (Argument &a : f.args()) args.push_back(&a);
  for (BasicBlock *set : sets) {
    std::vector<BasicBlock *> preds(pred_begin(set), pred_end(set));
    if (preds.empty()) continue;
    BasicBlock *call = BasicBlock::Create(f.getContext(), "whole", &f, set);
    IRBuilder<> b(call);
    CallInst *c = b.CreateCall(whole, args);
    c->setCallingConv(whole->getCallingConv());
    c->setTailCall();
    b.CreateRetVoid();
    for (BasicBlock *p : preds) p->getTerminator()->replaceSuccessorWith(set, call);
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
  for (Instruction &i : entry)
    if (auto *s = dyn_cast<StoreInst>(&i); s && s->getPointerOperand() == frame) chain.prev = s->getValueOperand();
  if (!chain.prev || !isa<Argument>(chain.prev)) return false;

  // The stack's test: the entry's branch on a comparison of the frame's address, to a block that calls the runtime
  // (xb_stack_deep) and goes on or to one that raises the error and leaves.
  auto *test = dyn_cast<BranchInst>(entry.getTerminator());
  if (!test || !test->isConditional()) return false;
  // The side of the test that calls the runtime (the simplification may have turned the condition around).
  auto callsRuntime = [](const BasicBlock *b) {
    return std::any_of(b->begin(), b->end(), [](const Instruction &i) {
      return isa<CallBase>(i) && !isa<IntrinsicInst>(i);
    });
  };
  unsigned belowSide = callsRuntime(test->getSuccessor(0)) ? 0 : 1;
  BasicBlock *below = test->getSuccessor(belowSide), *join = test->getSuccessor(1 - belowSide);
  auto *belowBr = dyn_cast<BranchInst>(below->getTerminator());
  if (below == join || !callsRuntime(below) || !below->getSinglePredecessor() || !belowBr ||
      !belowBr->isConditional() || (belowBr->getSuccessor(0) != join && belowBr->getSuccessor(1) != join) ||
      join->hasNPredecessorsOrMore(3))
    return false;
  BasicBlock *raise = belowBr->getSuccessor(0) == join ? belowBr->getSuccessor(1) : belowBr->getSuccessor(0);
  auto *raiseBr = dyn_cast<BranchInst>(raise->getTerminator());
  if (raise == join || !raise->getSinglePredecessor() || !raiseBr || raiseBr->isConditional()) return false;
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
    auto *bt = cast<BranchInst>(belowCopy->getTerminator());
    for (unsigned k = 0; k < bt->getNumSuccessors(); ++k)
      bt->setSuccessor(k, bt->getSuccessor(k) == join ? rest : raiseCopy);
    for (PHINode &phi : leave->phis()) phi.addIncoming(phi.getIncomingValueForBlock(raise), raiseCopy);
    BranchInst *check = b.CreateCondBr(vmap[test->getCondition()], belowCopy, rest);
    if (belowSide) check->swapSuccessors();
    check->setMetadata(LLVMContext::MD_prof, test->getMetadata(LLVMContext::MD_prof));
  }

  // The entry goes on without the setup or the test.
  BranchInst::Create(join, test->getIterator());
  below->removePredecessor(&entry);
  test->eraseFromParent();
  for (auto it = order.rbegin(); it != order.rend(); ++it) (*it)->eraseFromParent();
  for (PHINode &phi : leave->phis()) phi.removeIncomingValue(raise, /*DeletePHIIfEmpty=*/false);
  join->removePredecessor(below);
  below->eraseFromParent();
  raise->eraseFromParent();
  splitLeafEntry(f, sets, chain);
  return true;
}

} // namespace xb
