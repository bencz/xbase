// Inline fast paths of operators, of element access and of the bit
// functions: the number cases of xb_op_add, xb_op_sub, xb_op_mul, xb_op_div,
// xb_op_mod, xb_op_neg, the bit operators, the comparisons and xb_for_test
// (core/ops.c), xb_op_index and xb_op_index_set (types/array.c) and
// xb_bit_apply, done in place; every other case, an integer overflow and an
// index out of bounds included, calls the runtime, which raises the errors.
#include "codegen/CodeGen.h"

#include "common/BitFunctions.h"

#include "xbrt.h" // constants only (type tags)

#include <llvm/IR/Constants.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/MDBuilder.h>

using namespace llvm;

namespace xb {

Value *CodeGen::isInt(Value *item) { return isIntType(itemType(item)); }

Value *CodeGen::isIntType(Value *type) { return b_.CreateICmpEQ(type, ConstantInt::get(i32_, XB_T_INT)); }

// The pointer an item of a heap type holds (a string, an array, a cell, ...).
Value *CodeGen::itemPointer(Value *item, const char *name) {
  return loadFrom(ptrTy_, b_.CreateStructGEP(itemTy_, item, FValue), itemMemory(item), name);
}

// An array's length, as an i64.
Value *CodeGen::arrayLength(Value *array) {
  return b_.CreateZExtOrTrunc(loadFrom(intPtrTy_, b_.CreateStructGEP(arrayTy_, array, ArrayLen), Mem::ArrayHeader),
                              i64_);
}

// The address of 0-based element `pos0` of an array (within its length).
Value *CodeGen::arrayElement(Value *array, Value *pos0) {
  Value *items = loadFrom(ptrTy_, b_.CreateStructGEP(arrayTy_, array, ArrayItems), Mem::ArrayHeader, "items");
  return heapItem(b_.CreateInBoundsGEP(itemTy_, items, b_.CreateZExtOrTrunc(pos0, intPtrTy_), "elem"));
}

Value *CodeGen::intValue(Value *item) {
  if (auto w = wordsAt_.find(item); w != wordsAt_.end()) return w->second.payload;
  return loadFrom(i64_, b_.CreateStructGEP(itemTy_, item, FValue), itemMemory(item), "int");
}

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

// Branches to `fast` when `cond` holds (likely), else to `slow`. The edge to
// `slow` is cold: splitColdLoopPaths moves it out of loops. Edges to runtime
// calls are weighted so; a choice between two inline paths is a plain branch
// unless one of them is rare.
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

Value *CodeGen::bothInts(Value *ta, Value *tb) { return b_.CreateAnd(isIntType(ta), isIntType(tb)); }

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

// A comparison of two numbers that are not both integers, as xb_compare and
// xb_order_is make it: IEEE 754's, every comparison with NaN false but `!=`.
static Value *doubleCompare(IRBuilderBase &b, xir::IntPred pred, Value *x, Value *y) {
  switch (pred) {
  case xir::IntPred::Eq: return b.CreateFCmpOEQ(x, y);
  case xir::IntPred::Ne: return b.CreateFCmpUNE(x, y);
  case xir::IntPred::Lt: return b.CreateFCmpOLT(x, y);
  case xir::IntPred::Le: return b.CreateFCmpOLE(x, y);
  case xir::IntPred::Gt: return b.CreateFCmpOGT(x, y);
  case xir::IntPred::Ge: return b.CreateFCmpOGE(x, y);
  }
  return nullptr;
}

// +, -, * and the comparisons: two integers (no overflow) and two numbers of
// which one is a double are done here, with xb_op_add's, xb_op_sub's,
// xb_op_mul's and xb_compare's rules; everything else calls the operator.
void CodeGen::emitBinary(const xir::Instruction &i) {
  if (const xb_bit_function *f = bitOperator(i.binop)) {
    emitBitOperator(i, *f);
    return;
  }
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
    checkPending(unwind_);
    return;
  }
  bool fused = fusedComparisons_.count(&i) != 0;
  // The paths the operands' types allow (what type inference knows of them): two integers, two doubles, a double
  // with an integer, two strings for an equality; the others are not emitted.
  xir::TypeSet typesA = plan_->types().of(i.operand(0)), typesB = plan_->types().of(i.operand(1));
  auto both = [&](xb_type x, xb_type y) { return (typesA & xir::typeBit(x)) && (typesB & xir::typeBit(y)); };
  bool mayInt = both(XB_T_INT, XB_T_INT);
  bool mayDouble = both(XB_T_DOUBLE, XB_T_DOUBLE);
  bool mayMixed = both(XB_T_INT, XB_T_DOUBLE) || both(XB_T_DOUBLE, XB_T_INT);
  bool equality = i.binop == BinOp::Eq || i.binop == BinOp::ExactEq || i.binop == BinOp::NotEq;
  bool mayStrings = equality && both(XB_T_STRING, XB_T_STRING);
  BasicBlock *intBB = mayInt ? newBlock("int") : nullptr;
  BasicBlock *dblBB = mayDouble ? newBlock("double") : nullptr;
  BasicBlock *numBB = mayMixed ? newBlock("num") : nullptr;
  BasicBlock *slow = newBlock("op"), *join = newBlock("join");
  Value *ta = typeOf(i.operand(0), a);
  Value *tb = typeOf(i.operand(1), bv);
  if (mayInt) { // both paths are fast: neither is cold
    BasicBlock *notInt = newBlock("not.int");
    b_.CreateCondBr(bothInts(ta, tb), intBB, notInt);
    b_.SetInsertPoint(notInt);
  }
  Value *isDouble = ConstantInt::get(i32_, XB_T_DOUBLE);
  if (mayDouble) {
    BasicBlock *mixed = newBlock("mixed");
    b_.CreateCondBr(b_.CreateAnd(b_.CreateICmpEQ(ta, isDouble), b_.CreateICmpEQ(tb, isDouble)), dblBB, mixed);
    b_.SetInsertPoint(mixed);
  }
  // Both operands are read before `d`, which may be one of them, is written.
  std::vector<std::pair<Value *, BasicBlock *>> bools; // the comparison's result on each fast path
  if (mayStrings) { // two one-character strings (ValType( x ) == "C"): their bytes, for = and == alike
    BasicBlock *strBB = newBlock("str1"), *notStr = newBlock("not.str1");
    Value *isStr = ConstantInt::get(i32_, XB_T_STRING);
    BasicBlock *strings = newBlock("strings");
    b_.CreateCondBr(b_.CreateAnd(b_.CreateICmpEQ(ta, isStr), b_.CreateICmpEQ(tb, isStr)), strings, notStr);
    b_.SetInsertPoint(strings);
    Value *sa = itemPointer(a, "sa"), *sb = itemPointer(bv, "sb");
    Value *one = ConstantInt::get(i64_, 1);
    b_.CreateCondBr(b_.CreateAnd(b_.CreateICmpEQ(strLength(sa), one), b_.CreateICmpEQ(strLength(sb), one)), strBB,
                    slow);
    b_.SetInsertPoint(strBB);
    Value *same = b_.CreateICmpEQ(strByte(sa, ConstantInt::get(i64_, 0)), strByte(sb, ConstantInt::get(i64_, 0)));
    Value *r = i.binop == BinOp::NotEq ? b_.CreateNot(same) : same;
    if (!fused) storeLogical(d, r);
    bools.push_back({r, b_.GetInsertBlock()});
    b_.CreateBr(join);
    b_.SetInsertPoint(notStr);
  }
  if (mayMixed) branchLikely(exactNumbers(a, ta, bv, tb), numBB, slow); // an integer with a double: when exact
  else b_.CreateBr(slow);
  if (mayInt) {
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
  }

  // The operation on two doubles `fx`, `fy` with decimals `da`, `db`.
  auto doubles = [&](Value *fx, Value *fy, Value *da, Value *db) {
    if (compare) {
      Value *r = doubleCompare(b_, pred, fx, fy);
      if (!fused) storeLogical(d, r);
      bools.push_back({r, b_.GetInsertBlock()});
    } else {
      Value *dec = i.binop == BinOp::Mul ? b_.CreateAdd(da, db) // wraps at 16 bits, as the runtime's does
                                         : b_.CreateSelect(b_.CreateICmpUGT(da, db), da, db);
      Value *r = i.binop == BinOp::Add   ? b_.CreateFAdd(fx, fy)
                 : i.binop == BinOp::Sub ? b_.CreateFSub(fx, fy)
                                         : b_.CreateFMul(fx, fy);
      storeDouble(d, r, dec);
    }
    b_.CreateBr(join);
  };
  if (mayDouble) { // two doubles, the usual case of the double paths: their bits and decimals as they are
    b_.SetInsertPoint(dblBB);
    doubles(b_.CreateBitCast(intValue(a), b_.getDoubleTy()), b_.CreateBitCast(intValue(bv), b_.getDoubleTy()),
            numberDec(a, isDouble), numberDec(bv, isDouble));
  }
  if (mayMixed) {
    b_.SetInsertPoint(numBB);
    doubles(numberAsDouble(a, ta), numberAsDouble(bv, tb), numberDec(a, ta), numberDec(bv, tb));
  }

  b_.SetInsertPoint(slow);
  if (i.byOne) callItemOp(i.binop == BinOp::Add ? rtInc_ : rtDec_, d, {a}); // ++ or --: their own error
  else callItemOp(rtBin_.at(i.binop), d, {a, bv});
  checkPending(unwind_);
  if (fused) { // a logical, unless an error's handler substituted something else
    Value *r = testTruth(d, unwind_, itemType(d));
    bools.push_back({r, b_.GetInsertBlock()});
  }
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
  if (fused) {
    PHINode *phi = b_.CreatePHI(b_.getInt1Ty(), (unsigned)bools.size(), "cmp");
    for (auto &[v, from] : bools) phi->addIncoming(v, from);
    fusedTruth_[i.users().front()] = phi;
  }
}

// Unary minus: an integer other than INT64_MIN, and a double (keeping its
// decimals), are negated in place, as xb_op_neg does; anything else calls it.
void CodeGen::emitNeg(const xir::Instruction &i) {
  Value *a = addr(i.operand(0));
  Value *d = dest(i);
  Value *ta = typeOf(i.operand(0), a);
  BasicBlock *intBB = newBlock("int"), *intDone = newBlock("int.done"), *notInt = newBlock("not.int");
  BasicBlock *dblBB = newBlock("dbl"), *slow = newBlock("op"), *join = newBlock("join");
  b_.CreateCondBr(b_.CreateICmpEQ(ta, ConstantInt::get(i32_, XB_T_INT)), intBB, notInt);
  b_.SetInsertPoint(intBB);
  Value *r = b_.CreateBinaryIntrinsic(Intrinsic::ssub_with_overflow, ConstantInt::get(i64_, 0), intValue(a));
  MDBuilder md(ctx_);
  b_.CreateCondBr(b_.CreateExtractValue(r, 1), slow, intDone, md.createUnlikelyBranchWeights()); // INT64_MIN
  b_.SetInsertPoint(intDone);
  storeInt(d, b_.CreateExtractValue(r, 0));
  b_.CreateBr(join);
  b_.SetInsertPoint(notInt);
  branchLikely(b_.CreateICmpEQ(ta, ConstantInt::get(i32_, XB_T_DOUBLE)), dblBB, slow);
  b_.SetInsertPoint(dblBB);
  Value *dec = numberDec(a, ta); // read before `d`, which may be `a`, is written
  storeDouble(d, b_.CreateFNeg(numberAsDouble(a, ta)), dec);
  b_.CreateBr(join);
  b_.SetInsertPoint(slow);
  callItemOp(rtNeg_, d, {a});
  checkPending(unwind_);
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
}

// A number item of type `type` as an integer, as the runtime takes it
// (xb_dbl_to_i64): llvm.fptosi.sat truncates, saturates and gives 0 for NaN.
Value *CodeGen::numberAsInteger(Value *item, Value *type) {
  Value *bits = intValue(item);
  Value *fromDouble =
      b_.CreateIntrinsic(Intrinsic::fptosi_sat, {i64_, b_.getDoubleTy()}, {b_.CreateBitCast(bits, b_.getDoubleTy())});
  return b_.CreateSelect(b_.CreateICmpEQ(type, ConstantInt::get(i32_, XB_T_INT)), bits, fromDouble);
}

// xb_bit_apply's operation on machine integers (`y` unused by XB_BIT_NOT).
Value *CodeGen::bitApply(xb_bit_op op, Value *x, Value *y) {
  auto shift = [&](Instruction::BinaryOps kind, Value *outOfRange) { // a count outside 0..63 shifts every bit out
    Value *inRange = b_.CreateICmpULT(y, ConstantInt::get(i64_, 64));
    Value *count = b_.CreateAnd(y, ConstantInt::get(i64_, 63));
    return b_.CreateSelect(inRange, b_.CreateBinOp(kind, x, count), outOfRange);
  };
  switch (op) {
  case XB_BIT_AND: return b_.CreateAnd(x, y);
  case XB_BIT_OR: return b_.CreateOr(x, y);
  case XB_BIT_XOR: return b_.CreateXor(x, y);
  case XB_BIT_NOR: return b_.CreateNot(b_.CreateOr(x, y));
  case XB_BIT_NOT: return b_.CreateNot(x);
  case XB_BIT_SHL: return shift(Instruction::Shl, ConstantInt::get(i64_, 0));
  case XB_BIT_SHR: return shift(Instruction::LShr, ConstantInt::get(i64_, 0));
  case XB_BIT_SAR: return shift(Instruction::AShr, b_.CreateAShr(x, 63));
  }
  return nullptr;
}

// Bit operation `op` on the items `x` and `y` (null for one operand), of
// types `tx` and `ty`, into `d`: numbers are the machine operation, in place;
// for anything else `slow` emits the runtime call, which raises the error.
void CodeGen::emitBitOp(xb_bit_op op, Value *x, Value *tx, Value *y, Value *ty, Value *d,
                        function_ref<void()> slow) {
  Value *ints = y ? bothInts(tx, ty) : isIntType(tx);
  Value *numbers = y ? b_.CreateAnd(isNumberType(tx), isNumberType(ty)) : isNumberType(tx);
  BasicBlock *intBB = newBlock("bits.int"), *notInt = newBlock("bits.not_int"), *numBB = newBlock("bits.num");
  BasicBlock *other = newBlock("bits.other"), *join = newBlock("join");
  // Integers, the usual operands, take two instructions; a double is converted on a path of its own.
  branchLikely(ints, intBB, notInt);
  b_.SetInsertPoint(intBB);
  Value *ix = intValue(x); // both operands are read before `d`, which may be one of them, is written
  storeInt(d, bitApply(op, ix, y ? intValue(y) : nullptr));
  b_.CreateBr(join);
  b_.SetInsertPoint(notInt);
  branchLikely(numbers, numBB, other);
  b_.SetInsertPoint(numBB);
  Value *nx = numberAsInteger(x, tx);
  Value *ny = y ? numberAsInteger(y, ty) : nullptr;
  storeInt(d, bitApply(op, nx, ny));
  b_.CreateBr(join);
  b_.SetInsertPoint(other);
  slow();
  checkPending(unwind_);
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
}

// A bit operator (& | ^^ << >> ~); anything but numbers calls the operator.
void CodeGen::emitBitOperator(const xir::Instruction &i, const xb_bit_function &f) {
  bool binary = f.arity > 1;
  Value *a = addr(i.operand(0));
  Value *bv = binary ? addr(i.operand(1)) : nullptr;
  Value *d = dest(i);
  emitBitOp(f.op, a, typeOf(i.operand(0), a), bv, binary ? typeOf(i.operand(1), bv) : nullptr, d, [&] {
    if (binary) callItemOp(rtBin_.at(i.binop), d, {a, bv});
    else callItemOp(rtBitNot_, d, {a});
  });
}

// A call to one of xbase's bit functions (xb_bit_function in xbrt.h): number
// arguments are the machine operation; others call the runtime function.
// Returns false for any other call.
bool CodeGen::emitBitCall(const xir::Instruction &i) {
  if (i.callee.function || !i.callee.native || i.hasRest()) return false;
  const xb_bit_function *f = findBitFunction(i.callee.name);
  if (!f || i.operands().size() != f->arity) return false;
  Value *d = dest(i);
  bool binary = f->arity > 1;
  Value *x = addr(i.operand(0)); // read in place: the argument group is built only for the call
  Value *y = binary ? addr(i.operand(1)) : nullptr;
  emitBitOp(f->op, x, typeOf(i.operand(0), x), y, binary ? typeOf(i.operand(1), y) : nullptr, d, [&] {
    Value *argv = operandGroup(i);
    setNil(d);
    b_.CreateCall(xbFuncTy_, externFunction(i.callee), {fnCtx_, d, ConstantInt::get(i32_, f->arity), argv});
  });
  return true;
}

// `/` and `%` of two numbers and a divisor other than 0 (-0 included): a
// double with SET DECIMALS decimals, as xb_op_div and xb_op_mod make it; `%`
// of two integers is their remainder (0 for a divisor of -1). Anything else
// calls the operator, which raises the errors.
// fmod( x, y ) (y not 0, else anything): for two integers below 2^63 in
// magnitude (doubles that hold integers convert to int64 exactly), their
// integer remainder with x's sign, which is fmod's exactly (fmod's result is a
// double exactly, so converting it back loses nothing), without calling it;
// fmod otherwise.
Value *CodeGen::doubleRemainder(Value *x, Value *y) {
  Type *f64 = b_.getDoubleTy();
  auto integral = [&](Value *v) {
    Value *whole = b_.CreateFCmpOEQ(v, b_.CreateUnaryIntrinsic(Intrinsic::trunc, v));
    return b_.CreateAnd(whole, b_.CreateFCmpOLT(b_.CreateUnaryIntrinsic(Intrinsic::fabs, v),
                                                ConstantFP::get(f64, 9223372036854775808.0)));
  };
  BasicBlock *ints = newBlock("fmod.int"), *call = newBlock("fmod"), *done = newBlock("fmod.done");
  branchLikely(b_.CreateAnd(integral(x), integral(y)), ints, call);
  b_.SetInsertPoint(ints);
  Value *yi = b_.CreateFPToSI(y, i64_);
  Value *safe = b_.CreateSelect(b_.CreateICmpEQ(yi, ConstantInt::get(i64_, 0)), ConstantInt::get(i64_, 1), yi);
  Value *ir = b_.CreateSIToFP(b_.CreateSRem(b_.CreateFPToSI(x, i64_), safe), f64);
  Value *exact = b_.CreateBinaryIntrinsic(Intrinsic::copysign, ir, x); // fmod's zero has x's sign too
  BasicBlock *intsEnd = b_.GetInsertBlock();
  b_.CreateBr(done);
  b_.SetInsertPoint(call);
  Value *f = b_.CreateFRem(x, y);
  BasicBlock *callEnd = b_.GetInsertBlock();
  b_.CreateBr(done);
  b_.SetInsertPoint(done);
  PHINode *phi = b_.CreatePHI(f64, 2, "fmod");
  phi->addIncoming(exact, intsEnd);
  phi->addIncoming(f, callEnd);
  return phi;
}

// SET DECIMALS as an item's decimals (16 bits), read from the context's head.
Value *CodeGen::setDecimals() {
  return b_.CreateTrunc(loadFrom(i32_, b_.CreateStructGEP(ctxHeadTy_, fnCtx_, CtxDecimals), Mem::Ctx), i16_);
}

void CodeGen::emitDivMod(const xir::Instruction &i, Value *a, Value *bv, Value *d) {
  BasicBlock *dblBB = newBlock("double"), *other = newBlock("other"), *numBB = newBlock("num");
  BasicBlock *slow = newBlock("op"), *join = newBlock("join");
  Value *ta = typeOf(i.operand(0), a), *tb = typeOf(i.operand(1), bv);
  Value *isDouble = ConstantInt::get(i32_, XB_T_DOUBLE);
  b_.CreateCondBr(b_.CreateAnd(b_.CreateICmpEQ(ta, isDouble), b_.CreateICmpEQ(tb, isDouble)), dblBB, other);
  b_.SetInsertPoint(other);
  branchLikely(exactNumbers(a, ta, bv, tb), numBB, slow);
  // The quotient or remainder `r` of a non-zero divisor `fy`, with SET DECIMALS decimals.
  auto finish = [&](Value *r, Value *fy) {
    BasicBlock *done = newBlock("num.done");
    Value *dec = setDecimals();
    branchLikely(b_.CreateFCmpUNE(fy, ConstantFP::get(b_.getDoubleTy(), 0.0)), done, slow);
    b_.SetInsertPoint(done);
    storeDouble(d, r, dec);
    b_.CreateBr(join);
  };
  b_.SetInsertPoint(dblBB); // two doubles: their bits as they are
  Value *gx = b_.CreateBitCast(intValue(a), b_.getDoubleTy()), *gy = b_.CreateBitCast(intValue(bv), b_.getDoubleTy());
  finish(i.binop == BinOp::Div ? b_.CreateFDiv(gx, gy) : doubleRemainder(gx, gy), gy);
  b_.SetInsertPoint(numBB);
  Value *fx = numberAsDouble(a, ta), *fy = numberAsDouble(bv, tb);
  Value *r;
  if (i.binop == BinOp::Div) {
    r = b_.CreateFDiv(fx, fy);
  } else { // two integers' remainder (0 for a divisor of -1), else the doubles'
    BasicBlock *ints = newBlock("rem.int"), *dbls = newBlock("rem.double"), *rem = newBlock("rem");
    b_.CreateCondBr(bothInts(ta, tb), ints, dbls);
    b_.SetInsertPoint(ints);
    Value *xi = intValue(a), *yi = intValue(bv);
    Value *minus1 = b_.CreateICmpEQ(yi, ConstantInt::getSigned(i64_, -1));
    Value *safe = b_.CreateSelect(b_.CreateOr(minus1, b_.CreateICmpEQ(yi, ConstantInt::get(i64_, 0))),
                                  ConstantInt::get(i64_, 1), yi); // srem must not trap where it is not used
    Value *ir = b_.CreateSelect(minus1, ConstantFP::get(b_.getDoubleTy(), 0.0),
                                b_.CreateSIToFP(b_.CreateSRem(xi, safe), b_.getDoubleTy()));
    BasicBlock *intsEnd = b_.GetInsertBlock();
    b_.CreateBr(rem);
    b_.SetInsertPoint(dbls);
    Value *dr = doubleRemainder(fx, fy);
    BasicBlock *dblsEnd = b_.GetInsertBlock();
    b_.CreateBr(rem);
    b_.SetInsertPoint(rem);
    PHINode *phi = b_.CreatePHI(b_.getDoubleTy(), 2, "rem");
    phi->addIncoming(ir, intsEnd);
    phi->addIncoming(dr, dblsEnd);
    r = phi;
  }
  finish(r, fy);
  b_.SetInsertPoint(slow);
  callItemOp(rtBin_.at(i.binop), d, {a, bv});
  checkPending(unwind_);
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
  checkPending(unwind_);
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
  // An integer index is the usual one: a loop indexing with doubles runs in the
  // generic copy of splitColdLoopPaths, which keeps the double path inline.
  branchLikely(b_.CreateAnd(isArray, b_.CreateICmpEQ(idxType, ConstantInt::get(i32_, XB_T_INT))), intIdx, notInt);
  b_.SetInsertPoint(notInt);
  branchLikely(b_.CreateAnd(isArray, b_.CreateICmpEQ(idxType, ConstantInt::get(i32_, XB_T_DOUBLE))), dblIdx, slow);
  auto arrayAndLen = [&](Value *&arr) {
    arr = itemPointer(base, "arr");
    return arrayLength(arr);
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
  return arrayElement(array, pos);
}

void CodeGen::emitIndex(const xir::Instruction &i) {
  Value *base = addr(i.operand(0)), *idx = addr(i.operand(1)), *d = dest(i);
  BasicBlock *slow = newBlock("index.call"), *join = newBlock("index.end");
  Value *array, *pos;
  copyItem(d, inlineElement(i, base, idx, slow, array, pos)); // base and idx are read before d is written
  b_.CreateBr(join);
  b_.SetInsertPoint(slow);
  callItemOp(rtIndex_, d, {base, idx});
  checkPending(unwind_);
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
  checkPending(unwind_);
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
}

} // namespace xb
