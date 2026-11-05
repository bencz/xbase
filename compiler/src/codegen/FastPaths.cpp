// Inline fast paths of operators, of element access and of the bit functions:
// the number cases of xb_op_add, xb_op_sub, xb_op_mul, xb_op_div, xb_op_mod,
// the comparisons and xb_for_test (core/ops.c), xb_op_index and
// xb_op_index_set (types/array.c) and xb_bit_apply, done in place; every
// other case, an integer overflow and an index out of bounds included, calls
// the runtime, which raises the errors.
#include "codegen/CodeGen.h"

#include "common/BitFunctions.h"

#include "xbrt.h" // constants only (type tags)

#include <llvm/IR/Constants.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/MDBuilder.h>

using namespace llvm;

namespace xb {

Value *CodeGen::isInt(Value *item) {
  Value *type = itemType(item);
  return b_.CreateICmpEQ(type, ConstantInt::get(i32_, XB_T_INT));
}

Value *CodeGen::intValue(Value *item) { return b_.CreateLoad(i64_, b_.CreateStructGEP(itemTy_, item, FValue), "int"); }

// A computed integer: no display width or decimals of its own (they come
// from the value when it is shown), as xb_set_int makes it.
void CodeGen::storeInt(Value *dst, Value *v) { storeTagged(dst, XB_T_INT, v); }

// A double with `dec` decimals (i16) and no width of its own, as
// xb_set_double makes it.
void CodeGen::storeDouble(Value *dst, Value *v, Value *dec) {
  Value *header =
      b_.CreateOr(headerWord(XB_T_DOUBLE, 0, 0), b_.CreateShl(b_.CreateZExt(dec, i64_), headerShift(FDec)));
  storeWords(dst, header, payloadWord(v));
}

// Branches to `fast` when `cond` holds (likely), else to `slow`.
void CodeGen::branchLikely(Value *cond, BasicBlock *fast, BasicBlock *slow) {
  MDBuilder md(ctx_);
  b_.CreateCondBr(cond, fast, slow, md.createLikelyBranchWeights());
}

// A signed integer comparison as an LLVM predicate (in the order of IntPred).
CmpInst::Predicate CodeGen::llvmPredicate(xir::IntPred p) {
  static const CmpInst::Predicate preds[] = {CmpInst::ICMP_EQ,  CmpInst::ICMP_NE,  CmpInst::ICMP_SLT,
                                             CmpInst::ICMP_SLE, CmpInst::ICMP_SGT, CmpInst::ICMP_SGE};
  return preds[(int)p];
}

// The comparisons that have an integer fast path, as integer predicates.
static bool intPredicate(BinOp op, xir::IntPred &pred) {
  switch (op) {
  case BinOp::Eq: case BinOp::ExactEq: pred = xir::IntPred::Eq; return true;
  case BinOp::NotEq: pred = xir::IntPred::Ne; return true;
  case BinOp::Lt: pred = xir::IntPred::Lt; return true;
  case BinOp::Le: pred = xir::IntPred::Le; return true;
  case BinOp::Gt: pred = xir::IntPred::Gt; return true;
  case BinOp::Ge: pred = xir::IntPred::Ge; return true;
  default: return false;
  }
}

static Intrinsic::ID overflowIntrinsic(BinOp op) {
  switch (op) {
  case BinOp::Add: return Intrinsic::sadd_with_overflow;
  case BinOp::Sub: return Intrinsic::ssub_with_overflow;
  case BinOp::Mul: return Intrinsic::smul_with_overflow;
  default: return Intrinsic::not_intrinsic;
  }
}

// The comparisons of a block whose only use is the truth test right after
// them: the test takes the comparison's boolean, without the logical item in
// between. Found once per block, before its instructions are lowered.
void CodeGen::findFusedComparisons(const xir::BasicBlock &b) {
  fusedComparisons_.clear();
  for (size_t k = 0; k + 1 < b.insts.size(); ++k) {
    const xir::Instruction &i = *b.insts[k], &next = *b.insts[k + 1];
    xir::IntPred pred;
    if (i.op == xir::Op::Binary && intPredicate(i.binop, pred) && i.hasOneUse() && i.users().front() == &next &&
        next.op == xir::Op::Truth)
      fusedComparisons_.insert(&i);
  }
}

Value *CodeGen::isNumberType(Value *type) {
  return b_.CreateOr(b_.CreateICmpEQ(type, ConstantInt::get(i32_, XB_T_INT)),
                     b_.CreateICmpEQ(type, ConstantInt::get(i32_, XB_T_DOUBLE)));
}

// A number item that converts to a double exactly: a double, or an integer
// of at most 2^53 in magnitude. Larger integers go to the runtime, which
// converts them, so that the result does not depend on how a target keeps
// doubles in its registers.
Value *CodeGen::exactAsDouble(Value *item, Value *type) {
  Value *small = b_.CreateICmpULE(b_.CreateAdd(intValue(item), ConstantInt::get(i64_, 1ull << 53)),
                                  ConstantInt::get(i64_, 1ull << 54));
  return b_.CreateOr(b_.CreateICmpNE(type, ConstantInt::get(i32_, XB_T_INT)), small);
}

Value *CodeGen::bothInts(Value *ta, Value *tb) {
  Value *intTag = ConstantInt::get(i32_, XB_T_INT);
  return b_.CreateAnd(b_.CreateICmpEQ(ta, intTag), b_.CreateICmpEQ(tb, intTag));
}

// Two numbers that convert to doubles exactly (the operands of the double
// paths), of types `ta` and `tb`.
Value *CodeGen::exactNumbers(Value *a, Value *ta, Value *b, Value *tb) {
  return b_.CreateAnd(b_.CreateAnd(isNumberType(ta), isNumberType(tb)),
                      b_.CreateAnd(exactAsDouble(a, ta), exactAsDouble(b, tb)));
}

// The decimals of a number item of type `type` (an integer has none).
Value *CodeGen::numberDec(Value *item, Value *type) {
  Value *dec = b_.CreateTrunc(b_.CreateLShr(loadHeader(item), headerShift(FDec)), i16_, "dec");
  return b_.CreateSelect(b_.CreateICmpEQ(type, ConstantInt::get(i32_, XB_T_DOUBLE)), dec, b_.getInt16(0));
}

// The value of a number item of type `type` as a double.
Value *CodeGen::numberAsDouble(Value *item, Value *type) {
  Value *bits = intValue(item);
  return b_.CreateSelect(b_.CreateICmpEQ(type, ConstantInt::get(i32_, XB_T_INT)),
                         b_.CreateSIToFP(bits, b_.getDoubleTy()), b_.CreateBitCast(bits, b_.getDoubleTy()));
}

// A comparison of two numbers that are not both integers, as xb_compare makes
// it: c = (x > y) - (x < y), so NaN compares as equal to anything.
static Value *doubleCompare(IRBuilder<> &b, xir::IntPred pred, Value *x, Value *y) {
  Value *gt = b.CreateFCmpOGT(x, y), *lt = b.CreateFCmpOLT(x, y);
  switch (pred) {
  case xir::IntPred::Eq: return b.CreateNot(b.CreateOr(gt, lt));
  case xir::IntPred::Ne: return b.CreateOr(gt, lt);
  case xir::IntPred::Lt: return lt;
  case xir::IntPred::Le: return b.CreateNot(gt);
  case xir::IntPred::Gt: return gt;
  case xir::IntPred::Ge: return b.CreateNot(lt);
  }
  return nullptr;
}

// +, -, * and the comparisons: two integers (no overflow) and two numbers of
// which one is a double are done here, with xb_op_add's, xb_op_sub's,
// xb_op_mul's and xb_compare's rules; everything else calls the operator.
void CodeGen::emitBinary(const xir::Instruction &i) {
  Value *a = addr(i.operand(0));
  Value *bv = addr(i.operand(1));
  Value *d = dest(i);
  xir::IntPred pred;
  Intrinsic::ID arith = overflowIntrinsic(i.binop);
  bool compare = intPredicate(i.binop, pred);
  if (i.binop == BinOp::Div || i.binop == BinOp::Mod) {
    emitDivMod(i, a, bv, d);
    return;
  }
  if (arith == Intrinsic::not_intrinsic && !compare) {
    callItemOp(rtBin_.at(i.binop), d, {a, bv});
    checkPending(fnExit_);
    return;
  }
  bool fused = fusedComparisons_.count(&i) != 0;
  BasicBlock *intBB = newBlock("int"), *notInt = newBlock("not.int"), *numBB = newBlock("num");
  BasicBlock *slow = newBlock("op"), *join = newBlock("join");
  Value *ta = typeOf(i.operand(0), a);
  Value *tb = typeOf(i.operand(1), bv);
  branchLikely(bothInts(ta, tb), intBB, notInt);
  b_.SetInsertPoint(notInt);
  branchLikely(exactNumbers(a, ta, bv, tb), numBB, slow);

  // Both operands are read before `d`, which may be one of them, is written.
  std::vector<std::pair<Value *, BasicBlock *>> bools; // the comparison's result on each fast path
  b_.SetInsertPoint(intBB);
  Value *x = intValue(a), *y = intValue(bv);
  if (compare) {
    Value *r = b_.CreateICmp(llvmPredicate(pred), x, y);
    if (!fused) storeLogical(d, r);
    bools.push_back({r, b_.GetInsertBlock()});
  } else {
    Value *r = b_.CreateBinaryIntrinsic(arith, x, y);
    BasicBlock *done = newBlock("int.done");
    MDBuilder md(ctx_);
    b_.CreateCondBr(b_.CreateExtractValue(r, 1), slow, done, md.createUnlikelyBranchWeights()); // overflow
    b_.SetInsertPoint(done);
    storeInt(d, b_.CreateExtractValue(r, 0));
  }
  b_.CreateBr(join);

  b_.SetInsertPoint(numBB);
  Value *fx = numberAsDouble(a, ta), *fy = numberAsDouble(bv, tb);
  if (compare) {
    Value *r = doubleCompare(b_, pred, fx, fy);
    if (!fused) storeLogical(d, r);
    bools.push_back({r, b_.GetInsertBlock()});
  } else {
    Value *da = numberDec(a, ta), *db = numberDec(bv, tb);
    Value *dec = i.binop == BinOp::Mul ? b_.CreateAdd(da, db) // wraps at 16 bits, as the runtime's does
                                       : b_.CreateSelect(b_.CreateICmpUGT(da, db), da, db);
    Value *r = i.binop == BinOp::Add   ? b_.CreateFAdd(fx, fy)
               : i.binop == BinOp::Sub ? b_.CreateFSub(fx, fy)
                                       : b_.CreateFMul(fx, fy);
    storeDouble(d, r, dec);
  }
  b_.CreateBr(join);

  b_.SetInsertPoint(slow);
  callItemOp(rtBin_.at(i.binop), d, {a, bv});
  checkPending(fnExit_);
  if (fused) bools.push_back({logicalValue(d), b_.GetInsertBlock()}); // the operator made a logical
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
  if (fused) {
    PHINode *phi = b_.CreatePHI(b_.getInt1Ty(), (unsigned)bools.size(), "cmp");
    for (auto &[v, from] : bools) phi->addIncoming(v, from);
    fusedTruth_[i.users().front()] = phi;
  }
}

// A call to one of xbase's bit functions (xb_bit_function in xbrt.h) with integer
// arguments is the machine operation (xb_bit_apply's rules); other arguments
// go to the runtime function. Returns false for any other call.
bool CodeGen::emitBitCall(const xir::Instruction &i) {
  if (i.callee.function || !i.callee.native) return false;
  const xb_bit_function *f = findBitFunction(i.callee.name);
  if (!f || i.operands().size() != f->arity) return false;
  Value *argv = operandGroup(i);
  Value *d = dest(i);
  Value *allInt = isInt(slot(plan_->groupBase(&i)));
  for (unsigned k = 1; k < f->arity; ++k) allInt = b_.CreateAnd(allInt, isInt(slot(plan_->groupBase(&i) + k)));
  BasicBlock *fast = newBlock("int"), *slow = newBlock("call"), *join = newBlock("join");
  branchLikely(allInt, fast, slow);
  b_.SetInsertPoint(fast);
  Value *x = intValue(slot(plan_->groupBase(&i)));
  Value *y = f->arity > 1 ? intValue(slot(plan_->groupBase(&i) + 1)) : nullptr;
  Value *r = nullptr;
  auto shift = [&](bool left) { // a count outside 0..63 gives 0
    Value *inRange = b_.CreateICmpULT(y, ConstantInt::get(i64_, 64));
    Value *count = b_.CreateAnd(y, ConstantInt::get(i64_, 63));
    return b_.CreateSelect(inRange, left ? b_.CreateShl(x, count) : b_.CreateLShr(x, count), ConstantInt::get(i64_, 0));
  };
  switch (f->op) {
  case XB_BIT_AND: r = b_.CreateAnd(x, y); break;
  case XB_BIT_OR: r = b_.CreateOr(x, y); break;
  case XB_BIT_XOR: r = b_.CreateXor(x, y); break;
  case XB_BIT_NOR: r = b_.CreateNot(b_.CreateOr(x, y)); break;
  case XB_BIT_NOT: r = b_.CreateNot(x); break;
  case XB_BIT_SHL: r = shift(true); break;
  case XB_BIT_SHR: r = shift(false); break;
  }
  storeInt(d, r);
  b_.CreateBr(join);
  b_.SetInsertPoint(slow);
  setNil(d);
  b_.CreateCall(xbFuncTy_, externFunction(i.callee), {fnCtx_, d, ConstantInt::get(i32_, f->arity), argv});
  checkPending(fnExit_);
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
  return true;
}

// `/` and `%` of two numbers and a divisor other than 0 (-0 included): a
// double with SET DECIMALS decimals, as xb_op_div and xb_op_mod make it; `%`
// of two integers is their remainder (0 for a divisor of -1). Anything else
// calls the operator, which raises the errors.
void CodeGen::emitDivMod(const xir::Instruction &i, Value *a, Value *bv, Value *d) {
  BasicBlock *numBB = newBlock("num"), *done = newBlock("num.done"), *slow = newBlock("op"), *join = newBlock("join");
  Value *ta = typeOf(i.operand(0), a), *tb = typeOf(i.operand(1), bv);
  branchLikely(exactNumbers(a, ta, bv, tb), numBB, slow);
  b_.SetInsertPoint(numBB);
  Value *fx = numberAsDouble(a, ta), *fy = numberAsDouble(bv, tb);
  Value *r;
  if (i.binop == BinOp::Div) {
    r = b_.CreateFDiv(fx, fy);
  } else {
    Value *xi = intValue(a), *yi = intValue(bv);
    Value *minus1 = b_.CreateICmpEQ(yi, ConstantInt::getSigned(i64_, -1));
    Value *safe = b_.CreateSelect(b_.CreateOr(minus1, b_.CreateICmpEQ(yi, ConstantInt::get(i64_, 0))),
                                  ConstantInt::get(i64_, 1), yi); // srem must not trap where it is not used
    Value *ir = b_.CreateSelect(minus1, ConstantFP::get(b_.getDoubleTy(), 0.0),
                                b_.CreateSIToFP(b_.CreateSRem(xi, safe), b_.getDoubleTy()));
    r = b_.CreateSelect(bothInts(ta, tb), ir, b_.CreateFRem(fx, fy));
  }
  Value *dec = b_.CreateTrunc(b_.CreateLoad(i32_, b_.CreateStructGEP(ctxHeadTy_, fnCtx_, CtxDecimals)), i16_);
  branchLikely(b_.CreateFCmpUNE(fy, ConstantFP::get(b_.getDoubleTy(), 0.0)), done, slow);
  b_.SetInsertPoint(done);
  storeDouble(d, r, dec);
  b_.CreateBr(join);
  b_.SetInsertPoint(slow);
  callItemOp(rtBin_.at(i.binop), d, {a, bv});
  checkPending(fnExit_);
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
}

// FOR's test with an integer counter, limit and step: a negative step counts
// down (xb_for_test).
void CodeGen::emitForTest(const xir::Instruction &i) {
  Value *counter = addr(i.operand(0)), *limit = addr(i.operand(1)), *step = addr(i.operand(2));
  BasicBlock *fast = newBlock("int"), *slow = newBlock("op"), *join = newBlock("join");
  branchLikely(b_.CreateAnd(b_.CreateAnd(isInt(counter), isInt(limit)), isInt(step)), fast, slow);
  b_.SetInsertPoint(fast);
  Value *c = intValue(counter), *l = intValue(limit);
  Value *down = b_.CreateICmpSLT(intValue(step), ConstantInt::get(i64_, 0));
  Value *fastGo = b_.CreateSelect(down, b_.CreateICmpSGE(c, l), b_.CreateICmpSLE(c, l));
  BasicBlock *fastEnd = b_.GetInsertBlock();
  b_.CreateBr(join);
  b_.SetInsertPoint(slow);
  Value *go = b_.CreateCall(rtForTest_, {fnCtx_, callArg(counter, 0), callArg(limit, 1), callArg(step, 2)});
  checkPending(fnExit_);
  Value *slowGo = b_.CreateICmpNE(go, ConstantInt::get(i32_, 0));
  BasicBlock *slowEnd = b_.GetInsertBlock();
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
  PHINode *phi = b_.CreatePHI(b_.getInt1Ty(), 2, "go");
  phi->addIncoming(fastGo, fastEnd);
  phi->addIncoming(slowGo, slowEnd);
  values_[&i] = phi;
}

// The element base[idx] when base is an array and idx a number within it
// (1-based; a double is truncated, as xb_num_to_i64 does): its address, in the
// block where that holds; any other case branches to `slow`. Also gives the
// array and the 0-based position. An integer index, the usual one, has a path
// of its own.
Value *CodeGen::inlineElement(const xir::Instruction &i, Value *base, Value *idx, BasicBlock *slow, Value *&array,
                               Value *&pos) {
  Value *baseType = typeOf(i.operand(0), base), *idxType = typeOf(i.operand(1), idx);
  Value *isArray = b_.CreateICmpEQ(baseType, ConstantInt::get(i32_, XB_T_ARRAY));
  BasicBlock *intIdx = newBlock("index.int"), *notInt = newBlock("index.other"), *dblIdx = newBlock("index.double");
  BasicBlock *inBounds = newBlock("element");
  branchLikely(b_.CreateAnd(isArray, b_.CreateICmpEQ(idxType, ConstantInt::get(i32_, XB_T_INT))), intIdx, notInt);
  b_.SetInsertPoint(notInt);
  b_.CreateCondBr(b_.CreateAnd(isArray, b_.CreateICmpEQ(idxType, ConstantInt::get(i32_, XB_T_DOUBLE))), dblIdx, slow);
  auto arrayAndLen = [&](Value *&arr) {
    arr = b_.CreateLoad(ptrTy_, b_.CreateStructGEP(itemTy_, base, FValue), "arr");
    return b_.CreateZExtOrTrunc(b_.CreateLoad(intPtrTy_, b_.CreateStructGEP(arrayTy_, arr, ArrayLen)), i64_);
  };
  Value *one = ConstantInt::get(i64_, 1);

  b_.SetInsertPoint(intIdx);
  Value *intArr, *intLen = arrayAndLen(intArr);
  Value *intPos = b_.CreateSub(intValue(idx), one);
  branchLikely(b_.CreateICmpULT(intPos, intLen), inBounds, slow); // 1 <= idx <= len
  BasicBlock *intEnd = b_.GetInsertBlock();

  // A double from 1 to len + 1 (excluded) truncates to a position; NaN and
  // anything else go to the runtime. The position is checked again as an
  // integer: len + 1 rounds for huge arrays.
  b_.SetInsertPoint(dblIdx);
  Value *dblArr, *dblLen = arrayAndLen(dblArr);
  Type *f64 = b_.getDoubleTy();
  Value *d = b_.CreateBitCast(intValue(idx), f64);
  Value *end = b_.CreateFAdd(b_.CreateUIToFP(dblLen, f64), ConstantFP::get(f64, 1.0));
  Value *fits = b_.CreateAnd(b_.CreateFCmpOGE(d, ConstantFP::get(f64, 1.0)), b_.CreateFCmpOLT(d, end));
  Value *dblPos = b_.CreateSub(b_.CreateFPToSI(b_.CreateSelect(fits, d, ConstantFP::get(f64, 1.0)), i64_), one);
  b_.CreateCondBr(b_.CreateAnd(fits, b_.CreateICmpULT(dblPos, dblLen)), inBounds, slow);
  BasicBlock *dblEnd = b_.GetInsertBlock();

  b_.SetInsertPoint(inBounds);
  PHINode *arr = b_.CreatePHI(ptrTy_, 2, "arr");
  arr->addIncoming(intArr, intEnd);
  arr->addIncoming(dblArr, dblEnd);
  PHINode *p = b_.CreatePHI(i64_, 2, "pos");
  p->addIncoming(intPos, intEnd);
  p->addIncoming(dblPos, dblEnd);
  array = arr;
  pos = p;
  Value *items = b_.CreateLoad(ptrTy_, b_.CreateStructGEP(arrayTy_, array, ArrayItems), "items");
  return b_.CreateInBoundsGEP(itemTy_, items, b_.CreateZExtOrTrunc(pos, intPtrTy_), "elem");
}

void CodeGen::emitIndex(const xir::Instruction &i) {
  Value *base = addr(i.operand(0)), *idx = addr(i.operand(1)), *d = dest(i);
  BasicBlock *slow = newBlock("index.call"), *join = newBlock("index.end");
  Value *array, *pos;
  copyItem(d, inlineElement(i, base, idx, slow, array, pos)); // base and idx are read before d is written
  b_.CreateBr(join);
  b_.SetInsertPoint(slow);
  callItemOp(rtIndex_, d, {base, idx});
  checkPending(fnExit_);
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
}

// The store in place needs the write barrier only for a value that refers to
// a heap object (xb_type_is_heap).
void CodeGen::emitIndexSet(const xir::Instruction &i) {
  Value *base = addr(i.operand(0)), *idx = addr(i.operand(1)), *v = addr(i.operand(2));
  BasicBlock *slow = newBlock("index.set.call"), *join = newBlock("index.set.end");
  Value *array, *pos;
  copyItem(inlineElement(i, base, idx, slow, array, pos), v);
  Value *type = typeOf(i.operand(2), v);
  Value *heap = b_.CreateICmpULE(b_.CreateSub(type, ConstantInt::get(i32_, XB_T_HEAP_FIRST)),
                                 ConstantInt::get(i32_, XB_T_HEAP_LAST - XB_T_HEAP_FIRST));
  BasicBlock *barrier = newBlock("barrier");
  b_.CreateCondBr(heap, barrier, join);
  b_.SetInsertPoint(barrier);
  b_.CreateCall(rtWriteItem_, {array, b_.CreateZExtOrTrunc(pos, intPtrTy_)});
  b_.CreateBr(join);
  b_.SetInsertPoint(slow);
  b_.CreateCall(rtIndexSet_, {fnCtx_, callArg(base, 0), callArg(idx, 1), callArg(v, 2)});
  checkPending(fnExit_);
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
}

} // namespace xb
