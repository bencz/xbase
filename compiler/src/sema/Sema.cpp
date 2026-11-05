#include "sema/Sema.h"

#include <algorithm>

namespace xb {

struct Sema::Scope {
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
  frames_.push_back({{}, &f.boxed, nullptr});
  for (const Param &p : f.params) {
    if (!frames_.back().slots.emplace(p.name, s.next).second) diag_.error(p.loc, "duplicate parameter '" + p.name + "'");
    else ++s.next;
  }
  block(f.body, s);
  f.numLocals = s.next;
  f.boxed.resize(f.numLocals);
  frames_.pop_back();
  curFunc_ = nullptr;
}

// Finds a variable in the current frame or an enclosing one. A variable of the
// enclosing function is captured: it is boxed in its frame, and every codeblock
// between there and here gets a capture that passes its cell along. Parameters
// of an enclosing codeblock cannot be captured.
bool Sema::resolve(VarExpr &v) {
  for (size_t fi = frames_.size(); fi-- > 0;) {
    auto it = frames_[fi].slots.find(v.name);
    if (it == frames_[fi].slots.end()) continue;
    uint32_t slot = it->second;
    if (fi + 1 == frames_.size()) {
      v.where = VarWhere::Frame;
      v.slot = slot;
      return true;
    }
    if (frames_[fi].block) { // Clipper: only a function's variables can be captured
      diag_.error(v.loc, "outer codeblock variable '" + v.name + "' is out of reach");
      return true;
    }
    std::vector<bool> &boxed = *frames_[fi].boxed;
    if (boxed.size() <= slot) boxed.resize(slot + 1);
    boxed[slot] = true;
    BlockExpr::Capture cap{false, slot};
    for (size_t k = fi + 1; k < frames_.size(); ++k) {
      auto &caps = frames_[k].block->captures;
      auto pos = std::find(caps.begin(), caps.end(), cap);
      uint32_t index = (uint32_t)(pos - caps.begin());
      if (pos == caps.end()) caps.push_back(cap);
      cap = {true, index};
    }
    v.where = VarWhere::Capture;
    v.slot = cap.index;
    return true;
  }
  return false;
}

void Sema::codeblock(BlockExpr &b, Scope &s) {
  frames_.push_back({{}, nullptr, &b}); // block parameters are never captured
  uint32_t n = 0;
  for (const Param &p : b.params) {
    if (!frames_.back().slots.emplace(p.name, n).second) diag_.error(p.loc, "duplicate parameter '" + p.name + "'");
    else ++n;
  }
  for (auto &e : b.body) expr(*e, s);
  b.numLocals = n;
  frames_.pop_back();
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
      if (frames_.back().slots.count(d.name)) {
        diag_.error(d.loc, "'" + d.name + "' is already declared in this function");
        continue;
      }
      d.slot = s.next++;
      frames_.back().slots.emplace(d.name, d.slot);
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
  if (e.kind != ExprKind::Var && e.kind != ExprKind::Index) {
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
    if (!resolve(v)) diag_.error(v.loc, "variable '" + v.name + "' is not declared (MEMVAR/FIELD not supported yet)");
    break;
  }
  case ExprKind::ArrayLit:
    for (auto &el : static_cast<ArrayLitExpr &>(e).elems) expr(*el, s);
    break;
  case ExprKind::Index: {
    auto &ix = static_cast<IndexExpr &>(e);
    expr(*ix.base, s);
    expr(*ix.index, s);
    break;
  }
  case ExprKind::Block:
    codeblock(static_cast<BlockExpr &>(e), s);
    break;
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
