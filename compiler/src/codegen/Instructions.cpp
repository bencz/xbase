// xIR instructions to LLVM: each operation calls the runtime on the items the
// frame plan placed, and tests the pending flag after operations that can fail.
#include "codegen/CodeGen.h"

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
  case Home::Kind::Slot: return slot(h.slot);
  case Home::Kind::Place: return values_.at(v);
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
    Value *p = varAddress(*store.var, store.access, fnExit_);
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
    if (!plan_->inGroup(&i, k)) {
      Value *src = addr(ops[k]);
      copyItem(slot(base + (uint32_t)k), src);
    }
  return slot(base);
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
    Value *p = varAddress(*i.var, i.access, fnExit_);
    const Home &h = plan_->home(&i);
    if (h.kind == Home::Kind::Place && h.place == &i) values_[&i] = p; // the variable's own item serves the use
    else copyItem(addr(&i), p);
    break;
  }
  case Op::Store:
    if (!plan_->forwarded(&i)) {
      Value *src = addr(ops[0]);
      copyItem(varAddress(*i.var, i.access, fnExit_), src);
    }
    break;
  case Op::Barrier: writeBarrier(*i.var, i.access); break;
  case Op::Unary: {
    Value *a = addr(ops[0]);
    callItemOp(i.unop == UnOp::Neg ? rtNeg_ : rtNot_, dest(i), {a});
    checkPending(fnExit_);
    break;
  }
  case Op::Binary: emitBinary(i); break;
  case Op::Truth: {
    auto fused = fusedTruth_.find(&i);
    if (fused != fusedTruth_.end()) {
      values_[&i] = fused->second;
    } else {
      Value *item = addr(ops[0]);
      values_[&i] = testTruth(item, fnExit_, typeOf(ops[0], item));
    }
    break;
  }
  case Op::FromBool: storeLogical(dest(i), value(ops[0])); break;
  case Op::Call: {
    if (emitBitCall(i)) break;
    Value *argv = operandGroup(i);
    Function *callee = i.callee.function ? functions_.at(i.callee.function) : externFunction(i.callee);
    Value *d = dest(i);
    setNil(d); // the caller clears the result, so callees that return nothing need not
    b_.CreateCall(xbFuncTy_, callee, {fnCtx_, d, ConstantInt::get(i32_, ops.size()), argv});
    checkPending(fnExit_);
    break;
  }
  case Op::ArrayLit: {
    Value *elems = operandGroup(i);
    b_.CreateCall(rtArrayNew_, {fnCtx_, dest(i), ConstantInt::get(i32_, ops.size()), elems});
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
    // LLVM inlines the other body (this is its only call): its frame then adds
    // to every call's stack, but the common path's prologue does not prepare
    // a call, which measured faster (fib: 261 ms against 299 ms).
    b_.CreateCall(xbFuncTy_, functions_.at(i.callee.function), {fnCtx_, d, fn_->getArg(2), fn_->getArg(3)});
    checkPending(fnExit_);
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
    for (uint32_t k = 0; k < cells.size(); ++k) b_.CreateStore(cells[k], capturedCellAddress(block, k));
    break;
  }
  case Op::ForTest: emitForTest(i); break;
  case Op::EnumLen:
    values_[&i] = b_.CreateCall(rtEnumLen_, {fnCtx_, callArg(addr(ops[0]), 0)});
    checkPending(fnExit_);
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
    checkPending(fnExit_);
    break;
  }
  case Op::EachLeave: {
    Value *inCell = b_.CreateZExt(value(ops[0]), i32_);
    b_.CreateCall(rtEachLeave_, {rawAddress(*i.var), inCell, callArg(addr(ops[1]), 0)});
    break;
  }
  case Op::RefVar: refOf(*i.var, dest(i)); break;
  case Op::RefIndex:
    b_.CreateCall(rtRefElem_, {fnCtx_, dest(i), callArg(addr(ops[0]), 0), callArg(addr(ops[1]), 1)});
    checkPending(fnExit_);
    break;
  case Op::SwitchCheck:
    b_.CreateCall(rtSwitchCheck_, {fnCtx_, callArg(addr(ops[0]), 0)});
    checkPending(fnExit_);
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
    emitBranch(i.targets[0]);
    b_.CreateBr(blocks_.at(i.targets[0].block));
    break;
  case Op::CondBr: {
    // A target with arguments gets an edge block of its own for them.
    BasicBlock *dests[2];
    for (size_t k = 0; k < 2; ++k)
      dests[k] = i.targets[k].args.empty() ? blocks_.at(i.targets[k].block) : newBlock("edge");
    b_.CreateCondBr(value(ops[0]), dests[0], dests[1]);
    for (size_t k = 0; k < 2; ++k) {
      if (i.targets[k].args.empty()) continue;
      b_.SetInsertPoint(dests[k]);
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
