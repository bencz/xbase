#include "Sema.h"

#include <unordered_map>

namespace xb {

struct Sema::Scope {
  std::unordered_map<std::string, uint32_t> slots;
  uint32_t next = 0;
  unsigned loopDepth = 0;
  unsigned blockDepth = 0;       // nesting inside IF / WHILE / FOR
  bool seenExecutable = false;   // declarations must precede executable statements
};

void Sema::run(Module &m) {
  std::unordered_map<std::string, SourceLoc> seen;
  for (auto &f : m.funcs) {
    auto [it, fresh] = seen.emplace(f->name, f->loc);
    if (!fresh) {
      diag_.error(f->loc, "function '" + f->name + "' is already defined");
      diag_.note(it->second, "previous definition is here");
    }
    func(*f);
  }
}

void Sema::func(FuncDecl &f) {
  curFunc_ = &f;
  Scope s;
  for (const Param &p : f.params) {
    if (!s.slots.emplace(p.name, s.next).second) diag_.error(p.loc, "duplicate parameter '" + p.name + "'");
    else ++s.next;
  }
  block(f.body, s);
  f.numLocals = s.next;
  curFunc_ = nullptr;
}

void Sema::block(Block &b, Scope &s) {
  for (auto &st : b) {
    if (st->kind != StmtKind::Local) s.seenExecutable = true;
    stmt(*st, s);
  }
}

void Sema::nestedBlock(Block &b, Scope &s) {
  ++s.blockDepth;
  block(b, s);
  --s.blockDepth;
}

void Sema::stmt(Stmt &st, Scope &s) {
  switch (st.kind) {
  case StmtKind::Expr:
    expr(*static_cast<ExprStmt &>(st).expr, s);
    break;
  case StmtKind::Local:
    if (s.seenExecutable || s.blockDepth) diag_.error(st.loc, "LOCAL declaration follows executable statement");
    for (LocalDecl &d : static_cast<LocalStmt &>(st).decls) {
      if (d.init) expr(*d.init, s); // resolved before the name is visible
      if (s.slots.count(d.name)) {
        diag_.error(d.loc, "'" + d.name + "' is already declared in this function");
        continue;
      }
      d.slot = s.next++;
      s.slots.emplace(d.name, d.slot);
    }
    break;
  case StmtKind::Return: {
    auto &r = static_cast<ReturnStmt &>(st);
    if (r.value) {
      if (curFunc_->isProcedure && !curFunc_->implicit)
        diag_.error(r.loc, "PROCEDURE '" + curFunc_->name + "' cannot return a value");
      expr(*r.value, s);
    }
    break;
  }
  case StmtKind::If: {
    auto &i = static_cast<IfStmt &>(st);
    for (auto &arm : i.arms) {
      expr(*arm.cond, s);
      nestedBlock(arm.body, s);
    }
    nestedBlock(i.elseBody, s);
    break;
  }
  case StmtKind::While: {
    auto &w = static_cast<WhileStmt &>(st);
    expr(*w.cond, s);
    ++s.loopDepth;
    nestedBlock(w.body, s);
    --s.loopDepth;
    break;
  }
  case StmtKind::For: {
    auto &f = static_cast<ForStmt &>(st);
    lvalue(*f.var, s);
    expr(*f.start, s);
    expr(*f.limit, s);
    if (f.step) expr(*f.step, s);
    ++s.loopDepth;
    nestedBlock(f.body, s);
    --s.loopDepth;
    break;
  }
  case StmtKind::Exit:
  case StmtKind::Loop:
    if (!s.loopDepth) {
      diag_.error(st.loc, st.kind == StmtKind::Exit ? "EXIT outside of a loop" : "LOOP outside of a loop");
    }
    break;
  }
}

void Sema::lvalue(Expr &e, Scope &s) {
  if (e.kind != ExprKind::Var) {
    diag_.error(e.loc, "invalid assignment target");
    return;
  }
  expr(e, s);
}

void Sema::expr(Expr &e, Scope &s) {
  switch (e.kind) {
  case ExprKind::Nil: case ExprKind::Logical: case ExprKind::Number: case ExprKind::String:
    break;
  case ExprKind::Var: {
    auto &v = static_cast<VarExpr &>(e);
    auto it = s.slots.find(v.name);
    if (it == s.slots.end()) {
      diag_.error(v.loc, "variable '" + v.name + "' is not declared (MEMVAR/FIELD not supported yet)");
      return;
    }
    v.slot = it->second;
    break;
  }
  case ExprKind::Call:
    for (auto &a : static_cast<CallExpr &>(e).args) expr(*a, s);
    break;
  case ExprKind::Unary:
    expr(*static_cast<UnaryExpr &>(e).operand, s);
    break;
  case ExprKind::Binary: {
    auto &b = static_cast<BinaryExpr &>(e);
    expr(*b.lhs, s);
    expr(*b.rhs, s);
    break;
  }
  case ExprKind::And: case ExprKind::Or: {
    auto &b = static_cast<LogicExpr &>(e);
    expr(*b.lhs, s);
    expr(*b.rhs, s);
    break;
  }
  case ExprKind::Assign: {
    auto &a = static_cast<AssignExpr &>(e);
    expr(*a.value, s);
    lvalue(*a.target, s);
    break;
  }
  case ExprKind::IncDec:
    lvalue(*static_cast<IncDecExpr &>(e).target, s);
    break;
  case ExprKind::IIf: {
    auto &i = static_cast<IIfExpr &>(e);
    expr(*i.cond, s);
    expr(*i.then, s);
    expr(*i.otherwise, s);
    break;
  }
  }
}

} // namespace xb
