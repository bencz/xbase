// The traversal of the syntax tree every pass that only goes through it uses (the folder, the call resolver, the AST
// passes, the printer and the verifier): the children of each node in source order, each as the pointer that holds it,
// which a visitor may replace.
#pragma once

#include "parser/AST.h"

namespace xb {

// The child expressions of `e` (an extended codeblock's statements are its child block: forEachChildBlock).
template <class F> void forEachChildExpr(Expr &e, F &&f) {
  switch (e.kind) {
  case ExprKind::Nil: case ExprKind::Logical: case ExprKind::Number: case ExprKind::String: case ExprKind::Var:
  case ExprKind::Spread: case ExprKind::MacroText:
    return;
  case ExprKind::Call:
    for (auto &a : static_cast<CallExpr &>(e).args) f(a);
    return;
  case ExprKind::Unary: f(static_cast<UnaryExpr &>(e).operand); return;
  case ExprKind::Await: f(static_cast<AwaitExpr &>(e).operand); return;
  case ExprKind::Binary: {
    auto &b = static_cast<BinaryExpr &>(e);
    f(b.lhs);
    f(b.rhs);
    return;
  }
  case ExprKind::And: case ExprKind::Or: {
    auto &b = static_cast<LogicExpr &>(e);
    f(b.lhs);
    f(b.rhs);
    return;
  }
  case ExprKind::Assign: {
    auto &a = static_cast<AssignExpr &>(e);
    f(a.target);
    f(a.value);
    return;
  }
  case ExprKind::IncDec: f(static_cast<IncDecExpr &>(e).target); return;
  case ExprKind::IIf: {
    auto &i = static_cast<IIfExpr &>(e);
    f(i.cond);
    f(i.then);
    f(i.otherwise);
    return;
  }
  case ExprKind::Ref: f(static_cast<RefExpr &>(e).target); return;
  case ExprKind::List:
    for (auto &x : static_cast<ListExpr &>(e).exprs) f(x);
    return;
  case ExprKind::ArrayLit:
    for (auto &x : static_cast<ArrayLitExpr &>(e).elems) f(x);
    return;
  case ExprKind::Index: {
    auto &ix = static_cast<IndexExpr &>(e);
    f(ix.base);
    f(ix.index);
    return;
  }
  case ExprKind::Block:
    for (auto &x : static_cast<BlockExpr &>(e).body) f(x);
    return;
  case ExprKind::Send: {
    auto &s = static_cast<SendExpr &>(e);
    f(s.object);
    if (s.messageText) f(s.messageText);
    for (auto &a : s.args) f(a);
    return;
  }
  case ExprKind::HashLit: {
    auto &h = static_cast<HashLitExpr &>(e);
    for (size_t k = 0; k < h.keys.size(); ++k) {
      f(h.keys[k]);
      f(h.values[k]);
    }
    return;
  }
  case ExprKind::Macro: f(static_cast<MacroExpr &>(e).text); return;
  case ExprKind::MacroCall: {
    auto &m = static_cast<MacroCallExpr &>(e);
    f(m.text);
    for (auto &a : m.args) f(a);
    return;
  }
  case ExprKind::Field:
    if (auto &x = static_cast<FieldExpr &>(e); x.aliasExpr) f(x.aliasExpr);
    return;
  }
}

// The statements an expression holds: an extended codeblock's.
template <class F> void forEachChildBlock(Expr &e, F &&f) {
  if (e.kind == ExprKind::Block) f(static_cast<BlockExpr &>(e).statements);
}

// The parts of statement `s`: its expressions through `expr` (declarations' macro texts and initializers, loop
// variables, conditions, values) and its nested blocks through `block`.
template <class FE, class FB> void forEachPart(Stmt &s, FE &&expr, FB &&block) {
  auto decls = [&](std::vector<LocalDecl> &ds) {
    for (LocalDecl &d : ds) {
      if (d.macro) expr(d.macro);
      if (d.init) expr(d.init);
    }
  };
  switch (s.kind) {
  case StmtKind::Expr: expr(static_cast<ExprStmt &>(s).expr); return;
  case StmtKind::Local: case StmtKind::Static: case StmtKind::Private: case StmtKind::Public:
  case StmtKind::Parameters: case StmtKind::Memvar: case StmtKind::Field:
    decls(static_cast<LocalStmt &>(s).decls);
    return;
  case StmtKind::Return:
    if (auto &r = static_cast<ReturnStmt &>(s); r.value) expr(r.value);
    return;
  case StmtKind::Yield: expr(static_cast<YieldStmt &>(s).value); return;
  case StmtKind::If: {
    auto &i = static_cast<IfStmt &>(s);
    for (auto &arm : i.arms) {
      expr(arm.cond);
      block(arm.body);
    }
    block(i.elseBody);
    return;
  }
  case StmtKind::While: {
    auto &w = static_cast<WhileStmt &>(s);
    expr(w.cond);
    block(w.body);
    return;
  }
  case StmtKind::For: {
    auto &f = static_cast<ForStmt &>(s);
    expr(f.var);
    expr(f.start);
    expr(f.limit);
    if (f.step) expr(f.step);
    block(f.body);
    return;
  }
  case StmtKind::ForEach: {
    auto &f = static_cast<ForEachStmt &>(s);
    if (f.key) expr(f.key);
    for (auto &v : f.vars) expr(v);
    for (auto &c : f.collections) expr(c);
    block(f.body);
    return;
  }
  case StmtKind::Switch: {
    auto &w = static_cast<SwitchStmt &>(s);
    expr(w.value);
    for (auto &c : w.cases) {
      if (c.value) expr(c.value);
      block(c.body);
    }
    return;
  }
  case StmtKind::WithObject: {
    auto &w = static_cast<WithObjectStmt &>(s);
    expr(w.object);
    block(w.body);
    return;
  }
  case StmtKind::Sequence: {
    auto &q = static_cast<SequenceStmt &>(s);
    if (q.handler) expr(q.handler);
    block(q.body);
    if (q.usingVar) expr(q.usingVar);
    block(q.recover);
    block(q.always);
    return;
  }
  case StmtKind::Using: {
    auto &u = static_cast<UsingStmt &>(s);
    expr(u.value);
    block(u.seq);
    return;
  }
  case StmtKind::AwaitEach: {
    auto &a = static_cast<AwaitEachStmt &>(s);
    decls(a.hidden);
    block(a.seq);
    return;
  }
  case StmtKind::Exit: case StmtKind::Loop: return;
  }
}

// Every expression of a block, its statements' and the codeblocks' within, in source order, before its children
// (`f(ExprPtr &)`); what `f` replaces a node with is not visited.
template <class F> void forEachExprIn(Block &b, F &&f);

template <class F> void forEachExprUnder(ExprPtr &e, F &&f) {
  ExprPtr *slot = &e;
  Expr *before = e.get();
  f(*slot);
  if (slot->get() != before) return;
  forEachChildExpr(*e, [&](ExprPtr &c) { forEachExprUnder(c, f); });
  forEachChildBlock(*e, [&](Block &x) { forEachExprIn(x, f); });
}

template <class F> void forEachExprIn(Block &b, F &&f) {
  for (auto &s : b)
    forEachPart(*s, [&](ExprPtr &e) { forEachExprUnder(e, f); }, [&](Block &x) { forEachExprIn(x, f); });
}

} // namespace xb
