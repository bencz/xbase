// Inline fast paths of operators and of the bit functions: the integer cases
// the runtime takes first (xb_op_add, the comparisons, xb_for_test in
// core/ops.c, xb_bit_apply), done in place; every other case, and an integer
// overflow, calls the runtime.
#include "codegen/CodeGen.h"

#include "common/BitFunctions.h"

#include "xbrt.h" // constants only (type tags)

#include <llvm/IR/Constants.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/MDBuilder.h>

using namespace llvm;

namespace xb {

Value *CodeGen::isInt(Value *item) {
  Value *type = b_.CreateLoad(i32_, b_.CreateStructGEP(itemTy_, item, FType), "type");
  return b_.CreateICmpEQ(type, ConstantInt::get(i32_, XB_T_INT));
}

Value *CodeGen::intValue(Value *item) { return b_.CreateLoad(i64_, b_.CreateStructGEP(itemTy_, item, FValue), "int"); }

// A computed integer: no display width or decimals of its own (they come
// from the value when it is shown), as xb_set_int makes it.
void CodeGen::storeInt(Value *dst, Value *v) { storeTagged(dst, XB_T_INT, v); }

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

void CodeGen::emitBinary(const xir::Instruction &i) {
  Value *a = addr(i.operand(0));
  Value *bv = addr(i.operand(1));
  Value *d = dest(i);
  xir::IntPred pred;
  Intrinsic::ID arith = overflowIntrinsic(i.binop);
  bool compare = intPredicate(i.binop, pred);
  if (arith == Intrinsic::not_intrinsic && !compare) {
    b_.CreateCall(rtBin_.at(i.binop), {fnCtx_, d, a, bv});
    checkPending(fnExit_);
    return;
  }
  BasicBlock *fast = newBlock("int"), *slow = newBlock("op"), *join = newBlock("join");
  branchLikely(b_.CreateAnd(isInt(a), isInt(bv)), fast, slow);
  b_.SetInsertPoint(fast);
  Value *x = intValue(a), *y = intValue(bv); // read before `d`, which may be an operand, is written
  Value *fastBool = nullptr;
  if (compare) {
    fastBool = b_.CreateICmp(llvmPredicate(pred), x, y);
    if (!fusedComparisons_.count(&i)) storeLogical(d, fastBool);
  } else {
    Value *r = b_.CreateBinaryIntrinsic(arith, x, y);
    BasicBlock *done = newBlock("int.done");
    MDBuilder md(ctx_);
    b_.CreateCondBr(b_.CreateExtractValue(r, 1), slow, done, md.createUnlikelyBranchWeights()); // overflow: a double
    b_.SetInsertPoint(done);
    storeInt(d, b_.CreateExtractValue(r, 0));
  }
  BasicBlock *fastEnd = b_.GetInsertBlock();
  b_.CreateBr(join);
  b_.SetInsertPoint(slow);
  b_.CreateCall(rtBin_.at(i.binop), {fnCtx_, d, a, bv});
  checkPending(fnExit_);
  Value *slowBool = nullptr;
  if (compare && fusedComparisons_.count(&i)) slowBool = logicalValue(d); // the operator made a logical
  BasicBlock *slowEnd = b_.GetInsertBlock();
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
  if (slowBool) {
    PHINode *phi = b_.CreatePHI(b_.getInt1Ty(), 2, "cmp");
    phi->addIncoming(fastBool, fastEnd);
    phi->addIncoming(slowBool, slowEnd);
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
  Value *go = b_.CreateCall(rtForTest_, {fnCtx_, counter, limit, step});
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

} // namespace xb
