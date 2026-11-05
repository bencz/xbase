// Expressions: operands, operators, conditions, assignments, calls, array literals.
#include "codegen/CodeGen.h"

#include "opt/Fold.h"
#include "xbrt.h" // constants only (type tags)

#include <llvm/IR/Constants.h>
#include <llvm/IR/MDBuilder.h>

using namespace llvm;

namespace xb {

// Whether evaluating `e` can change variables or call code.
bool CodeGen::hasSideEffects(const Expr &e) {
  switch (e.kind) {
  case ExprKind::Call: case ExprKind::Assign: case ExprKind::IncDec:
    return true;
  case ExprKind::Unary:
    return hasSideEffects(*static_cast<const UnaryExpr &>(e).operand);
  case ExprKind::Binary: {
    auto &b = static_cast<const BinaryExpr &>(e);
    return hasSideEffects(*b.lhs) || hasSideEffects(*b.rhs);
  }
  case ExprKind::And: case ExprKind::Or: {
    auto &b = static_cast<const LogicExpr &>(e);
    return hasSideEffects(*b.lhs) || hasSideEffects(*b.rhs);
  }
  case ExprKind::IIf: {
    auto &i = static_cast<const IIfExpr &>(e);
    return hasSideEffects(*i.cond) || hasSideEffects(*i.then) || hasSideEffects(*i.otherwise);
  }
  case ExprKind::Index: {
    auto &ix = static_cast<const IndexExpr &>(e);
    return hasSideEffects(*ix.base) || hasSideEffects(*ix.index);
  }
  case ExprKind::ArrayLit:
    for (auto &el : static_cast<const ArrayLitExpr &>(e).elems)
      if (hasSideEffects(*el)) return true;
    return false;
  default: // a codeblock only captures; its body runs when it is evaluated
    return false;
  }
}

// Returns a pointer to an item holding the value of `e`, without copying
// when the value already lives somewhere (a variable or a literal).
Value *CodeGen::genOperand(const Expr &e) {
  switch (e.kind) {
  case ExprKind::Nil: return nilLiteral();
  case ExprKind::Logical: return logicalLiteral(static_cast<const LogicalExpr &>(e).value);
  case ExprKind::Number: {
    auto &n = static_cast<const NumberExpr &>(e);
    if (n.isInt || n.dec != kDefaultDecimals) return numberLiteral(n);
    Value *t = tempSlot(); // decimals known only at run time (SET DECIMALS)
    b_.CreateCall(rtNumDefaultDec_, {fnCtx_, t, ConstantFP::get(Type::getDoubleTy(ctx_), n.dval)});
    return t;
  }
  case ExprKind::String: return stringLiteral(static_cast<const StringExpr &>(e).value);
  case ExprKind::Var: return varAddress(e);
  default: {
    Value *t = tempSlot();
    genExpr(e, t);
    return t;
  }
  }
}

// Left-to-right evaluation: if evaluating the right side could change a
// variable used on the left (`x + (x := 5)`), snapshot the left first.
std::pair<Value *, Value *> CodeGen::genOperands(const Expr &lhs, const Expr &rhs) {
  Value *a = genOperand(lhs);
  if (lhs.kind == ExprKind::Var && hasSideEffects(rhs)) {
    Value *t = tempSlot();
    copyItem(t, a);
    a = t;
  }
  Value *b = genOperand(rhs);
  return {a, b};
}

// Runtime operators accept a result pointer that aliases an operand.
void CodeGen::genBinary(BinOp op, Value *dst, Value *a, Value *b) {
  b_.CreateCall(rtBin_[op], {fnCtx_, dst, a, b});
  checkPending();
}

Value *CodeGen::testTruth(Value *item) {
  Value *type = b_.CreateLoad(i32_, b_.CreateStructGEP(itemTy_, item, FType), "type");
  BasicBlock *ok = newBlock("cond.ok");
  BasicBlock *bad = newBlock("cond.bad");
  MDBuilder md(ctx_);
  b_.CreateCondBr(b_.CreateICmpEQ(type, ConstantInt::get(i32_, XB_T_LOGICAL)), ok, bad, md.createLikelyBranchWeights());
  b_.SetInsertPoint(bad);
  b_.CreateCall(rtCondError_, {fnCtx_, item});
  b_.CreateBr(fnExit_);
  b_.SetInsertPoint(ok);
  Value *v = b_.CreateLoad(i32_, b_.CreateStructGEP(itemTy_, item, FValue), "lval");
  return b_.CreateICmpNE(v, ConstantInt::get(i32_, 0));
}

Value *CodeGen::genCond(const Expr &e) {
  switch (e.kind) {
  case ExprKind::Logical:
    return b_.getInt1(static_cast<const LogicalExpr &>(e).value);
  case ExprKind::And:
  case ExprKind::Or: {
    auto &l = static_cast<const LogicExpr &>(e);
    bool isAnd = e.kind == ExprKind::And;
    Value *lv = genCond(*l.lhs);
    BasicBlock *lhsEnd = b_.GetInsertBlock();
    BasicBlock *rhsBB = newBlock(isAnd ? "and.rhs" : "or.rhs");
    BasicBlock *join = newBlock(isAnd ? "and.end" : "or.end");
    if (isAnd) b_.CreateCondBr(lv, rhsBB, join);
    else b_.CreateCondBr(lv, join, rhsBB);
    b_.SetInsertPoint(rhsBB);
    Value *rv = genCond(*l.rhs);
    BasicBlock *rhsEnd = b_.GetInsertBlock();
    b_.CreateBr(join);
    b_.SetInsertPoint(join);
    PHINode *phi = b_.CreatePHI(b_.getInt1Ty(), 2);
    phi->addIncoming(b_.getInt1(!isAnd), lhsEnd);
    phi->addIncoming(rv, rhsEnd);
    return phi;
  }
  case ExprKind::Unary:
    if (static_cast<const UnaryExpr &>(e).op == UnOp::Not)
      return b_.CreateNot(genCond(*static_cast<const UnaryExpr &>(e).operand));
    [[fallthrough]];
  default:
    return testTruth(genOperand(e));
  }
}

void CodeGen::genDiscard(const Expr &e) {
  switch (e.kind) {
  case ExprKind::Assign:
    genAssign(static_cast<const AssignExpr &>(e), nullptr);
    break;
  case ExprKind::IncDec:
    genIncDec(static_cast<const IncDecExpr &>(e), nullptr);
    break;
  default:
    genExpr(e, tempSlot());
    break;
  }
}

void CodeGen::genExpr(const Expr &e, Value *dst) {
  switch (e.kind) {
  case ExprKind::Nil:
    setNil(dst);
    break;
  case ExprKind::Logical:
  case ExprKind::Number:
  case ExprKind::String:
  case ExprKind::Var:
    copyItem(dst, genOperand(e));
    break;
  case ExprKind::Call:
    genCall(static_cast<const CallExpr &>(e), dst);
    break;
  case ExprKind::Unary: {
    auto &u = static_cast<const UnaryExpr &>(e);
    Value *a = genOperand(*u.operand);
    switch (u.op) {
    case UnOp::Neg:
      b_.CreateCall(rtNeg_, {fnCtx_, dst, a});
      checkPending();
      break;
    case UnOp::Not:
      b_.CreateCall(rtNot_, {fnCtx_, dst, a});
      checkPending();
      break;
    case UnOp::Plus:
      copyItem(dst, a);
      break;
    }
    break;
  }
  case ExprKind::Binary: {
    auto &bin = static_cast<const BinaryExpr &>(e);
    auto [a, b] = genOperands(*bin.lhs, *bin.rhs);
    genBinary(bin.op, dst, a, b);
    break;
  }
  case ExprKind::And:
  case ExprKind::Or:
    storeLogical(dst, genCond(e));
    break;
  case ExprKind::Assign:
    genAssign(static_cast<const AssignExpr &>(e), dst);
    break;
  case ExprKind::IncDec:
    genIncDec(static_cast<const IncDecExpr &>(e), dst);
    break;
  case ExprKind::IIf:
    genIIf(static_cast<const IIfExpr &>(e), dst);
    break;
  case ExprKind::ArrayLit:
    genArrayLit(static_cast<const ArrayLitExpr &>(e), dst);
    break;
  case ExprKind::Index: {
    auto &ix = static_cast<const IndexExpr &>(e);
    auto [base, index] = genOperands(*ix.base, *ix.index);
    b_.CreateCall(rtIndex_, {fnCtx_, dst, base, index});
    checkPending();
    break;
  }
  case ExprKind::Block:
    genCodeblock(static_cast<const BlockExpr &>(e), dst);
    break;
  }
}

// Elements are evaluated into consecutive slots, then copied into the array.
void CodeGen::genArrayLit(const ArrayLitExpr &a, Value *dst) {
  uint32_t n = (uint32_t)a.elems.size();
  uint32_t first = n ? allocTemps(n) : 0;
  for (uint32_t i = 0; i < n; ++i) genExpr(*a.elems[i], slot(first + i));
  Value *elems = n ? slot(first) : ConstantPointerNull::get(ptrTy_);
  b_.CreateCall(rtArrayNew_, {fnCtx_, dst, ConstantInt::get(i32_, n), elems});
}

// Element assignment, in Clipper's evaluation order:
//   a[i] := v          v, then a, then i, then the store;
//   a[i] op= v, a[i]++ a, then i, then the element, then v.
// A variable operand is copied when something evaluated after it has side
// effects, so each operand keeps the value it had when it was evaluated.
void CodeGen::genIndexAssign(const IndexExpr &target, const AssignExpr *assign, const IncDecExpr *incdec, Value *dst) {
  auto snapshot = [&](const Expr &e, bool laterEffects) {
    Value *v = genOperand(e);
    if (laterEffects && e.kind == ExprKind::Var) {
      Value *t = tempSlot();
      copyItem(t, v);
      v = t;
    }
    return v;
  };
  bool indexEffects = hasSideEffects(*target.index);
  if (assign && !assign->compound) {
    Value *value = snapshot(*assign->value, hasSideEffects(*target.base) || indexEffects);
    Value *base = snapshot(*target.base, indexEffects);
    Value *index = genOperand(*target.index);
    b_.CreateCall(rtIndexSet_, {fnCtx_, base, index, value});
    checkPending();
    if (dst) copyItem(dst, value);
    return;
  }
  bool valueEffects = assign && hasSideEffects(*assign->value);
  Value *base = snapshot(*target.base, indexEffects || valueEffects);
  Value *index = snapshot(*target.index, valueEffects);
  Value *value = tempSlot();
  b_.CreateCall(rtIndex_, {fnCtx_, value, base, index});
  checkPending();
  if (incdec && dst && !incdec->prefix) copyItem(dst, value);
  Value *rhs = assign ? genOperand(*assign->value) : intLiteral(1);
  BinOp op = assign ? assign->op : incdec->increment ? BinOp::Add : BinOp::Sub;
  genBinary(op, value, value, rhs);
  b_.CreateCall(rtIndexSet_, {fnCtx_, base, index, value});
  checkPending();
  if (dst && !(incdec && !incdec->prefix)) copyItem(dst, value);
}

void CodeGen::genCall(const CallExpr &c, Value *dst) {
  // Arguments go into consecutive slots of the caller's frame; the callee
  // receives (argc, argv) pointing at them.
  uint32_t n = (uint32_t)c.args.size();
  uint32_t first = n ? allocTemps(n) : 0;
  for (uint32_t i = 0; i < n; ++i) genExpr(*c.args[i], slot(first + i));
  Value *argv = n ? slot(first) : ConstantPointerNull::get(ptrTy_);

  // Resolution (sema/Calls) linked calls to EXTERN FUNCTIONs; the others go to this module's functions.
  Function *callee = c.callee ? declareXbFunc(c.name, false, c.callee->native) : funcs_.at(c.name);
  setNil(dst); // the caller clears the result, so callees that return nothing need not
  b_.CreateCall(xbFuncTy_, callee, {fnCtx_, dst, ConstantInt::get(i32_, n), argv});
  checkPending();
}

void CodeGen::genAssign(const AssignExpr &a, Value *dst) {
  if (a.target->kind == ExprKind::Index) {
    genIndexAssign(static_cast<const IndexExpr &>(*a.target), &a, nullptr, dst);
    return;
  }
  // The target's address is taken after anything that can run code: a FOR
  // EACH variable designates an array element, which moves when the array
  // grows. A value without side effects is computed in place.
  Value *target;
  if (a.compound) {
    auto [l, r] = genOperands(*a.target, *a.value);
    target = varAddress(*a.target);
    genBinary(a.op, target, l, r);
  } else if (hasSideEffects(*a.value)) {
    // The value may change the target while it is computed (`x := x++`): the
    // assignment stores the value as computed, after those side effects.
    Value *t = tempSlot();
    genExpr(*a.value, t);
    target = varAddress(*a.target);
    copyItem(target, t);
  } else {
    target = varAddress(*a.target);
    genExpr(*a.value, target);
  }
  writeBarrier(*a.target);
  if (dst) copyItem(dst, target);
}

void CodeGen::genIncDec(const IncDecExpr &e, Value *dst) {
  if (e.target->kind == ExprKind::Index) {
    genIndexAssign(static_cast<const IndexExpr &>(*e.target), nullptr, &e, dst);
    return;
  }
  Value *target = varAddress(*e.target);
  if (dst && !e.prefix) copyItem(dst, target);
  genBinary(e.increment ? BinOp::Add : BinOp::Sub, target, target, intLiteral(1));
  writeBarrier(*e.target);
  if (dst && e.prefix) copyItem(dst, target);
}

void CodeGen::genIIf(const IIfExpr &e, Value *dst) {
  Value *c = genCond(*e.cond);
  BasicBlock *thenBB = newBlock("iif.then");
  BasicBlock *elseBB = newBlock("iif.else");
  BasicBlock *end = newBlock("iif.end");
  b_.CreateCondBr(c, thenBB, elseBB);
  b_.SetInsertPoint(thenBB);
  genExpr(*e.then, dst);
  b_.CreateBr(end);
  b_.SetInsertPoint(elseBB);
  genExpr(*e.otherwise, dst);
  b_.CreateBr(end);
  b_.SetInsertPoint(end);
}

} // namespace xb
