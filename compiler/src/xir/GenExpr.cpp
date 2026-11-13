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
    return sendFrom(m.message, object, args, m.super); // a method receives values (see CodeGen::withRest)
  }
  case ExprKind::Ref: { // a call argument passed by reference, or a function reference
    const Expr &target = *static_cast<const RefExpr &>(e).target;
    if (target.kind == ExprKind::Call) return b.funcRef(calleeOf(static_cast<const CallExpr &>(target)));
    if (target.kind == ExprKind::Var) {
      auto &v = static_cast<const VarExpr &>(target);
      return v.where == VarWhere::Memvar ? b.memvar(Op::MvRef, v.name) : b.refVar(varOf(v));
    }
    auto &ix = static_cast<const IndexExpr &>(target);
    Value *base = genExpr(*ix.base);
    Value *idx = genExpr(*ix.index);
    return b.refIndex(base, idx);
  }
  case ExprKind::Spread: break; // only an element of a list (genArgs)
  case ExprKind::Macro: {
    Value *text = genExpr(*static_cast<const MacroExpr &>(e).text);
    b.setLoc(e.loc);
    return b.macro(Op::MacroGet, text);
  }
  case ExprKind::MacroText:
    return b.macro(Op::MacroText, b.string(static_cast<const MacroTextExpr &>(e).value));
  case ExprKind::MacroCall: {
    auto &m = static_cast<const MacroCallExpr &>(e);
    Value *text = genExpr(*m.text);
    std::vector<Value *> args = genArgs(m.args);
    b.setLoc(m.loc);
    return b.macro(Op::MacroCall, text, args);
  }
  case ExprKind::Field: {
    auto &f = static_cast<const FieldExpr &>(e);
    Value *area = f.aliasExpr ? genExpr(*f.aliasExpr) : nullptr;
    b.setLoc(f.loc);
    return b.field(f.alias, area, f.name);
  }
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

// The values of a list of arguments or elements, in order. `...`, the last, is
// an args.rest made right before the list's operation, which is then its one
// use in the same block.
std::vector<Value *> Generator::genArgs(const std::vector<ExprPtr> &exprs) {
  std::vector<Value *> values;
  for (auto &e : exprs) {
    if (e->kind != ExprKind::Spread) {
      values.push_back(genExpr(*e));
      continue;
    }
    values.push_back(cur_.b->argsRest());
    cur_.fn->usesRest = true;
  }
  return values;
}

// What a call (or a function reference) names: an EXTERN FUNCTION, another module's function (sema/Calls), or the
// module's own.
Callee Generator::calleeOf(const CallExpr &c) {
  Callee callee;
  if (c.callee || c.external) {
    callee.name = c.name;
    callee.native = c.callee && c.callee->native;
    callee.readsCallerArgs = c.callee && c.callee->args;
    if (c.callee) callee.byref = c.callee->refMask();
  } else {
    callee.function = funcs_.at(c.name);
  }
  return callee;
}

Value *Generator::genCall(const CallExpr &c) {
  std::vector<Value *> args = genArgs(c.args);
  Callee callee = calleeOf(c);
  cur_.b->setLoc(c.loc);
  return cur_.b->call(std::move(callee), args);
}

// x := v evaluates v, then stores; x op= v reads x, then evaluates v. The
// value of the expression is the value stored.
Value *Generator::genAssign(const AssignExpr &a) {
  if (a.target->kind == ExprKind::Index) return genIndexAssign(static_cast<const IndexExpr &>(*a.target), &a, nullptr);
  if (a.target->kind == ExprKind::Send) return genSendAssign(static_cast<const SendExpr &>(*a.target), &a, nullptr);
  if (a.target->kind == ExprKind::Macro) return genMacroAssign(static_cast<const MacroExpr &>(*a.target), &a, nullptr);
  if (a.target->kind == ExprKind::Field) return genFieldAssign(static_cast<const FieldExpr &>(*a.target), &a, nullptr);
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
  if (e.target->kind == ExprKind::Macro) return genMacroAssign(static_cast<const MacroExpr &>(*e.target), nullptr, &e);
  if (e.target->kind == ExprKind::Field) return genFieldAssign(static_cast<const FieldExpr &>(*e.target), nullptr, &e);
  auto &target = static_cast<const VarExpr &>(*e.target);
  Value *old = load(target);
  Value *now = cur_.b->stepByOne(e.increment, old);
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
  Value *now = assign ? b.binary(assign->op, elem, genExpr(*assign->value)) : b.stepByOne(incdec->increment, elem);
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
    return sendFrom(setter, object, {value}, target.super);
  }
  b.setLoc(target.loc);
  Value *old = sendFrom(target.message, object, {}, target.super);
  Value *now = assign ? b.binary(assign->op, old, genExpr(*assign->value)) : b.stepByOne(incdec->increment, old);
  b.setLoc(target.loc);
  Value *result = sendFrom(setter, object, {now}, target.super);
  return incdec && !incdec->prefix ? old : result;
}

// Macro assignment, the text evaluated once:
//   &x := v            v, then the text, then the store;
//   &x op= v, &x++     the text, then its value, then v, then the store.
Value *Generator::genMacroAssign(const MacroExpr &target, const AssignExpr *assign, const IncDecExpr *incdec) {
  Builder &b = *cur_.b;
  if (assign && !assign->compound) {
    Value *value = genExpr(*assign->value);
    Value *text = genExpr(*target.text);
    b.setLoc(target.loc);
    b.macro(Op::MacroSet, text, {value});
    return value;
  }
  Value *text = genExpr(*target.text);
  b.setLoc(target.loc);
  Value *old = b.macro(Op::MacroGet, text);
  Value *now = assign ? b.binary(assign->op, old, genExpr(*assign->value)) : b.stepByOne(incdec->increment, old);
  b.setLoc(target.loc);
  b.macro(Op::MacroSet, text, {now});
  return incdec && !incdec->prefix ? old : now;
}

// Field assignment, each operand once:
//   alias->x := v      v, then the work area's value (for (expr)->x), then the store;
//   alias->x op= v     the work area's value, then the field, then v, then the store.
Value *Generator::genFieldAssign(const FieldExpr &target, const AssignExpr *assign, const IncDecExpr *incdec) {
  Builder &b = *cur_.b;
  if (assign && !assign->compound) {
    Value *value = genExpr(*assign->value);
    Value *area = target.aliasExpr ? genExpr(*target.aliasExpr) : nullptr;
    b.setLoc(target.loc);
    b.field(target.alias, area, target.name, value);
    return value;
  }
  Value *area = target.aliasExpr ? genExpr(*target.aliasExpr) : nullptr;
  b.setLoc(target.loc);
  Value *old = b.field(target.alias, area, target.name);
  Value *now = assign ? b.binary(assign->op, old, genExpr(*assign->value)) : b.stepByOne(incdec->increment, old);
  b.setLoc(target.loc);
  b.field(target.alias, area, target.name, now);
  return incdec && !incdec->prefix ? old : now;
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
