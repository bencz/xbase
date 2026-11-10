// AST to xIR: expressions, in Clipper's evaluation order.
#include "xir/Gen.h"

#include "opt/Fold.h"

namespace xb::xir {

Value *Generator::genExpr(const Expr &e) {
  Builder &b = *cur_.b;
  switch (e.kind) {
  case ExprKind::Nil: return b.nil();
  case ExprKind::Logical: return b.logical(static_cast<const LogicalExpr &>(e).value);
  case ExprKind::Number: {
    auto &n = static_cast<const NumberExpr &>(e);
    if (!n.isInt && n.dec == kDefaultDecimals) return b.numDefaultDec(n.dval); // decimals known at run time
    return b.number(n);
  }
  case ExprKind::String: return b.string(static_cast<const StringExpr &>(e).value);
  case ExprKind::Var: return load(static_cast<const VarExpr &>(e));
  case ExprKind::Call: return genCall(static_cast<const CallExpr &>(e));
  case ExprKind::Unary: {
    auto &u = static_cast<const UnaryExpr &>(e);
    Value *a = genExpr(*u.operand);
    return u.op == UnOp::Plus ? a : b.unary(u.op, a);
  }
  case ExprKind::Binary: {
    auto &bin = static_cast<const BinaryExpr &>(e);
    Value *l = genExpr(*bin.lhs);
    Value *r = genExpr(*bin.rhs);
    return b.binary(bin.op, l, r);
  }
  case ExprKind::And:
  case ExprKind::Or: return genLogic(static_cast<const LogicExpr &>(e));
  case ExprKind::Assign: return genAssign(static_cast<const AssignExpr &>(e));
  case ExprKind::IncDec: return genIncDec(static_cast<const IncDecExpr &>(e));
  case ExprKind::IIf: return genIIf(static_cast<const IIfExpr &>(e));
  case ExprKind::List: {
    auto &l = static_cast<const ListExpr &>(e);
    for (size_t k = 0; k + 1 < l.exprs.size(); ++k) genDiscard(*l.exprs[k]);
    return genExpr(*l.exprs.back());
  }
  case ExprKind::ArrayLit: {
    return b.arrayLit(genArgs(static_cast<const ArrayLitExpr &>(e).elems));
  }
  case ExprKind::HashLit: {
    auto &h = static_cast<const HashLitExpr &>(e);
    std::vector<Value *> pairs;
    for (size_t k = 0; k < h.keys.size(); ++k) {
      pairs.push_back(genExpr(*h.keys[k]));
      pairs.push_back(genExpr(*h.values[k]));
    }
    return b.hashLit(pairs);
  }
  case ExprKind::Index: {
    auto &ix = static_cast<const IndexExpr &>(e);
    Value *base = genExpr(*ix.base);
    Value *idx = genExpr(*ix.index);
    return b.index(base, idx);
  }
  case ExprKind::Block: return genCodeblock(static_cast<const BlockExpr &>(e));
  case ExprKind::Send: {
    auto &m = static_cast<const SendExpr &>(e);
    Value *object = genExpr(*m.object);
    std::vector<Value *> args = genArgs(m.args);
    b.setLoc(m.loc);
    return b.send(m.message, object, args); // a method receives values (restRefs 0)
  }
  case ExprKind::Ref: { // a call argument passed by reference
    const Expr &target = *static_cast<const RefExpr &>(e).target;
    if (target.kind == ExprKind::Var) return b.refVar(varOf(static_cast<const VarExpr &>(target)));
    auto &ix = static_cast<const IndexExpr &>(target);
    Value *base = genExpr(*ix.base);
    Value *idx = genExpr(*ix.index);
    return b.refIndex(base, idx);
  }
  case ExprKind::Spread: break; // only an element of a list (genArgs)
  }
  return b.nil();
}

void Generator::genDiscard(const Expr &e) { genExpr(e); }

// A condition: .AND. and .OR. evaluate their right side only when needed.
Value *Generator::genCond(const Expr &e) {
  Builder &b = *cur_.b;
  switch (e.kind) {
  case ExprKind::Logical: return b.boolean(static_cast<const LogicalExpr &>(e).value);
  case ExprKind::And:
  case ExprKind::Or: {
    auto &l = static_cast<const LogicExpr &>(e);
    bool isAnd = e.kind == ExprKind::And;
    Value *lv = genCond(*l.lhs);
    BasicBlock *rhs = cur_.fn->addBlock(isAnd ? "and.rhs" : "or.rhs");
    BasicBlock *join = cur_.fn->addBlock(isAnd ? "and.end" : "or.end");
    Value *result = join->addParam(Type::Bool);
    if (isAnd) b.condBr(lv, rhs, join, {}, {b.boolean(false)});
    else b.condBr(lv, join, rhs, {b.boolean(true)}, {});
    b.setBlock(rhs);
    b.br(join, {genCond(*l.rhs)});
    b.setBlock(join);
    return result;
  }
  case ExprKind::List: {
    auto &l = static_cast<const ListExpr &>(e);
    for (size_t k = 0; k + 1 < l.exprs.size(); ++k) genDiscard(*l.exprs[k]);
    return genCond(*l.exprs.back());
  }
  case ExprKind::Unary:
    if (static_cast<const UnaryExpr &>(e).op == UnOp::Not)
      return b.boolNot(genCond(*static_cast<const UnaryExpr &>(e).operand));
    break;
  default:
    break;
  }
  return b.truth(genExpr(e));
}

// The values of a list of arguments or elements, in order. `...` is an
// args.rest made after the others, right before the list's operation, which is
// then its one use in the same block.
std::vector<Value *> Generator::genArgs(const std::vector<ExprPtr> &exprs) {
  std::vector<Value *> values;
  for (auto &e : exprs) values.push_back(e->kind == ExprKind::Spread ? nullptr : genExpr(*e));
  for (Value *&v : values)
    if (!v) v = cur_.b->argsRest();
  return values;
}

Value *Generator::genCall(const CallExpr &c) {
  std::vector<Value *> args = genArgs(c.args);
  Callee callee;
  if (c.callee || c.external) { // an EXTERN FUNCTION, or another module's function (sema/Calls)
    callee.name = c.name;
    callee.native = c.callee && c.callee->native;
  } else {
    callee.function = funcs_.at(c.name);
  }
  cur_.b->setLoc(c.loc);
  Instruction *call = cur_.b->call(std::move(callee), args);
  // A native function receives references only where it takes them; an xBase one always.
  if (call->hasRest()) call->restRefs = c.callee ? c.callee->refMask() : UINT32_MAX;
  return call;
}

// x := v evaluates v, then stores; x op= v reads x, then evaluates v. The
// value of the expression is the value stored.
Value *Generator::genAssign(const AssignExpr &a) {
  if (a.target->kind == ExprKind::Index) return genIndexAssign(static_cast<const IndexExpr &>(*a.target), &a, nullptr);
  if (a.target->kind == ExprKind::Send) return genSendAssign(static_cast<const SendExpr &>(*a.target), &a, nullptr);
  auto &target = static_cast<const VarExpr &>(*a.target);
  Value *value;
  if (a.compound) {
    Value *l = load(target);
    Value *r = genExpr(*a.value);
    value = cur_.b->binary(a.op, l, r);
  } else {
    value = genExpr(*a.value);
  }
  store(target, value);
  return value;
}

Value *Generator::genIncDec(const IncDecExpr &e) {
  if (e.target->kind == ExprKind::Index) return genIndexAssign(static_cast<const IndexExpr &>(*e.target), nullptr, &e);
  if (e.target->kind == ExprKind::Send) return genSendAssign(static_cast<const SendExpr &>(*e.target), nullptr, &e);
  auto &target = static_cast<const VarExpr &>(*e.target);
  Value *old = load(target);
  Value *now = cur_.b->binary(e.increment ? BinOp::Add : BinOp::Sub, old, cur_.b->integer(1, 0));
  store(target, now);
  return e.prefix ? now : old;
}

// Element assignment, in Clipper's evaluation order, each operand once:
//   a[i] := v          v, then a, then i, then the store;
//   a[i] op= v, a[i]++ a, then i, then the element, then v.
Value *Generator::genIndexAssign(const IndexExpr &target, const AssignExpr *assign, const IncDecExpr *incdec) {
  Builder &b = *cur_.b;
  if (assign && !assign->compound) {
    Value *value = genExpr(*assign->value);
    Value *base = genExpr(*target.base);
    Value *idx = genExpr(*target.index);
    b.indexSet(base, idx, value);
    return value;
  }
  Value *base = genExpr(*target.base);
  Value *idx = genExpr(*target.index);
  Value *elem = b.index(base, idx);
  Value *rhs = assign ? genExpr(*assign->value) : b.integer(1, 0);
  BinOp op = assign ? assign->op : incdec->increment ? BinOp::Add : BinOp::Sub;
  Value *now = b.binary(op, elem, rhs);
  b.indexSet(base, idx, now);
  return incdec && !incdec->prefix ? elem : now;
}

// Instance variable assignment, each operand once: the object first, then
//   o:x := v           v, then the message _X with it;
//   o:x op= v, o:x++   the message X, then v, then _X with the new value.
// The expression's value is what _X returns (a variable may keep a converted
// value, or ignore it), except for o:x++ and o:x--, whose value is the old one.
Value *Generator::genSendAssign(const SendExpr &target, const AssignExpr *assign, const IncDecExpr *incdec) {
  Builder &b = *cur_.b;
  Value *object = genExpr(*target.object);
  std::string setter = "_" + target.message;
  if (assign && !assign->compound) {
    Value *value = genExpr(*assign->value);
    b.setLoc(target.loc);
    return b.send(setter, object, {value});
  }
  b.setLoc(target.loc);
  Value *old = b.send(target.message, object, {});
  Value *rhs = assign ? genExpr(*assign->value) : b.integer(1, 0);
  BinOp op = assign ? assign->op : incdec->increment ? BinOp::Add : BinOp::Sub;
  Value *now = b.binary(op, old, rhs);
  b.setLoc(target.loc);
  Value *result = b.send(setter, object, {now});
  return incdec && !incdec->prefix ? old : result;
}

// .AND. and .OR. as a value: the left side decides (it must be logical) and,
// when it does not settle the result, the right side's value is the result as
// it is, whatever its type, as in Clipper (`.T. .AND. 5` is 5).
Value *Generator::genLogic(const LogicExpr &l) {
  Builder &b = *cur_.b;
  bool isAnd = l.kind == ExprKind::And;
  Value *lv = genCond(*l.lhs);
  BasicBlock *rhs = cur_.fn->addBlock(isAnd ? "and.rhs" : "or.rhs");
  BasicBlock *join = cur_.fn->addBlock(isAnd ? "and.end" : "or.end");
  Value *result = join->addParam(Type::Item);
  if (isAnd) b.condBr(lv, rhs, join, {}, {b.logical(false)});
  else b.condBr(lv, join, rhs, {b.logical(true)}, {});
  b.setBlock(rhs);
  b.br(join, {genExpr(*l.rhs)});
  b.setBlock(join);
  return result;
}

Value *Generator::genIIf(const IIfExpr &e) {
  Builder &b = *cur_.b;
  Value *c = genCond(*e.cond);
  BasicBlock *thenBB = cur_.fn->addBlock("iif.then");
  BasicBlock *elseBB = cur_.fn->addBlock("iif.else");
  BasicBlock *end = cur_.fn->addBlock("iif.end");
  Value *result = end->addParam(Type::Item);
  b.condBr(c, thenBB, elseBB);
  b.setBlock(thenBB);
  b.br(end, {genExpr(*e.then)});
  b.setBlock(elseBB);
  b.br(end, {genExpr(*e.otherwise)});
  b.setBlock(end);
  return result;
}

// The block receives the cells it captures: the current function's shared
// variables (moved into cells if they are not yet), or cells the current
// block itself captured.
Value *Generator::genCodeblock(const BlockExpr &blk) {
  Function *fn = genCodeblockFunction(blk);
  std::vector<Var *> captures;
  for (const BlockExpr::Capture &cap : blk.captures) {
    if (cap.fromParentCapture) {
      for (auto &v : cur_.fn->vars)
        if (v->kind == Var::Kind::Capture && v->index == cap.index) captures.push_back(v.get());
    } else {
      captures.push_back(frameVar(cap.index));
    }
  }
  cur_.b->setLoc(blk.loc);
  return cur_.b->makeBlock(fn, std::move(captures));
}

} // namespace xb::xir
