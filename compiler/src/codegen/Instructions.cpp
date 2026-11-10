// xIR instructions to LLVM: each operation calls the runtime on the items the
// frame plan placed, and tests the pending flag after operations that can fail.
#include "codegen/CodeGen.h"

#include "common/BitFunctions.h"

#include "xbrt.h" // constants only (type tags)

#include <llvm/IR/Constants.h>
#include <llvm/IR/MDBuilder.h>

using namespace llvm;

namespace xb {

// The address of an item value.
Value *CodeGen::addr(const xir::Value *v) {
  const Home &h = plan_->home(v);
  switch (h.kind) {
  case Home::Kind::Constant: return literal(static_cast<const xir::Constant &>(*v));
  case Home::Kind::Slot: {
    Value *a = slot(h.slot);
    if (auto w = words_.find(v); w != words_.end()) wordsAt_[a] = w->second;
    return a;
  }
  case Home::Kind::Place: {
    Value *a = values_.at(v);
    if (auto w = words_.find(v); w != words_.end()) wordsAt_[a] = w->second;
    return a;
  }
  case Home::Kind::Ret: return fnRet_;
  case Home::Kind::Local: {
    AllocaInst *&a = locals_[v];
    if (!a) {
      a = entryAlloca("local");
      localItems_.insert(a);
    }
    return a;
  }
  case Home::Kind::None: break;
  }
  report_fatal_error("xbc: an item value has no home");
}

AllocaInst *CodeGen::entryAlloca(const char *name) {
  IRBuilder<> entry(&fn_->getEntryBlock(), fn_->getEntryBlock().begin());
  AllocaInst *a = entry.CreateAlloca(itemTy_, nullptr, name);
  a->setAlignment(itemAlign_);
  return a;
}

// Scratch item `k` of the function: distinct per operand of one call; the
// same items serve every call, each only around its call.
Value *CodeGen::scratchItem(unsigned k) {
  while (scratch_.size() <= k) scratch_.push_back(entryAlloca("scratch"));
  return scratch_[k];
}

// Runtime calls never receive the address of a Local home: LLVM keeps such an
// item in registers only while nothing but loads and stores reach it. An
// operand goes through scratch item `k`.
Value *CodeGen::callArg(Value *item, unsigned k) {
  if (!localItems_.count(item)) return item;
  Value *s = scratchItem(k);
  copyItem(s, item);
  return s;
}

// The item a runtime call writes: a Local home receives it through scratch
// item `k`, copied back by callDone right after the call.
CodeGen::CallResult CodeGen::callResult(Value *item, unsigned k) {
  if (!localItems_.count(item)) return {item, nullptr};
  return {scratchItem(k), item};
}

void CodeGen::callDone(const CallResult &r) {
  if (r.local) copyItem(r.local, r.arg);
}

// fn( ctx, result, operands... ), the shape of the runtime's operators.
void CodeGen::callItemOp(FunctionCallee fn, Value *result, std::initializer_list<Value *> operands) {
  std::vector<Value *> args{fnCtx_, nullptr};
  unsigned k = 1;
  for (Value *op : operands) args.push_back(callArg(op, k++));
  CallResult r = callResult(result, 0);
  args[1] = r.arg;
  b_.CreateCall(fn, args);
  callDone(r);
}

// Where an instruction's item result goes: its home, or for a value computed
// into a variable, the variable's item (reached now, before the operation).
Value *CodeGen::dest(const xir::Instruction &i) {
  const Home &h = plan_->home(&i);
  if (h.kind == Home::Kind::Place) {
    const xir::Instruction &store = *h.place;
    Value *p = varAddress(*store.var, store.access, unwind_);
    values_[&i] = p;
    return p;
  }
  return addr(&i);
}

// A machine value (Bool, I64).
Value *CodeGen::value(const xir::Value *v) {
  if (v->kind() == xir::Value::Kind::Constant) {
    auto &c = static_cast<const xir::Constant &>(*v);
    if (c.lit == xir::Constant::Lit::Bool) return b_.getInt1(c.logical);
    return ConstantInt::get(i64_, (uint64_t)c.ival, true);
  }
  return values_.at(v);
}

// The operands of a call or array literal, in consecutive slots: those not
// already computed there are copied in.
Value *CodeGen::operandGroup(const xir::Instruction &i) {
  const auto &ops = i.operands();
  if (ops.empty()) return ConstantPointerNull::get(ptrTy_);
  uint32_t base = plan_->groupBase(&i);
  for (size_t k = 0; k < ops.size(); ++k)
    if (ops[k]->type() == xir::Type::Item && !plan_->inGroup(&i, k)) { // args.rest: filled by withRest
      Value *src = addr(ops[k]);
      copyItem(slot(base + (uint32_t)k), src);
    }
  return slot(base);
}

// A list with `...` (args.rest operands), for `emit` to use as its count and
// items: the operands, with the arguments the function received past its
// parameters in the place of each args.rest. They go into a frame of their own
// on the stack, sized at run time and linked into the chain while `emit` runs,
// so the collector sees them (a callee may replace them, PValue()).
// xb_args_rest copies the arguments passed on; one whose reference is gone
// unwinds instead. A call of only `...` may pass them in place
// (forwardInPlace_).
void CodeGen::withRest(const xir::Instruction &i, Value *group, function_ref<void(Value *, Value *)> emit) {
  const auto &ops = i.operands();
  uint32_t rests = 0;
  for (const xir::Value *v : ops) rests += v->type() == xir::Type::Args;
  Value *argc = fn_->getArg(2), *argv = fn_->getArg(3);
  Value *params = ConstantInt::get(i32_, fnParams_);
  Value *rest = b_.CreateSelect(b_.CreateICmpUGT(argc, params), b_.CreateSub(argc, params), ConstantInt::get(i32_, 0),
                                "rest");
  Value *restItems = b_.CreateGEP(itemTy_, argv, params, "rest.items"); // not dereferenced when there are none
  if (forwardInPlace_ && i.op == xir::Op::Call && ops.size() == 1 && i.restRefs == UINT32_MAX) {
    // f( ... ): the callee receives the arguments where they are, which nothing here reads again
    emit(rest, restItems);
    return;
  }
  Value *total = b_.CreateAdd(b_.CreateMul(rest, ConstantInt::get(i32_, rests)),
                              ConstantInt::get(i32_, (uint32_t)ops.size() - rests), "total");
  Value *save = b_.CreateStackSave();
  StructType *ty = frameType(0);
  uint64_t header = dl_.getStructLayout(ty)->getElementOffset(FrameSlots);
  Value *itemBytes = b_.CreateMul(b_.CreateZExt(total, intPtrTy_), ConstantInt::get(intPtrTy_, itemSize_));
  AllocaInst *frame = b_.CreateAlloca(i8_, b_.CreateAdd(itemBytes, ConstantInt::get(intPtrTy_, header)), "list.frame");
  frame->setAlignment(std::max(itemAlign_, dl_.getPointerABIAlignment(0)));
  Value *items = b_.CreateStructGEP(ty, frame, FrameSlots, "list.items");
  b_.CreateMemSet(items, b_.getInt8(0), itemBytes, itemAlign_); // NIL until filled
  linkFrame(frame, total, ConstantPointerNull::get(ptrTy_));
  auto unlink = [&] {
    unlinkFrame(frame);
    b_.CreateStackRestore(save);
  };
  BasicBlock *failed = newBlock("list.failed");
  Value *pos = ConstantInt::get(i32_, 0);
  for (size_t k = 0; k < ops.size(); ++k) {
    Value *at = b_.CreateInBoundsGEP(itemTy_, items, pos);
    if (ops[k]->type() != xir::Type::Args) {
      copyItem(at, b_.CreateConstInBoundsGEP1_32(itemTy_, group, (uint32_t)k));
      pos = b_.CreateAdd(pos, ConstantInt::get(i32_, 1));
      continue;
    }
    Value *ok = b_.CreateCall(rtArgsRest_, {fnCtx_, at, restItems, rest, pos, ConstantInt::get(i32_, i.restRefs)});
    BasicBlock *next = newBlock("list.rest");
    b_.CreateCondBr(b_.CreateICmpNE(ok, ConstantInt::get(i32_, 0)), next, failed);
    b_.SetInsertPoint(next);
    pos = b_.CreateAdd(pos, rest);
  }
  emit(total, items);
  unlink();
  BasicBlock *done = b_.GetInsertBlock();
  b_.SetInsertPoint(failed);
  unlink();
  b_.CreateBr(unwind_);
  b_.SetInsertPoint(done);
}

// A call to a function of the module with a direct entry: each parameter's
// item as two words in registers (NIL for one not passed; arguments past the
// parameters are evaluated and left out, as nothing reads them).
void CodeGen::emitDirectCall(const xir::Instruction &i, Value *d) {
  const xir::Function &callee = *i.callee.function;
  std::vector<Value *> args{fnCtx_, d, fnFrame_}; // the chain's frame at a call is the caller's
  for (uint32_t k = 0; k < callee.numParams; ++k) {
    if (k >= i.operands().size()) {
      args.insert(args.end(), {headerWord(XB_T_NIL, 0, 0), ConstantInt::get(i64_, 0)});
      continue;
    }
    Value *item = addr(i.operand(k));
    args.push_back(loadHeader(item));        // the cached header when there is one
    args.push_back(loadWords(item).payload); // a pointer is read where it is
  }
  setNil(d);
  b_.CreateCall(direct_.at(&callee), args);
  checkPending(unwind_);
}

// .T. or .F.; any other value is an error (the function leaves). `type` is
// the item's type (a constant when it is known).
Value *CodeGen::testTruth(Value *item, BasicBlock *unwind, Value *type) {
  BasicBlock *ok = newBlock("cond.ok");
  BasicBlock *bad = newBlock("cond.bad");
  MDBuilder md(ctx_);
  b_.CreateCondBr(b_.CreateICmpEQ(type, ConstantInt::get(i32_, XB_T_LOGICAL)), ok, bad, md.createLikelyBranchWeights());
  b_.SetInsertPoint(bad);
  b_.CreateCall(rtCondError_, {fnCtx_, callArg(item, 0)});
  b_.CreateBr(unwind);
  b_.SetInsertPoint(ok);
  return logicalValue(item);
}

// The boolean of a logical item: its i32 starts the value word (read whole,
// like the header).
Value *CodeGen::logicalValue(Value *item) {
  Value *v = b_.CreateTrunc(b_.CreateLShr(intValue(item), valueShift(32)), i32_, "lval");
  return b_.CreateICmpNE(v, ConstantInt::get(i32_, 0));
}

// The arguments of a branch: item parameters get their values copied into
// their homes (all sources read before any destination is written, when a
// destination is also a source), machine parameters get PHI incomings.
void CodeGen::emitBranch(const xir::Target &t) {
  std::vector<std::pair<const xir::BlockParam *, const xir::Value *>> copies; // parameter, argument
  for (size_t k = 0; k < t.args.size(); ++k) {
    const xir::BlockParam *p = t.block->params[k].get();
    const xir::Value *arg = t.args[k];
    if (p->type() != xir::Type::Item) {
      cast<PHINode>(values_.at(p))->addIncoming(value(arg), b_.GetInsertBlock());
      continue;
    }
    const Home &dst = plan_->home(p), &src = plan_->home(arg);
    if (arg == p || (dst.kind == Home::Kind::Slot && src.kind == Home::Kind::Slot && src.slot == dst.slot))
      continue; // computed in place
    copies.push_back({p, arg});
  }
  // A source that is another copy's destination: the same Local value, or the same slot.
  auto sameHome = [&](const xir::Value *a, const xir::BlockParam *p) {
    const Home &ha = plan_->home(a), &hp = plan_->home(p);
    return a == p || (ha.kind == Home::Kind::Slot && hp.kind == Home::Kind::Slot && ha.slot == hp.slot);
  };
  bool overlap = false;
  for (size_t d = 0; d < copies.size(); ++d)
    for (size_t s = 0; s < copies.size(); ++s) overlap |= s != d && sameHome(copies[s].second, copies[d].first);
  if (!overlap) {
    for (auto &[p, arg] : copies) {
      Value *src = addr(arg);
      copyItem(addr(p), src);
    }
    return;
  }
  // Nothing allocates between these copies, so the staging buffer need not be
  // a GC root.
  IRBuilder<> entry(&fn_->getEntryBlock(), fn_->getEntryBlock().begin());
  Value *stage = entry.CreateAlloca(itemTy_, ConstantInt::get(i32_, copies.size()), "stage");
  for (size_t k = 0; k < copies.size(); ++k) {
    Value *src = addr(copies[k].second);
    copyItem(b_.CreateConstInBoundsGEP1_32(itemTy_, stage, (unsigned)k), src);
  }
  for (size_t k = 0; k < copies.size(); ++k) {
    Value *src = b_.CreateConstInBoundsGEP1_32(itemTy_, stage, (unsigned)k);
    copyItem(addr(copies[k].first), src);
  }
}

void CodeGen::emit(const xir::Instruction &i) {
  using xir::Op;
  const auto &ops = i.operands();
  switch (i.op) {
  case Op::NumDefaultDec: {
    CallResult r = callResult(dest(i), 0);
    b_.CreateCall(rtNumDefaultDec_, {fnCtx_, r.arg, ConstantFP::get(Type::getDoubleTy(ctx_), i.imm)});
    callDone(r);
    break;
  }
  case Op::Load: {
    Value *p = varAddress(*i.var, i.access, unwind_);
    if (sharingLoads_.count(&i)) shareString(p); // the value leaves the variable: its string stops growing
    const Home &h = plan_->home(&i);
    if (h.kind == Home::Kind::Place && h.place == &i) values_[&i] = p; // the variable's own item serves the use
    else copyItem(addr(&i), p);
    break;
  }
  case Op::Store:
    if (!plan_->forwarded(&i)) {
      Value *src = addr(ops[0]);
      copyItem(varAddress(*i.var, i.access, unwind_), src);
    }
    break;
  case Op::Append: emitAppend(i); break;
  case Op::Barrier: writeBarrier(*i.var, i.access); break;
  case Op::Unary: {
    if (const xb_bit_function *f = bitOperator(i.unop)) {
      emitBitOperator(i, *f);
      break;
    }
    if (i.unop == UnOp::Neg) {
      emitNeg(i);
      break;
    }
    Value *a = addr(ops[0]);
    callItemOp(rtNot_, dest(i), {a});
    checkPending(unwind_);
    break;
  }
  case Op::Binary: emitBinary(i); break;
  case Op::Truth: {
    auto fused = fusedTruth_.find(&i);
    if (fused != fusedTruth_.end()) {
      values_[&i] = fused->second;
    } else {
      Value *item = addr(ops[0]);
      values_[&i] = testTruth(item, unwind_, typeOf(ops[0], item));
    }
    break;
  }
  case Op::FromBool: storeLogical(dest(i), value(ops[0])); break;
  case Op::ArgsRest: break; // read by the list that uses it (withRest)
  case Op::Call: {
    if (emitBitCall(i) || emitInlineCall(i)) break;
    if (xir::isDirectCall(i)) {
      emitDirectCall(i, dest(i));
      break;
    }
    Value *argv = operandGroup(i);
    Function *callee = i.callee.function ? functions_.at(i.callee.function) : externFunction(i.callee);
    Value *d = dest(i);
    setNil(d); // the caller clears the result, so callees that return nothing need not
    auto call = [&](Value *n, Value *items) { b_.CreateCall(xbFuncTy_, callee, {fnCtx_, d, n, items}); };
    if (i.hasRest()) withRest(i, argv, call);
    else call(ConstantInt::get(i32_, ops.size()), argv);
    checkPending(unwind_);
    break;
  }
  case Op::Send: {
    Value *argv = operandGroup(i);
    Value *d = dest(i);
    setNil(d);
    auto send = [&](Value *n, Value *items) {
      b_.CreateCall(rtSend_, {fnCtx_, d, messageName(i.message), n, items});
    };
    if (i.hasRest()) withRest(i, argv, send);
    else send(ConstantInt::get(i32_, ops.size()), argv);
    checkPending(unwind_);
    break;
  }
  case Op::HashLit: {
    Value *pairs = operandGroup(i);
    b_.CreateCall(rtHashNew_, {fnCtx_, dest(i), ConstantInt::get(i32_, ops.size() / 2), pairs});
    checkPending(unwind_);
    break;
  }
  case Op::ArrayLit: {
    Value *elems = operandGroup(i);
    Value *d = dest(i);
    auto make = [&](Value *n, Value *items) { b_.CreateCall(rtArrayNew_, {fnCtx_, d, n, items}); };
    if (i.hasRest()) withRest(i, elems, make);
    else make(ConstantInt::get(i32_, ops.size()), elems);
    break;
  }
  case Op::Index: emitIndex(i); break;
  case Op::IndexSet: emitIndexSet(i); break;
  case Op::ParamsRef: {
    Value *any = b_.getFalse();
    for (const xir::Var *p : i.captures) any = b_.CreateOr(any, holdsRef(rawAddress(*p)));
    values_[&i] = any;
    break;
  }
  case Op::CallForward: {
    Value *d = dest(i);
    setNil(d);
    // The other body shows on the call stack as this function: this frame no longer does.
    tag(b_.CreateStore(ConstantPointerNull::get(ptrTy_), b_.CreateStructGEP(frameType(0), fnFrame_, FrameProc)),
        Mem::FrameLink);
    // LLVM inlines the other body (this is its only call): its frame then adds
    // to every call's stack, but the common path's prologue does not prepare
    // a call, which measured faster (fib: 261 ms against 299 ms).
    // A direct body passes its parameters' slots: the arguments as received, those past the parameters left out
    // (neither body reads them: they read no arguments as received).
    Value *argc = fnDirect_ ? static_cast<Value *>(ConstantInt::get(i32_, fnParams_)) : fn_->getArg(2);
    Value *argv = fnDirect_ ? fnBase_ : fn_->getArg(3);
    b_.CreateCall(xbFuncTy_, functions_.at(i.callee.function), {fnCtx_, d, argc, argv});
    checkPending(unwind_);
    break;
  }
  case Op::MakeBlock: {
    // The cells first (a variable moving into its cell changes its slot, which
    // the block's home may be), then the block, which receives them: nothing
    // can allocate between its creation and the last cell store.
    std::vector<Value *> cells;
    for (const xir::Var *c : i.captures)
      cells.push_back(c->kind == xir::Var::Kind::Capture ? capturedCell(*c)
                                                          : b_.CreateCall(rtCellOf_, {fnCtx_, rawAddress(*c)}, "cell"));
    Value *d = dest(i);
    Value *block = b_.CreateCall(rtBlockNew_, {fnCtx_, d, functions_.at(i.blockFn),
                                               ConstantInt::get(i32_, i.captures.size())}, "block");
    for (uint32_t k = 0; k < cells.size(); ++k)
      tag(b_.CreateStore(cells[k], capturedCellAddress(block, k)), Mem::BlockCells);
    break;
  }
  case Op::ForTest: emitForTest(i); break;
  case Op::EnumLen:
    values_[&i] = b_.CreateCall(rtEnumLen_, {fnCtx_, callArg(addr(ops[0]), 0)});
    checkPending(unwind_);
    break;
  case Op::EachInCell: {
    Value *shared = b_.CreateCall(rtEachShared_, {fnCtx_, rawAddress(*i.var)}, "shared");
    values_[&i] = b_.CreateICmpNE(shared, ConstantInt::get(i32_, 0));
    break;
  }
  case Op::EachEnter: {
    Value *inCell = b_.CreateZExt(value(ops[0]), i32_);
    b_.CreateCall(rtEachEnter_, {fnCtx_, dest(i), rawAddress(*i.var), inCell, callArg(addr(ops[1]), 0)});
    break;
  }
  case Op::EachMove: {
    Value *inCell = b_.CreateZExt(value(ops[0]), i32_);
    b_.CreateCall(rtEachMove_, {fnCtx_, rawAddress(*i.var), inCell, value(ops[1])});
    checkPending(unwind_);
    break;
  }
  case Op::EachLeave: {
    Value *inCell = b_.CreateZExt(value(ops[0]), i32_);
    b_.CreateCall(rtEachLeave_, {rawAddress(*i.var), inCell, callArg(addr(ops[1]), 0)});
    break;
  }
  case Op::EachKey: {
    CallResult r = callResult(dest(i), 0);
    b_.CreateCall(rtEachKey_, {fnCtx_, r.arg, callArg(addr(ops[0]), 1), value(ops[1])});
    callDone(r);
    checkPending(unwind_);
    break;
  }
  case Op::RefVar: refOf(*i.var, dest(i)); break;
  case Op::RefIndex:
    b_.CreateCall(rtRefElem_, {fnCtx_, dest(i), callArg(addr(ops[0]), 0), callArg(addr(ops[1]), 1)});
    checkPending(unwind_);
    break;
  case Op::SeqCatch:
    values_[&i] = b_.CreateICmpNE(b_.CreateCall(rtSeqCatch_, {fnCtx_}), ConstantInt::get(i32_, 0));
    break;
  case Op::SeqValue: callItemOp(rtSeqValue_, dest(i), {}); break;
  case Op::SubstCheck: { // the tag as the item holds it: a substituted value may have any type
    Value *bit = b_.CreateShl(ConstantInt::get(i32_, 1), itemType(addr(ops[0])));
    Value *own = ConstantInt::get(i32_, plan_->types().of(ops[0]));
    values_[&i] = b_.CreateICmpEQ(b_.CreateAnd(bit, own), ConstantInt::get(i32_, 0));
    break;
  }
  case Op::SubstValue: copyItem(dest(i), addr(ops[0])); break;
  case Op::SwitchCheck:
    callItemOp(rtSwitchCheck_, dest(i), {addr(ops[0])});
    checkPending(unwind_);
    break;
  case Op::SwitchMatch: {
    Value *m = b_.CreateCall(rtSwitchMatch_, {callArg(addr(ops[0]), 0), callArg(addr(ops[1]), 1)});
    values_[&i] = b_.CreateICmpNE(m, ConstantInt::get(i32_, 0));
    break;
  }
  case Op::IntAdd: values_[&i] = b_.CreateAdd(value(ops[0]), value(ops[1])); break;
  case Op::IntSub: values_[&i] = b_.CreateSub(value(ops[0]), value(ops[1])); break;
  case Op::IntCmp: values_[&i] = b_.CreateICmp(llvmPredicate(i.pred), value(ops[0]), value(ops[1])); break;
  case Op::BoolAnd: values_[&i] = b_.CreateAnd(value(ops[0]), value(ops[1])); break;
  case Op::BoolNot: values_[&i] = b_.CreateNot(value(ops[0])); break;
  case Op::Br:
    shareOnEdge(i.parent, i.targets[0].block);
    emitBranch(i.targets[0]);
    b_.CreateBr(blocks_.at(i.targets[0].block));
    break;
  case Op::CondBr: {
    // A target with arguments, or whose edge ends a string's growth, gets an edge block of its own.
    BasicBlock *dests[2];
    bool own[2];
    for (size_t k = 0; k < 2; ++k) {
      own[k] = !i.targets[k].args.empty() || shareOnEdge_.count({i.parent, i.targets[k].block});
      dests[k] = own[k] ? newBlock("edge") : blocks_.at(i.targets[k].block);
    }
    const auto *cond = ops[0]->kind() == xir::Value::Kind::Instruction ? static_cast<const xir::Instruction *>(ops[0])
                                                                       : nullptr;
    if (cond && cond->op == xir::Op::SubstCheck) // a substituted value of another type: rare
      branchLikely(b_.CreateNot(value(ops[0])), dests[1], dests[0]);
    else
      b_.CreateCondBr(value(ops[0]), dests[0], dests[1]);
    for (size_t k = 0; k < 2; ++k) {
      if (!own[k]) continue;
      b_.SetInsertPoint(dests[k]);
      shareOnEdge(i.parent, i.targets[k].block);
      emitBranch(i.targets[k]);
      b_.CreateBr(blocks_.at(i.targets[k].block));
    }
    break;
  }
  case Op::Ret:
    if (!ops.empty() && plan_->home(ops[0]).kind != Home::Kind::Ret) copyItem(fnRet_, addr(ops[0]));
    b_.CreateBr(fnExit_);
    break;
  }
}

} // namespace xb
