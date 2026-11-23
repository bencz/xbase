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

// The runtime's operator for a Binary instruction.
FunctionCallee CodeGen::binaryOp(const xir::Instruction &i) { return i.grows ? rtAddGrow_ : rtBin_.at(i.binop); }

// Where an instruction's item result goes: its home, or for a value computed
// into a variable, the variable's item (reached now, before the operation).
Value *CodeGen::dest(const xir::Instruction &i) {
  const Home &h = plan_->home(&i);
  if (h.kind == Home::Kind::Place) {
    const xir::Instruction &store = *h.place; // its access is direct (FramePlan::canForward)
    Value *p = rawAddress(*store.var);
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
  std::vector<uint32_t> moved; // slots of values the group uses last: let go once all are copied (one may come twice)
  for (size_t k = 0; k < ops.size(); ++k)
    if (ops[k]->type() == xir::Type::Item && !plan_->inGroup(&i, k)) { // args.rest: filled by withRest
      Value *src = addr(ops[k]);
      copyItem(slot(base + (uint32_t)k), src);
      if (std::optional<uint32_t> s = plan_->lastUse(ops[k], &i)) moved.push_back(*s);
    }
  for (uint32_t s : moved) setNil(slot(s));
  return slot(base);
}

// A list ending in `...` (an args.rest operand), for `emit` to use as its
// count and items: the other operands, then the arguments the function
// received past its parameters. They go into a frame of their own on the
// stack, sized at run time and linked into the chain while `emit` runs, so the
// collector sees them (a callee may replace them, PValue()). xb_args_rest
// copies the arguments passed on; one whose reference is gone unwinds instead.
// A call of only `...` may pass them in place (forwardInPlace_).
void CodeGen::withRest(const xir::Instruction &i, Value *group, function_ref<void(Value *, Value *)> emit) {
  const auto &ops = i.operands();
  const auto fixed = (uint32_t)ops.size() - 1;
  Value *argc = fn_->getArg(2), *argv = fn_->getArg(3);
  Value *params = ConstantInt::get(i32_, fnParams_);
  Value *rest = b_.CreateSelect(b_.CreateICmpUGT(argc, params), b_.CreateSub(argc, params), ConstantInt::get(i32_, 0),
                                "rest");
  Value *restItems = b_.CreateGEP(itemTy_, argv, params, "rest.items"); // not dereferenced when there are none
  uint32_t refs = i.op == xir::Op::Call ? i.callee.byref : 0; // a method and an array element receive values
  if (forwardInPlace_ && i.op == xir::Op::Call && fixed == 0 && refs == UINT32_MAX) {
    // f( ... ): the callee receives the arguments where they are, which nothing here reads again
    emit(rest, restItems);
    return;
  }
  Value *total = b_.CreateAdd(rest, ConstantInt::get(i32_, fixed), "total");
  Value *save = b_.CreateStackSave();
  StructType *ty = frameType(0);
  uint64_t header = dl_.getStructLayout(ty)->getElementOffset(FrameSlots);
  Value *itemBytes = b_.CreateMul(b_.CreateZExt(total, intPtrTy_), ConstantInt::get(intPtrTy_, itemSize_));
  AllocaInst *frame = b_.CreateAlloca(i8_, b_.CreateAdd(itemBytes, ConstantInt::get(intPtrTy_, header)), "list.frame");
  frame->setAlignment(std::max(itemAlign_, dl_.getPointerABIAlignment(0)));
  checkStack(frame, unwind_, save); // a long list near the limit: the error, not a write past the stack's end
  Value *items = b_.CreateStructGEP(ty, frame, FrameSlots, "list.items");
  b_.CreateMemSet(items, b_.getInt8(0), itemBytes, itemAlign_); // NIL until filled
  linkFrame(frame, total, ConstantPointerNull::get(ptrTy_));
  auto unlink = [&] {
    unlinkFrame(frame);
    b_.CreateStackRestore(save);
  };
  for (uint32_t k = 0; k < fixed; ++k)
    copyItem(b_.CreateConstInBoundsGEP1_32(itemTy_, items, k), b_.CreateConstInBoundsGEP1_32(itemTy_, group, k));
  Value *at = b_.CreateConstInBoundsGEP1_32(itemTy_, items, fixed);
  Value *ok = b_.CreateCall(rtArgsRest_, {fnCtx_, at, restItems, rest, ConstantInt::get(i32_, fixed),
                                          ConstantInt::get(i32_, refs)});
  BasicBlock *failed = newBlock("list.failed");
  BasicBlock *next = newBlock("list.rest");
  b_.CreateCondBr(b_.CreateICmpNE(ok, ConstantInt::get(i32_, 0)), next, failed);
  b_.SetInsertPoint(next);
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
// parameters are evaluated and left out, as nothing reads them). A result that
// never refers to a heap object comes back as two words too (returnsWords),
// stored into its home (NIL when the callee unwinds); any other is written by
// the callee into the result's item, not cleared first: a compiled function
// always writes it when it returns, and when it unwinds the result is not
// read (its slot keeps a valid item for the collector, as every slot does).
void CodeGen::emitDirectCall(const xir::Instruction &i, Value *d) {
  const xir::Function &callee = *i.callee.function;
  bool words = returnsWords(callee);
  std::vector<Value *> args{fnCtx_};
  if (!words) args.push_back(d);
  args.push_back(innermostFrame()); // the chain's frame at a call is the caller's
  for (uint32_t k = 0; k < callee.numParams; ++k) {
    if (k >= i.operands().size()) {
      args.insert(args.end(), {headerWord(XB_T_NIL, 0, 0), ConstantInt::get(i64_, 0)});
      continue;
    }
    Value *item = addr(i.operand(k));
    args.push_back(loadHeader(item));        // the cached header when there is one
    args.push_back(loadWords(item).payload); // a pointer is read where it is
  }
  // An argument this call uses last: its slot let go (the callee copies it into its own before anything can collect),
  // so a callee that sets its parameter to NIL frees what only it then holds.
  for (size_t k = 0; k < i.operands().size() && k < callee.numParams; ++k)
    if (std::optional<uint32_t> s = plan_->lastUse(i.operand(k), &i)) setNil(slot(*s));
  Value *result = b_.CreateCall(direct_.at(&callee), args);
  if (words) storeWords(d, b_.CreateExtractValue(result, 0), b_.CreateExtractValue(result, 1));
  checkPending(unwind_);
}

// .T. or .F.; any other value is an error (the function leaves). `type` is
// the item's type (a constant when it is known).
Value *CodeGen::testTruth(Value *item, BasicBlock *unwind, Value *type, bool ofNot) {
  BasicBlock *ok = newBlock("cond.ok");
  BasicBlock *bad = newBlock("cond.bad");
  MDBuilder md(ctx_);
  b_.CreateCondBr(b_.CreateICmpEQ(type, ConstantInt::get(i32_, XB_T_LOGICAL)), ok, bad, md.createLikelyBranchWeights());
  b_.SetInsertPoint(bad);
  if (!ofNot) {
    b_.CreateCall(rtCondError_, {fnCtx_, callArg(item, 0)});
    b_.CreateBr(unwind);
    b_.SetInsertPoint(ok);
    return logicalValue(item);
  }
  // The operand of a .NOT.: an object's class may overload it (xb_cond_not), else the same error and unwinding.
  Value *answer = b_.CreateCall(rtCondNot_, {fnCtx_, callArg(item, 0)});
  BasicBlock *answered = newBlock("cond.not"), *join = newBlock("cond.join");
  b_.CreateCondBr(b_.CreateICmpEQ(answer, ConstantInt::get(i32_, XB_COND_NOT_FAILED)), unwind, answered);
  b_.SetInsertPoint(answered);
  Value *operatorTruth = b_.CreateICmpNE(answer, ConstantInt::get(i32_, 0));
  b_.CreateBr(join);
  b_.SetInsertPoint(ok);
  Value *plain = logicalValue(item);
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
  PHINode *phi = b_.CreatePHI(b_.getInt1Ty(), 2, "cond");
  phi->addIncoming(plain, ok);
  phi->addIncoming(operatorTruth, answered);
  return phi;
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

// A test speculate-sends makes: object.is, or the "or" of several (not( and( not a, not b ) )).
static bool classTest(const xir::Instruction &i) {
  if (i.op == xir::Op::ObjectIs) return true;
  const auto *a = i.op == xir::Op::BoolNot ? xir::asInstruction(i.operand(0), xir::Op::BoolAnd) : nullptr;
  if (!a) return false;
  for (const xir::Value *v : a->operands()) {
    const xir::Instruction *n = xir::asInstruction(v, xir::Op::BoolNot);
    const xir::Value *o = n ? n->operand(0) : nullptr;
    if (!o || o->kind() != xir::Value::Kind::Instruction || !classTest(*static_cast<const xir::Instruction *>(o)))
      return false;
  }
  return true;
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
    // The variable's own item serves the use (an elided load), or what the runtime read into the load's shadow slot.
    const Home &h = plan_->home(&i);
    bool inPlace = h.kind == Home::Kind::Place && h.place == &i;
    Value *own = inPlace ? (xir::isPlain(*i.var, i.access) ? nullptr : slot(plan_->shadowSlot(&i))) : addr(&i);
    Value *p = loadEachElement(i, own);
    if (!p) p = loadVar(*i.var, i.access, own, unwind_);
    if (sharingLoads_.count(&i)) shareString(p); // the value leaves the variable: its string stops growing
    if (inPlace) values_[&i] = p;
    else if (p != own) copyItem(own, p);
    break;
  }
  case Op::Store:
    if (!plan_->forwarded(&i)) storeVar(*i.var, i.access, addr(ops[0]), unwind_);
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
    emitNot(i);
    break;
  }
  case Op::Binary: emitBinary(i); break;
  case Op::JoinStep: case Op::JoinEnd: emitJoin(i); break;
  case Op::Truth: {
    auto fused = fusedTruth_.find(&i);
    if (fused != fusedTruth_.end()) {
      values_[&i] = fused->second;
    } else {
      Value *item = addr(ops[0]);
      values_[&i] = testTruth(item, unwind_, typeOf(ops[0], item), i.index != 0);
    }
    break;
  }
  case Op::FromBool: storeLogical(dest(i), value(ops[0])); break;
  case Op::MvGet:
  case Op::MvRef: {
    CallResult r = callResult(dest(i), 0);
    b_.CreateCall(i.op == Op::MvGet ? rtMvGet_ : rtMvRef_, {fnCtx_, r.arg, memvars_.at(i.message)});
    callDone(r);
    checkPending(unwind_);
    break;
  }
  case Op::MvSet:
    b_.CreateCall(rtMvSet_, {fnCtx_, memvars_.at(i.message), callArg(addr(ops[0]), 1)});
    checkPending(unwind_);
    break;
  case Op::MvField: {
    CallResult r = callResult(dest(i), 0);
    b_.CreateCall(rtMvField_, {fnCtx_, r.arg, memvars_.at(i.message), fieldCache()});
    callDone(r);
    checkPending(unwind_);
    break;
  }
  case Op::FieldGet: {
    CallResult r = callResult(dest(i), 0);
    Value *area = ops.empty() ? static_cast<Value *>(ConstantPointerNull::get(ptrTy_)) : callArg(addr(ops[0]), 1);
    b_.CreateCall(rtFieldGet_, {fnCtx_, r.arg, area, aliasName(i.alias), messageName(i.message), fieldCache()});
    callDone(r);
    checkPending(unwind_);
    break;
  }
  case Op::FieldSet: {
    Value *area = ops.size() < 2 ? static_cast<Value *>(ConstantPointerNull::get(ptrTy_)) : callArg(addr(ops[0]), 0);
    b_.CreateCall(rtFieldSet_, {fnCtx_, area, aliasName(i.alias), messageName(i.message), fieldCache(),
                                callArg(addr(ops.back()), 1)});
    checkPending(unwind_);
    break;
  }
  case Op::MvPrivate: b_.CreateCall(rtMvPrivate_, {fnCtx_, memvars_.at(i.message), privateMark()}); break;
  case Op::MvPublic: b_.CreateCall(rtMvPublic_, {fnCtx_, memvars_.at(i.message)}); break;
  case Op::MvParam: // argument `index` of those received, NIL past them
    b_.CreateCall(rtMvParam_, {fnCtx_, memvars_.at(i.message), argItem(fn_->getArg(2), fn_->getArg(3), i.index)});
    break;
  case Op::ClassNew: emitClassNew(i); break;
  case Op::MacroGet:
  case Op::MacroRef:
    callItemOp(i.op == Op::MacroGet ? rtMacroGet_ : rtMacroRef_, dest(i), {addr(ops[0])});
    checkPending(unwind_);
    break;
  case Op::MacroSet:
    b_.CreateCall(rtMacroSet_, {fnCtx_, callArg(addr(ops[0]), 0), callArg(addr(ops[1]), 1)});
    checkPending(unwind_);
    break;
  case Op::MacroCall: { // the text, then the arguments, in consecutive slots
    Value *group = operandGroup(i);
    CallResult r = callResult(dest(i), 0);
    setNil(r.arg);
    Value *argv = b_.CreateConstInBoundsGEP1_32(itemTy_, group, 1, "macro.args");
    b_.CreateCall(rtMacroCall_, {fnCtx_, r.arg, group, ConstantInt::get(i32_, ops.size() - 1), argv});
    callDone(r);
    checkPending(unwind_);
    break;
  }
  case Op::MacroText: callItemOp(rtMacroText_, dest(i), {addr(ops[0])}); break;
  case Op::MacroPrivate:
  case Op::MacroPublic:
    if (i.op == Op::MacroPrivate) b_.CreateCall(rtMacroPrivate_, {fnCtx_, callArg(addr(ops[0]), 0), privateMark()});
    else b_.CreateCall(rtMacroPublic_, {fnCtx_, callArg(addr(ops[0]), 0)});
    checkPending(unwind_);
    break;
  case Op::ArgsRest: break; // read by the list that uses it (withRest)
  case Op::Call: {
    if (emitBitCall(i) || emitInlineCall(i)) break;
    if (xir::isDirectCall(i)) {
      emitDirectCall(i, dest(i));
      break;
    }
    Value *argv = operandGroup(i);
    Function *callee = i.callee.function ? functionOf(i.callee.function) : externFunction(i.callee);
    Value *d = dest(i);
    setNil(d); // the caller clears the result, so callees that return nothing need not
    auto call = [&](Value *n, Value *items) { b_.CreateCall(xbFuncTy_, callee, {fnCtx_, d, n, items}); };
    xir::TypeSet first = xir::isEvalCall(i) ? plan_->types().of(ops[0]) : 0;
    if (first & xir::typeBit(XB_T_BLOCK)) { // Eval( block, ... ): the block's function called here, as xb_block_call
                                            // does (a block ends the loan of its arguments itself); else Eval()
      bool surely = first == xir::typeBit(XB_T_BLOCK);
      BasicBlock *block = newBlock("eval.block"), *other = surely ? nullptr : newBlock("eval.call");
      BasicBlock *done = newBlock("eval.done");
      if (surely) b_.CreateBr(block);
      else branchLikely(b_.CreateICmpEQ(typeOf(ops[0], argv), ConstantInt::get(i32_, XB_T_BLOCK)), block, other);
      b_.SetInsertPoint(block);
      Value *self = itemPointer(argv, "block");
      Value *fn = loadFrom(ptrTy_, b_.CreateStructGEP(blockTy_, self, 0), Mem::Unknown, "block.fn");
      Value *rest = b_.CreateConstInBoundsGEP1_32(itemTy_, argv, 1);
      b_.CreateCall(blockFnTy_, fn, {fnCtx_, d, ConstantInt::get(i32_, ops.size() - 1), rest, self});
      b_.CreateBr(done);
      if (other) {
        b_.SetInsertPoint(other);
        bool lent = lends(i, 0);
        if (lent) lend(argv);
        call(ConstantInt::get(i32_, ops.size()), argv);
        if (lent) endLend();
        b_.CreateBr(done);
      }
      b_.SetInsertPoint(done);
    } else if (i.hasRest()) {
      withRest(i, argv, call);
    } else {
      bool lent = lends(i, 0);
      if (lent) lend(argv);
      call(ConstantInt::get(i32_, ops.size()), argv);
      if (lent) endLend();
    }
    checkPending(unwind_);
    break;
  }
  case Op::FuncRef:
    copyItem(dest(i), funcRef(i.callee));
    break;
  case Op::Send:
  case Op::SendMacro: { // a macro's: the text, then the object and the arguments, in consecutive slots; no cache
    Value *argv = operandGroup(i);
    Value *d = dest(i);
    setNil(d);
    Constant *from = i.classIndex < 0 ? static_cast<Constant *>(ConstantPointerNull::get(ptrTy_))
                                      : classDef(i.classIndex);
    Value *super = ConstantInt::get(i32_, i.superSend ? 1 : 0);
    GlobalVariable *cache = nullptr;
    if (i.op == xir::Op::Send) { // the send's own inline cache (xb_send_cache: pointer-sized words the runtime fills)
      auto *cacheTy = ArrayType::get(intPtrTy_, XB_SEND_CACHE_WORDS);
      cache = new GlobalVariable(mod_, cacheTy, false, GlobalValue::PrivateLinkage, ConstantAggregateZero::get(cacheTy),
                                 "xb.send.cache");
    }
    auto send = [&](Value *n, Value *items) {
      if (cache) b_.CreateCall(rtSendFrom_, {fnCtx_, d, messageName(i.message), from, super, cache, n, items});
      else b_.CreateCall(rtMacroSend_, {fnCtx_, d, from, super, ConstantInt::get(i32_, i.index), n, items});
    };
    if (i.hasRest()) {
      withRest(i, argv, send);
    } else {
      bool macro = i.op == xir::Op::SendMacro; // the text, then the object: the method receives them past the text
      bool lent = lends(i, macro ? 2 : 1);
      if (lent) lend(macro ? b_.CreateConstInBoundsGEP1_32(itemTy_, argv, 1) : argv);
      if (cache) sendInPlace(i, argv, d, cache, [&] { send(ConstantInt::get(i32_, ops.size()), argv); });
      else send(ConstantInt::get(i32_, ops.size()), argv);
      if (lent) endLend();
    }
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
    if (emitArrayLiteral(i)) break;
    Value *elems = operandGroup(i);
    Value *d = dest(i);
    auto make = [&](Value *n, Value *items) { b_.CreateCall(rtArrayNew_, {fnCtx_, d, n, items}); };
    if (i.hasRest()) withRest(i, elems, make);
    else make(ConstantInt::get(i32_, ops.size()), elems);
    break;
  }
  case Op::Index: emitIndex(i); break;
  case Op::TableIndex: emitTableIndex(i); break;
  case Op::ObjectGet: case Op::ObjectSet: emitObjectAccess(i); break;
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
    b_.CreateCall(xbFuncTy_, functionOf(i.callee.function), {fnCtx_, d, argc, argv});
    checkPending(unwind_);
    break;
  }
  case Op::MakeBlock: emitMakeBlock(i); break;
  case Op::ForTest: emitForTest(i); break;
  case Op::EnumLen:
    emitEnumLen(i);
    checkPending(unwind_);
    break;
  case Op::EachSource:
    b_.CreateCall(rtEachSource_, {fnCtx_, dest(i), callArg(addr(ops[0]), 0), b_.CreateZExt(value(ops[1]), i32_)});
    checkPending(unwind_);
    break;
  case Op::EachInCell: {
    Value *shared = b_.CreateCall(rtEachShared_, {fnCtx_, rawAddress(*i.var), isStaticArg(*i.var)}, "shared");
    values_[&i] = b_.CreateICmpNE(shared, ConstantInt::get(i32_, 0));
    break;
  }
  case Op::EachEnter: {
    b_.CreateCall(rtEachEnter_, {fnCtx_, dest(i), rawAddress(*i.var), eachHow(i), callArg(addr(ops[1]), 0)});
    break;
  }
  case Op::EachMove: { // in place: to an element of a local array or hash without holes, a local generator's
                       // Current, or a string's byte into the loop's own item (xb_each_move for the rest); the
                       // collection is the loop's source (the reference's), whose type a versioned loop knows
    // A string's byte that no load reads through the reference (they read it from the string, loadEachElement): none
    // is put there. Their path through the reference, for a position past the end, never runs: the loop test keeps
    // the position within the string, which does not change.
    if (plan_->readsInPlace(i.var) && plan_->types().of(ops[2]) == xir::typeBit(XB_T_STRING)) break;
    Value *var = rawAddress(*i.var);
    Value *src = addr(ops[2]), *srcType = typeOf(ops[2], src);
    BasicBlock *slow = newBlock("each.move"), *join = newBlock("each.moved"), *local = newBlock("each.local");
    if (i.var->kind == xir::Var::Kind::Static) { // shared storage: the runtime reads the variable whole
      BasicBlock *own = newBlock("static.local");
      branchLikely(storageLocal(), own, slow);
      b_.SetInsertPoint(own);
    }
    Value *cell = itemPointer(var, "cell");
    BasicBlock *from = b_.GetInsertBlock(), *inCell = newBlock("each.cell");
    b_.CreateCondBr(value(ops[0]), inCell, local); // a variable a codeblock captured: its cell's value
    b_.SetInsertPoint(inCell);
    branchLikely(isLocal(cell), local, slow); // a shared cell's reference is the runtime's to move
    b_.SetInsertPoint(local);
    PHINode *base = b_.CreatePHI(ptrTy_, 2, "base");
    base->addIncoming(var, from);
    base->addIncoming(heapItem(cell), inCell);
    Value *ref = itemPointer(base, "ref");
    BasicBlock *refLocal = newBlock("each.ref.local"); // a loop's reference other threads reach: the runtime's
    branchLikely(isLocal(ref), refLocal, slow);
    b_.SetInsertPoint(refLocal);
    tag(b_.CreateStore(value(ops[1]), b_.CreateStructGEP(refTy_, ref, RefIndex)), Mem::RefIndex);
    BasicBlock *object = newBlock("each.object"), *other = newBlock("each.collection");
    b_.CreateCondBr(b_.CreateICmpEQ(srcType, ConstantInt::get(i32_, XB_T_OBJECT)), object, other);
    b_.SetInsertPoint(object); // a local generator's Current into the loop's own item, as xb_ref_own_put puts it
    Value *gen = localGenerator(src, slow);
    Value *refState = stateByte(ref, AtomicOrdering::Monotonic);
    Value *vars = b_.CreateStructGEP(objectTy_, gen, ObjectVars);
    copyItem(b_.CreateStructGEP(refTy_, ref, RefValue),
             b_.CreateInBoundsGEP(itemTy_, vars, ConstantInt::get(i64_, XB_GEN_CURRENT), "gen.current"));
    recordIfOld(ref, refState);
    b_.CreateBr(join);
    b_.SetInsertPoint(other);
    BasicBlock *str = newBlock("each.string"), *inStr = newBlock("each.byte"), *coll2 = newBlock("each.elements");
    b_.CreateCondBr(b_.CreateICmpEQ(srcType, ConstantInt::get(i32_, XB_T_STRING)), str, coll2);
    b_.SetInsertPoint(str); // a string's byte, the shared one-byte string, into the loop's own item (as move_to does;
                            // past the end, nothing): a static string needs no barrier
    Value *s = itemPointer(src, "str"), *pos0 = b_.CreateSub(value(ops[1]), ConstantInt::get(i64_, 1));
    b_.CreateCondBr(b_.CreateICmpULT(pos0, strLength(s)), inStr, join);
    b_.SetInsertPoint(inStr);
    storeTagged(b_.CreateStructGEP(refTy_, ref, RefValue), XB_T_STRING, byteString(strByte(s, pos0)));
    b_.CreateBr(join);
    b_.SetInsertPoint(coll2); // a local array, of any form, within its length: an element there (else the runtime's
                              // bound error); a hash's entry in place (inlineRefTarget)
    BasicBlock *arr = newBlock("each.array"), *arrLocal = newBlock("each.array.local"), *hash = newBlock("each.hash");
    b_.CreateCondBr(b_.CreateICmpEQ(srcType, ConstantInt::get(i32_, XB_T_ARRAY)), arr, hash);
    b_.SetInsertPoint(arr);
    Value *array = itemPointer(src, "arr");
    branchLikely(isLocal(array), arrLocal, slow);
    b_.SetInsertPoint(arrLocal);
    Value *at0 = b_.CreateSub(value(ops[1]), ConstantInt::get(i64_, 1));
    branchLikely(b_.CreateICmpULT(at0, arrayLength(array)), join, slow);
    b_.SetInsertPoint(hash);
    inlineRefTarget(ref, slow, src, srcType);
    b_.CreateBr(join);
    b_.SetInsertPoint(slow);
    b_.CreateCall(rtEachMove_, {fnCtx_, var, eachHow(i), value(ops[1])});
    checkPending(unwind_);
    b_.CreateBr(join);
    b_.SetInsertPoint(join);
    break;
  }
  case Op::EachLeave: {
    b_.CreateCall(rtEachLeave_, {rawAddress(*i.var), eachHow(i), callArg(addr(ops[1]), 0)});
    break;
  }
  case Op::EachKey: { // the position itself, or a key read in place from a hash without holes (else xb_each_key)
    Value *coll = addr(ops[0]), *d = dest(i);
    BasicBlock *hash = newBlock("each.key.hash"), *other = newBlock("each.key.pos"), *slow = newBlock("each.key");
    BasicBlock *join = newBlock("each.key.end");
    b_.CreateCondBr(b_.CreateICmpEQ(typeOf(ops[0], coll), ConstantInt::get(i32_, XB_T_HASH)), hash, other);
    b_.SetInsertPoint(other);
    storeInt(d, value(ops[1]));
    b_.CreateBr(join);
    b_.SetInsertPoint(hash);
    Value *h = itemPointer(coll, "hash");
    BasicBlock *local = newBlock("each.key.local");
    branchLikely(isLocal(h), local, slow);
    b_.SetInsertPoint(local);
    Value *entry = hashEntryAt(h, b_.CreateSub(value(ops[1]), ConstantInt::get(i64_, 1)), slow);
    copyItem(d, heapItem(b_.CreateStructGEP(hentryTy_, entry, HEntryKey)));
    b_.CreateBr(join);
    b_.SetInsertPoint(slow);
    CallResult r = callResult(d, 0);
    b_.CreateCall(rtEachKey_, {fnCtx_, r.arg, callArg(coll, 1), value(ops[1])});
    callDone(r);
    checkPending(unwind_);
    b_.CreateBr(join);
    b_.SetInsertPoint(join);
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
  case Op::SeqWith: callItemOp(rtSeqWith_, dest(i), {addr(ops[0])}); break;
  case Op::SeqRestore: b_.CreateCall(rtSeqRestore_, {fnCtx_, addr(ops[0])}); break;
  case Op::SeqSuspend: values_[&i] = b_.CreateZExt(b_.CreateCall(rtSeqSuspend_, {fnCtx_}), i64_); break;
  case Op::SeqResume:
    b_.CreateCall(rtSeqResume_, {fnCtx_, b_.CreateTrunc(value(ops[0]), i32_), addr(ops[1])});
    break;
  case Op::SubstCheck: { // the tag as the item holds it: a substituted value may have any type
    Value *bit = b_.CreateShl(ConstantInt::get(i32_, 1), itemType(addr(ops[0])));
    Value *own = ConstantInt::get(i32_, plan_->types().of(ops[0]));
    values_[&i] = b_.CreateICmpEQ(b_.CreateAnd(bit, own), ConstantInt::get(i32_, 0));
    break;
  }
  case Op::SubstValue: copyItem(dest(i), addr(ops[0])); break;
  case Op::ScalarPlain: { // the value's type in the mask, and xb_scalar_replaced[type] == 0 (set once, never cleared)
    auto *table = ArrayType::get(i8_, XB_T_COUNT_);
    if (!scalarReplaced_) scalarReplaced_ = mod_.getOrInsertGlobal("xb_scalar_replaced", table);
    Value *type = typeOf(ops[0], addr(ops[0]));
    Value *inMask = b_.CreateICmpNE(b_.CreateAnd(b_.CreateShl(ConstantInt::get(i32_, 1), type),
                                                 ConstantInt::get(i32_, i.index)),
                                    ConstantInt::get(i32_, 0));
    Value *at = b_.CreateInBoundsGEP(table, scalarReplaced_, {ConstantInt::get(i32_, 0), type});
    auto *flag = cast<LoadInst>(loadFrom(i8_, at, Mem::Unknown, "scalar.replaced"));
    flag->setAtomic(AtomicOrdering::Unordered);
    values_[&i] = b_.CreateAnd(inMask, b_.CreateICmpEQ(flag, ConstantInt::get(i8_, 0)));
    break;
  }
  case Op::HasOperator: { // an object (a test of its tag), then its class's operator (the runtime's look-up)
    Value *item = addr(ops[0]);
    Value *isObject = b_.CreateICmpEQ(typeOf(ops[0], item), ConstantInt::get(i32_, XB_T_OBJECT));
    BasicBlock *from = b_.GetInsertBlock(), *object = newBlock("has.operator"), *done = newBlock("has.operator.done");
    b_.CreateCondBr(isObject, object, done);
    b_.SetInsertPoint(object);
    Value *found = b_.CreateCall(rtHasOperator_, {callArg(item, 0), messageName(i.message)});
    Value *has = b_.CreateICmpNE(found, ConstantInt::get(i32_, 0));
    BasicBlock *at = b_.GetInsertBlock();
    b_.CreateBr(done);
    b_.SetInsertPoint(done);
    PHINode *phi = b_.CreatePHI(b_.getInt1Ty(), 2, "has.operator");
    phi->addIncoming(b_.getFalse(), from);
    phi->addIncoming(has, at);
    values_[&i] = phi;
    break;
  }
  case Op::ObjectIs: { // an object whose class is the module's class `classIndex`: its xb_class_def's `runtime`
    Value *item = addr(ops[0]);
    Value *isObject = b_.CreateICmpEQ(typeOf(ops[0], item), ConstantInt::get(i32_, XB_T_OBJECT));
    BasicBlock *from = b_.GetInsertBlock(), *object = newBlock("object.is"), *done = newBlock("object.is.done");
    b_.CreateCondBr(isObject, object, done);
    b_.SetInsertPoint(object);
    Value *cls = loadFrom(ptrTy_, b_.CreateStructGEP(objectTy_, itemPointer(item, "obj"), ObjectCls),
                          Mem::ObjectHeader, "cls");
    // `runtime`: written once, when the class is prepared (before any object of it exists)
    auto *runtime = cast<LoadInst>(loadFrom(ptrTy_, b_.CreateStructGEP(classDefTy_, classDef(i.classIndex),
                                                                       ClassDefRuntime),
                                            Mem::Unknown, "class.runtime"));
    runtime->setAtomic(AtomicOrdering::Unordered);
    // Written once: a value read before (null) only fails the test, which the send then answers.
    runtime->setMetadata(LLVMContext::MD_invariant_load, MDNode::get(ctx_, {}));
    Value *same = b_.CreateICmpEQ(cls, runtime);
    BasicBlock *at = b_.GetInsertBlock();
    b_.CreateBr(done);
    b_.SetInsertPoint(done);
    PHINode *is = b_.CreatePHI(b_.getInt1Ty(), 2, "object.is");
    is->addIncoming(b_.getFalse(), from);
    is->addIncoming(same, at);
    values_[&i] = is;
    break;
  }
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
  case Op::BoxI64: storeInt(dest(i), value(ops[0])); break;
  case Op::BoxBool: storeLogical(dest(i), value(ops[0])); break;
  case Op::UnboxI64: values_[&i] = intValue(addr(ops[0])); break;
  case Op::UnboxBool: values_[&i] = logicalValue(addr(ops[0])); break;
  case Op::GenNew:
  case Op::AgenNew: {
    Value *argv = operandGroup(i);
    Value *d = dest(i);
    setNil(d);
    b_.CreateCall(i.op == Op::GenNew ? rtGenNew_ : rtAgenNew_,
                  {fnCtx_, d, functionOf(i.callee.function), ConstantInt::get(i32_, ops.size()), argv});
    break;
  }
  case Op::GenRestore: emitGenRestore(i); break;
  case Op::GenDone: b_.CreateCall(rtGenDone_, {fnCtx_, rawAddress(*i.var)}); break;
  case Op::AsyncNew: { // the task, after its first step ran here: a QUIT there goes on unwinding
    Value *argv = operandGroup(i);
    Value *d = dest(i);
    setNil(d);
    b_.CreateCall(rtAsyncNew_, {fnCtx_, d, functions_.at(i.blockFn), functionOf(i.callee.function),
                                ConstantInt::get(i32_, ops.size()), argv});
    checkPending(unwind_);
    break;
  }
  case Op::AsyncRestore: // the frame's slots after the task's own (parameter 0)
    values_[&i] = b_.CreateSExt(
        b_.CreateCall(rtAsyncRestore_, {fnCtx_, fnBase_, ConstantInt::get(i32_, plan_->savedSize())}), i64_);
    break;
  case Op::AwaitFuse: { // no entry: a send's method
    Value *entry = i.callee.function ? static_cast<Value *>(functionOf(i.callee.function))
                                     : ConstantPointerNull::get(ptrTy_);
    b_.CreateCall(rtAwaitFuse_, {fnCtx_, entry});
    break;
  }
  case Op::AwaitFused:
    values_[&i] = b_.CreateICmpNE(b_.CreateCall(rtAwaitFused_, {fnCtx_}), ConstantInt::get(i32_, 0));
    break;
  case Op::AwaitReady:
    values_[&i] = b_.CreateICmpNE(b_.CreateCall(rtAwaitReady_, {addr(ops[0])}), ConstantInt::get(i32_, 0));
    break;
  case Op::AwaitValue:
  case Op::AwaitWait:
    callItemOp(i.op == Op::AwaitValue ? rtAwaitResult_ : rtAwaitWait_, dest(i), {addr(ops[0])});
    checkPending(unwind_);
    break;
  case Op::InlineEnter: { // a body whose position the frame below shows (lineValue); one that reads its arguments as
                           // received, or an ASYNC function's entry (xb_async_new runs its first step fused with the
                           // AWAIT of the call when the entry's frame is the innermost, over the AWAIT's), links a
                           // frame without slots, showing the inlined function, over the innermost one
    uint32_t callLine = lineValue(inlineOpen_.size(), showsLines() ? line_ : 0); // as the call it replaces stores it
    bool makesTask = std::any_of(i.blockFn->blocks.begin(), i.blockFn->blocks.end(), [](const auto &b) {
      return std::any_of(b->insts.begin(), b->insts.end(), [](const auto &x) { return x->op == Op::AsyncNew; });
    });
    if (!i.blockFn->readsArgs && !makesTask) {
      inlineCaller_[&i] = callLine;
      inlineOpen_.push_back(&i);
      break;
    }
    size_t depth = 0; // of the bodies open that link a frame
    for (const xir::Instruction *open : inlineOpen_) depth += inlineFrame_.count(open);
    if (inlineFrames_.size() == depth) {
      IRBuilder<> entry(&fn_->getEntryBlock(), fn_->getEntryBlock().begin());
      inlineFrames_.push_back(entry.CreateAlloca(frameType(0), nullptr, "inline.frame"));
    }
    Value *frame = inlineFrames_[depth];
    Value *outer = innermostFrame();
    {
      Value *at = b_.CreateStructGEP(frameType(0), outer, FrameLine);
      StoreInst *line = b_.CreateStore(ConstantInt::get(i32_, callLine), at);
      tag(line, Mem::FrameLink);
      line->setMetadata(lineKind_, MDNode::get(ctx_, ConstantAsMetadata::get(ConstantInt::get(i32_, callLine))));
      callFrame_[line] = outer;
    }
    // Its own proc, which holds the positions of the bodies inlined into it in turn.
    Constant *shared = procOf(*i.blockFn);
    Constant *proc = shared;
    if (auto *gv = dyn_cast<GlobalVariable>(shared)) {
      Constant *init = withInlines(gv->getInitializer(), ConstantPointerNull::get(ptrTy_));
      auto *own = new GlobalVariable(mod_, gv->getValueType(), true, GlobalValue::PrivateLinkage, init, ".xb.proc");
      own->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
      proc = own;
    }
    inlineTableOf_[&i] = inlineTables_.size();
    inlineTables_.push_back({dyn_cast<GlobalVariable>(proc), {}, {}});
    inlineFrame_[&i] = frame;
    if (i.blockFn->readsArgs) { // the arguments as received, which its body reads (PCount())
      Value *argv = operandGroup(i);
      tag(b_.CreateStore(argv, b_.CreateStructGEP(frameType(0), frame, FrameArgv)), Mem::FrameLink);
      Value *argc = ConstantInt::get(i32_, i.operands().size());
      tag(b_.CreateStore(argc, b_.CreateStructGEP(frameType(0), frame, FrameArgc)), Mem::FrameLink);
    }
    linkFrame(frame, ConstantInt::get(i32_, 0), proc, outer);
    inlineOpen_.push_back(&i);
    break;
  }
  case Op::FrameRelease:
    for (uint32_t s : plan_->clearsAt(&i)) setNil(slot(s));
    break;
  case Op::InlineLeave: {
    for (uint32_t s : plan_->clearsAt(&i)) setNil(slot(s));
    bool linked = inlineFrame_.count(inlineOpen_.back()) != 0;
    inlineOpen_.pop_back();
    if (linked) tag(b_.CreateStore(innermostFrame(), b_.CreateStructGEP(ctxHeadTy_, fnCtx_, CtxFrames)), Mem::Ctx);
    break;
  }
  case Op::Br:
    pollOnBackEdge(i.parent, i.targets[0].block);
    shareOnEdge(i.parent, i.targets[0].block);
    emitBranch(i.targets[0]);
    for (uint32_t s : plan_->clearsOnEdge(i.parent, i.targets[0].block)) setNil(slot(s)); // after the arguments
    b_.CreateBr(blocks_.at(i.targets[0].block));
    break;
  case Op::CondBr: {
    pollOnBackEdge(i.parent, i.targets[0].block);
    if (blockOrder_.at(i.targets[0].block) > blockOrder_.at(i.parent)) pollOnBackEdge(i.parent, i.targets[1].block);
    // A target with arguments, or whose edge ends a string's growth or clears a loop's slots, gets an edge block.
    BasicBlock *dests[2];
    bool own[2];
    for (size_t k = 0; k < 2; ++k) {
      own[k] = !i.targets[k].args.empty() || shareOnEdge_.count({i.parent, i.targets[k].block}) ||
               !plan_->clearsOnEdge(i.parent, i.targets[k].block).empty();
      dests[k] = own[k] ? newBlock("edge") : blocks_.at(i.targets[k].block);
    }
    const auto *cond = ops[0]->kind() == xir::Value::Kind::Instruction ? static_cast<const xir::Instruction *>(ops[0])
                                                                       : nullptr;
    if (cond && cond->op == xir::Op::SubstCheck) // a substituted value of another type: rare
      branchLikely(b_.CreateNot(value(ops[0])), dests[1], dests[0]);
    else if (cond && classTest(*cond)) // speculate-sends: the classes it chose are the ones its objects have
      branchLikely(value(ops[0]), dests[0], dests[1]);
    else
      b_.CreateCondBr(value(ops[0]), dests[0], dests[1]);
    for (size_t k = 0; k < 2; ++k) {
      if (!own[k]) continue;
      b_.SetInsertPoint(dests[k]);
      shareOnEdge(i.parent, i.targets[k].block);
      emitBranch(i.targets[k]);
      for (uint32_t s : plan_->clearsOnEdge(i.parent, i.targets[k].block)) setNil(slot(s));
      b_.CreateBr(blocks_.at(i.targets[k].block));
    }
    break;
  }
  case Op::Ret: // a compiled function always gives its result, NIL for none: a direct call clears nothing first
    if (ops.empty()) {
      if (fnRet_) setNil(fnRet_);
    } else if (plan_->home(ops[0]).kind != Home::Kind::Ret) {
      copyItem(fnRet_, addr(ops[0]));
    }
    b_.CreateBr(fnExit_);
    break;
  case Op::GenYield: // the frame and the element into the enumerator; the step returns .T.
    shareOnEdge(i.parent, i.targets[0].block); // the next step enters the target through gen.dispatch
    emitGenYield(i);
    storeLogical(fnRet_, b_.getInt1(true));
    b_.CreateBr(fnExit_);
    break;
  case Op::AsyncSuspend: // the frame and the task awaited into the task; the step returns
  case Op::AsyncYield:   // the frame into the task, the element into the enumerator; the step returns
    shareOnEdge(i.parent, i.targets[0].block);
    b_.CreateCall(i.op == Op::AsyncSuspend ? rtAsyncSuspend_ : rtAsyncYield_,
                  {fnCtx_, fnBase_, ConstantInt::get(i32_, plan_->savedSize()), ConstantInt::get(i32_, i.index),
                   addr(ops[0])});
    if (fnRet_) setNil(fnRet_);
    b_.CreateBr(fnExit_);
    break;
  }
}

// An object of the module's class `i.classIndex`: made here, in the thread's hole, when the class publishes how
// (xb_class_make in xbrt.h: an object that takes nothing but its memory) and the object fits; else by
// xb_class_instance, which prepares the class (its first object), runs what an object takes, or allocates elsewhere.
// Nothing collects between the allocation and the store of the object into its home.
void CodeGen::emitClassNew(const xir::Instruction &i) {
  Value *def = classDef(i.classIndex);
  BasicBlock *ready = newBlock("new.ready"), *slow = newBlock("new.call"), *done = newBlock("new.done");
  auto *make = b_.CreateLoad(ptrTy_, b_.CreateStructGEP(classDefTy_, def, ClassDefMake), "class.make");
  make->setAtomic(AtomicOrdering::Acquire);
  make->setAlignment(Align(dl_.getPointerABIAlignment(0)));
  branchLikely(b_.CreateIsNotNull(make), ready, slow);
  b_.SetInsertPoint(ready);
  Value *nvars = loadFrom(i32_, b_.CreateStructGEP(classMakeTy_, make, ClassMakeVars), Mem::Unknown, "nvars");
  // A count the compiler knows makes the copy of the model one of known size, inline; the published one must agree.
  if (uint32_t known = classVarCounts_.at(size_t(i.classIndex))) {
    BasicBlock *agrees = newBlock("new.count");
    Value *count = ConstantInt::get(i32_, known);
    branchLikely(b_.CreateICmpEQ(nvars, count), agrees, slow);
    b_.SetInsertPoint(agrees);
    nvars = count;
  }
  uint64_t varsAt = dl_.getStructLayout(objectTy_)->getElementOffset(ObjectVars);
  Value *bytes = b_.CreateMul(b_.CreateZExt(nvars, intPtrTy_), ConstantInt::get(intPtrTy_, itemSize_));
  Value *size = b_.CreateAdd(bytes, ConstantInt::get(intPtrTy_, varsAt), "object.size");
  // Each slot that holds a new empty array takes one, made right after the object in the same bump, while the model's
  // array there is empty still (an INIT value the program may hold and fill: the runtime copies what it holds then).
  Value *empties = loadFrom(i32_, b_.CreateStructGEP(classMakeTy_, make, ClassMakeEmpties), Mem::Unknown, "nempty");
  Value *model = loadFrom(ptrTy_, b_.CreateStructGEP(classMakeTy_, make, ClassMakeModel), Mem::Unknown, "model");
  Value *list = loadFrom(ptrTy_, b_.CreateStructGEP(classMakeTy_, make, ClassMakeEmpty), Mem::Unknown, "empty");
  {
    BasicBlock *pre = b_.GetInsertBlock(), *check = newBlock("new.check"), *next = newBlock("new.check.next");
    BasicBlock *alloc = newBlock("new.alloc");
    b_.CreateCondBr(b_.CreateICmpNE(empties, ConstantInt::get(i32_, 0)), check, alloc);
    b_.SetInsertPoint(check);
    PHINode *j = b_.CreatePHI(i32_, 2, "j");
    j->addIncoming(ConstantInt::get(i32_, 0), pre);
    Value *slotNo = loadFrom(i32_, b_.CreateGEP(i32_, list, j), Mem::Unknown, "slot");
    Value *held = itemPointer(b_.CreateGEP(itemTy_, model, slotNo), "model.array");
    Value *len = loadFrom(intPtrTy_, b_.CreateStructGEP(arrayTy_, held, ArrayLen), Mem::ArrayHeader, "model.len");
    branchLikely(b_.CreateICmpEQ(len, ConstantInt::get(intPtrTy_, 0)), next, slow);
    b_.SetInsertPoint(next);
    Value *after = b_.CreateAdd(j, ConstantInt::get(i32_, 1));
    j->addIncoming(after, next);
    b_.CreateCondBr(b_.CreateICmpULT(after, empties), check, alloc);
    b_.SetInsertPoint(alloc);
  }
  uint64_t hdr = dl_.getTypeAllocSize(gcHdrTy_), itemsAt = arrayItemsOffset();
  uint64_t emptyBytes = hdr + alignTo(itemsAt, XB_GC_ALIGN);
  Value *extra = b_.CreateMul(b_.CreateZExt(empties, intPtrTy_), ConstantInt::get(intPtrTy_, emptyBytes));
  Value *birth = nullptr;
  Value *obj = allocateInHole(size, XB_GC_OBJECT, 0, slow, extra, &birth);
  Value *cls = loadFrom(ptrTy_, b_.CreateStructGEP(classMakeTy_, make, ClassMakeCls), Mem::Unknown, "cls");
  tag(b_.CreateStore(cls, b_.CreateStructGEP(objectTy_, obj, ObjectCls)), Mem::ObjectHeader);
  tag(b_.CreateStore(nvars, b_.CreateStructGEP(objectTy_, obj, ObjectNvars)), Mem::ObjectHeader);
  Value *vars = b_.CreateStructGEP(objectTy_, obj, ObjectVars);
  b_.CreateMemCpy(vars, itemAlign_, model, itemAlign_, bytes);
  BasicBlock *arrays = newBlock("new.arrays"), *made = newBlock("new.made");
  BasicBlock *from = b_.GetInsertBlock();
  b_.CreateCondBr(b_.CreateICmpNE(empties, ConstantInt::get(i32_, 0)), arrays, made);
  b_.SetInsertPoint(arrays);
  PHINode *k = b_.CreatePHI(i32_, 2, "k");
  k->addIncoming(ConstantInt::get(i32_, 0), from);
  Value *first = b_.CreateGEP(i8_, obj, roundToAlign(size));
  Value *offset = b_.CreateMul(b_.CreateZExt(k, intPtrTy_), ConstantInt::get(intPtrTy_, emptyBytes));
  Value *at = b_.CreateGEP(i8_, first, offset);
  writeGcHeader(at, ConstantInt::get(intPtrTy_, itemsAt), XB_GC_ARRAY, uint8_t(XB_ELEMS_NIL << XB_GC_STATE_ELEMS_SHIFT),
                birth);
  Value *a = b_.CreateConstInBoundsGEP1_64(i8_, at, hdr, "empty.array");
  Value *zero = ConstantInt::get(intPtrTy_, 0);
  tag(b_.CreateStore(zero, b_.CreateStructGEP(arrayTy_, a, ArrayLen)), Mem::ArrayHeader);
  tag(b_.CreateStore(zero, b_.CreateStructGEP(arrayTy_, a, ArrayCap)), Mem::ArrayHeader);
  tag(b_.CreateStore(b_.CreateConstInBoundsGEP1_64(i8_, a, itemsAt), b_.CreateStructGEP(arrayTy_, a, ArrayData)),
      Mem::ArrayHeader);
  Value *slotNo = loadFrom(i32_, b_.CreateGEP(i32_, list, k), Mem::Unknown, "slot");
  storeTagged(b_.CreateGEP(itemTy_, vars, slotNo), XB_T_ARRAY, a);
  Value *next = b_.CreateAdd(k, ConstantInt::get(i32_, 1));
  k->addIncoming(next, b_.GetInsertBlock());
  b_.CreateCondBr(b_.CreateICmpULT(next, empties), arrays, made);
  b_.SetInsertPoint(made);
  storeTagged(dest(i), XB_T_OBJECT, obj);
  b_.CreateBr(done);
  b_.SetInsertPoint(slow);
  CallResult r = callResult(dest(i), 0);
  setNil(r.arg);
  b_.CreateCall(rtClassInstance_, {fnCtx_, r.arg, def});
  callDone(r);
  checkPending(unwind_);
  b_.CreateBr(done);
  b_.SetInsertPoint(done);
}

// An array literal whose form the types of its elements tell (xb_array_new's rule: an empty one holds NILs, one with
// an element that is surely neither NIL, a logical nor an integer holds items), made in the thread's hole when it fits:
// false when it is left to xb_array_new. Nothing collects between the allocation and the store into its home.
bool CodeGen::emitArrayLiteral(const xir::Instruction &i) {
  const auto &ops = i.operands();
  if (i.hasRest()) return false;
  constexpr xir::TypeSet compact =
      xir::typeBit(XB_T_NIL) | xir::typeBit(XB_T_LOGICAL) | xir::typeBit(XB_T_INT);
  bool items = std::any_of(ops.begin(), ops.end(), [&](const xir::Value *v) {
    xir::TypeSet t = plan_->types().of(v);
    return t != 0 && (t & compact) == 0;
  });
  if (!ops.empty() && !items) return false;
  uint64_t itemsAt = arrayItemsOffset();
  uint64_t bytes = ops.size() * itemSize_;
  if (dl_.getTypeAllocSize(gcHdrTy_) + alignTo(itemsAt + bytes, XB_GC_ALIGN) > XB_GC_SMALL_MAX) return false;
  Value *elems = operandGroup(i);
  BasicBlock *slow = newBlock("array.call"), *done = newBlock("array.done");
  uint32_t form = ops.empty() ? XB_ELEMS_NIL : XB_ELEMS_ITEMS;
  Value *a = allocateInHole(ConstantInt::get(intPtrTy_, itemsAt + bytes), XB_GC_ARRAY,
                            uint8_t(form << XB_GC_STATE_ELEMS_SHIFT), slow);
  Value *len = ConstantInt::get(intPtrTy_, ops.size());
  tag(b_.CreateStore(len, b_.CreateStructGEP(arrayTy_, a, ArrayLen)), Mem::ArrayHeader);
  tag(b_.CreateStore(len, b_.CreateStructGEP(arrayTy_, a, ArrayCap)), Mem::ArrayHeader);
  Value *data = b_.CreateConstInBoundsGEP1_64(i8_, a, itemsAt, "inline.items");
  tag(b_.CreateStore(data, b_.CreateStructGEP(arrayTy_, a, ArrayData)), Mem::ArrayHeader);
  if (!ops.empty()) b_.CreateMemCpy(data, itemAlign_, elems, itemAlign_, bytes);
  storeTagged(dest(i), XB_T_ARRAY, a);
  b_.CreateBr(done);
  b_.SetInsertPoint(slow);
  b_.CreateCall(rtArrayNew_, {fnCtx_, dest(i), ConstantInt::get(i32_, ops.size()), elems});
  b_.CreateBr(done);
  b_.SetInsertPoint(done);
  return true;
}

// Where an array's own items start (xb_array.inline_items, after len, cap and data).
uint64_t CodeGen::arrayItemsOffset() const {
  return alignTo(dl_.getTypeAllocSize(arrayTy_), itemAlign_.value());
}

// `size` rounded up to the collector's alignment, as the hole takes an object (xbrt.h).
Value *CodeGen::roundToAlign(Value *size) {
  return b_.CreateAnd(b_.CreateAdd(size, ConstantInt::get(intPtrTy_, XB_GC_ALIGN - 1)),
                      ConstantInt::getSigned(intPtrTy_, -int64_t(XB_GC_ALIGN)));
}

// The header of an object of `size` bytes at `at`: {size, kind, state, birth} (xb_gc_hdr in xbrt.h).
void CodeGen::writeGcHeader(Value *at, Value *size, xb_gc_kind kind, uint8_t state, Value *birth) {
  tag(b_.CreateStore(b_.CreateTrunc(size, i32_), b_.CreateStructGEP(gcHdrTy_, at, GcHdrSize)), Mem::GcState);
  tag(b_.CreateStore(ConstantInt::get(i8_, kind), b_.CreateStructGEP(gcHdrTy_, at, GcHdrKind)), Mem::GcState);
  tag(b_.CreateStore(ConstantInt::get(i8_, state), b_.CreateStructGEP(gcHdrTy_, at, GcHdrState)), Mem::GcState);
  tag(b_.CreateStore(birth, b_.CreateStructGEP(gcHdrTy_, at, GcHdrFlags)), Mem::GcState);
}

// `size` bytes of an object of `kind` taken from the thread's hole (xb_tlab in xbrt.h), its state byte `state`: its
// address, past the header written there; a branch to `slow` when the thread has no hole, the object is too large for
// one, or the hole has no room left for it. `extra` (header and alignment included) is taken right after it too, in
// the same bump, for the caller's objects there (`birth`: the flags their headers take).
Value *CodeGen::allocateInHole(Value *size, xb_gc_kind kind, uint8_t state, BasicBlock *slow, Value *extra,
                               Value **birth) {
  uint64_t hdr = dl_.getTypeAllocSize(gcHdrTy_);
  Value *own = b_.CreateAdd(roundToAlign(size), ConstantInt::get(intPtrTy_, hdr), "object.total");
  Value *total = extra ? b_.CreateAdd(own, extra) : own;
  Value *tlab = loadFrom(ptrTy_, b_.CreateStructGEP(ctxHeadTy_, fnCtx_, CtxTlab), Mem::Ctx, "tlab");
  BasicBlock *have = newBlock("hole"), *fits = newBlock("hole.fits");
  Value *small = b_.CreateICmpULE(own, ConstantInt::get(intPtrTy_, XB_GC_SMALL_MAX));
  branchLikely(b_.CreateAnd(b_.CreateIsNotNull(tlab), small), have, slow);
  b_.SetInsertPoint(have);
  Value *cursorAt = b_.CreateStructGEP(tlabTy_, tlab, TlabCursor);
  Value *cursor = loadFrom(ptrTy_, cursorAt, Mem::Ctx, "cursor");
  Value *limit = loadFrom(ptrTy_, b_.CreateStructGEP(tlabTy_, tlab, TlabLimit), Mem::Ctx, "limit");
  Value *room = b_.CreateSub(b_.CreatePtrToInt(limit, intPtrTy_), b_.CreatePtrToInt(cursor, intPtrTy_));
  branchLikely(b_.CreateICmpUGE(room, total), fits, slow);
  b_.SetInsertPoint(fits);
  tag(b_.CreateStore(b_.CreateGEP(i8_, cursor, total), cursorAt), Mem::Ctx);
  Value *allocatedAt = b_.CreateStructGEP(tlabTy_, tlab, TlabAllocated);
  Value *allocated = loadFrom(intPtrTy_, allocatedAt, Mem::Ctx, "allocated");
  tag(b_.CreateStore(b_.CreateAdd(allocated, total), allocatedAt), Mem::Ctx);
  Value *flags = loadFrom(i16_, b_.CreateStructGEP(tlabTy_, tlab, TlabBirth), Mem::Ctx, "birth");
  if (birth) *birth = flags;
  writeGcHeader(cursor, size, kind, state, flags);
  return b_.CreateGEP(i8_, cursor, ConstantInt::get(intPtrTy_, hdr), "object");
}

// block: the block made first, into an item of its own (blockTemp_) kept as a C root while its cells are obtained, each
// stored in it at once: obtaining one may allocate (a variable's first cell, a new one holding the current element of a
// FOR EACH variable or a value captured, which only the block refers to) and so collect, which may move the block (read
// again from its item) or age it (the barrier test after the stores). It goes to its home last, which may be the slot
// of a variable that moved into its cell meanwhile.
void CodeGen::emitMakeBlock(const xir::Instruction &i) {
  if (!blockTemp_) blockTemp_ = entryAlloca("block.tmp");
  size_t ncells = i.valueCells.empty() ? i.captures.size() : i.valueCells.size();
  b_.CreateCall(rtBlockNew_, {fnCtx_, blockTemp_, functions_.at(i.blockFn), ConstantInt::get(i32_, ncells)});
  b_.CreateCall(rtRootPush_, {fnCtx_, blockTemp_});
  size_t var = 0, value = 0;
  for (uint32_t k = 0; k < ncells; ++k) {
    Value *cell;
    if (!i.valueCells.empty() && i.valueCells[k]) {
      cell = b_.CreateCall(rtCellNew_, {fnCtx_, callArg(addr(i.operand(value++)), 0)}, "cell");
    } else {
      const xir::Var *c = i.captures[var++];
      cell = c->kind == xir::Var::Kind::Capture ? capturedCell(*c)
                                                 : b_.CreateCall(rtCellOf_, {fnCtx_, rawAddress(*c), isStaticArg(*c)},
                                                                 "cell");
    }
    tag(b_.CreateStore(cell, capturedCellAddress(itemPointer(blockTemp_, "block"), k)), Mem::BlockCells);
  }
  Value *block = itemPointer(blockTemp_, "block");
  recordIfOld(block, stateByte(block, AtomicOrdering::Monotonic));
  b_.CreateCall(rtRootPop_, {fnCtx_, ConstantInt::get(i32_, 1)});
  copyItem(dest(i), blockTemp_);
}

} // namespace xb
