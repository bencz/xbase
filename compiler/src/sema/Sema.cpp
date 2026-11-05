#include "sema/Sema.h"

#include <algorithm>
#include <set>

namespace xb {

struct Sema::Scope {
  uint32_t next = 0;
  unsigned loopDepth = 0;
  unsigned switchDepth = 0;
  unsigned blockDepth = 0;       // nesting inside a statement's body
  bool seenExecutable = false;   // declarations must precede executable statements
};

void Sema::run(Module &m) {
  module_ = &m;
  frames_.push_back({{}, nullptr, nullptr}); // the file's STATICs
  Scope fileScope;
  for (LocalDecl &d : m.fileStatics) declare(d, true, fileScope);
  std::unordered_map<std::string, SourceLoc> seen;
  for (auto &f : m.funcs) {
    auto [it, fresh] = seen.emplace(f->name, f->loc);
    if (!fresh) {
      diag_.error(f->loc, "function '" + f->name + "' is already defined");
      diag_.note(it->second, "previous definition is here");
    }
    func(*f);
  }
  frames_.pop_back();
}

// A STATIC is initialized once, before the program starts, so its initializer
// must be constant after folding: literals, arrays of constants, codeblocks
// whose variables are their parameters and STATICs (Clipper rule).
bool Sema::constantInit(const Expr &e, unsigned blockDepth) const {
  switch (e.kind) {
  case ExprKind::Nil: case ExprKind::Logical: case ExprKind::Number: case ExprKind::String:
    return true;
  case ExprKind::ArrayLit:
    for (auto &el : static_cast<const ArrayLitExpr &>(e).elems)
      if (!constantInit(*el, blockDepth)) return false;
    return true;
  case ExprKind::Block:
    for (auto &b : static_cast<const BlockExpr &>(e).body)
      if (!constantInit(*b, blockDepth + 1)) return false;
    return true;
  case ExprKind::Var: // a codeblock's own parameters and STATICs; a captured LOCAL has no value yet
    return blockDepth > 0 && static_cast<const VarExpr &>(e).where != VarWhere::Capture;
  default: // anything else runs code: allowed only inside a codeblock body
    return blockDepth > 0;
  }
}

// LOCAL and STATIC declarations. A LOCAL's initializer is resolved before
// its name is visible; a STATIC is visible in its own initializer, so a
// codeblock stored in it can call itself.
void Sema::declare(LocalDecl &d, bool isStatic, Scope &s) {
  if (!isStatic && d.init) expr(*d.init, s);
  bool duplicate = frames_.back().names.count(d.name) != 0;
  if (duplicate) {
    diag_.error(d.loc, "'" + d.name + "' is already declared");
  } else if (isStatic) {
    d.slot = (uint32_t)module_->statics.size();
    module_->statics.push_back(&d);
    frames_.back().names.emplace(d.name, Binding{VarWhere::Static, d.slot});
  } else {
    d.slot = s.next++;
    frames_.back().names.emplace(d.name, Binding{VarWhere::Frame, d.slot});
  }
  if (isStatic && d.init) {
    expr(*d.init, s);
    if (!constantInit(*d.init, 0))
      diag_.error(d.init->loc, "illegal initializer for STATIC '" + d.name + "': it must be constant");
  }
}

void Sema::func(FuncDecl &f) {
  curFunc_ = &f;
  Scope s;
  frames_.push_back({{}, &f.boxed, nullptr});
  for (const Param &p : f.params) {
    if (!frames_.back().names.emplace(p.name, Binding{VarWhere::Frame, s.next}).second)
      diag_.error(p.loc, "duplicate parameter '" + p.name + "'");
    else ++s.next;
  }
  block(f.body, s);
  f.numLocals = s.next;
  f.boxed.resize(f.numLocals);
  frames_.pop_back();
  curFunc_ = nullptr;
}

void Sema::markBoxed(Frame &f, uint32_t slot) {
  std::vector<bool> &boxed = *f.boxed;
  if (boxed.size() <= slot) boxed.resize(slot + 1);
  boxed[slot] = true;
}

// Finds a variable in the current frame or an enclosing one. A variable of the
// enclosing function is captured: it is marked shared in its frame, and every codeblock
// between there and here gets a capture that passes its cell along. Parameters
// of an enclosing codeblock cannot be captured.
bool Sema::resolve(VarExpr &v) {
  for (size_t fi = frames_.size(); fi-- > 0;) {
    auto it = frames_[fi].names.find(v.name);
    if (it == frames_[fi].names.end()) continue;
    uint32_t slot = it->second.index;
    if (it->second.where == VarWhere::Static) { // module storage: no capture needed
      v.where = VarWhere::Static;
      v.slot = slot;
      return true;
    }
    if (fi + 1 == frames_.size()) {
      v.where = VarWhere::Frame;
      v.slot = slot;
      return true;
    }
    if (frames_[fi].block) { // Clipper: only a function's variables can be captured
      diag_.error(v.loc, "outer codeblock variable '" + v.name + "' is out of reach");
      return true;
    }
    markBoxed(frames_[fi], slot);
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
  frames_.push_back({{}, nullptr, &b}); // parameters only: never captured, and tested for references anyway
  uint32_t n = 0;
  for (const Param &p : b.params) {
    if (!frames_.back().names.emplace(p.name, Binding{VarWhere::Frame, n}).second)
      diag_.error(p.loc, "duplicate parameter '" + p.name + "'");
    else ++n;
  }
  for (auto &e : b.body) expr(*e, s);
  b.numLocals = n;
  frames_.pop_back();
}

void Sema::block(Block &b, Scope &s) {
  for (auto &st : b) {
    if (st->kind != StmtKind::Local && st->kind != StmtKind::Static) s.seenExecutable = true;
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
  case StmtKind::Static: {
    bool isStatic = st.kind == StmtKind::Static;
    if (s.seenExecutable || s.blockDepth)
      diag_.error(st.loc, std::string(isStatic ? "STATIC" : "LOCAL") + " declaration follows executable statement");
    for (LocalDecl &d : static_cast<LocalStmt &>(st).decls) declare(d, isStatic, s);
    break;
  }
  case StmtKind::ForEach: {
    auto &f = static_cast<ForEachStmt &>(st);
    for (auto &v : f.vars) lvalue(*v, s);
    for (auto &c : f.collections) expr(*c, s);
    ++s.loopDepth;
    nestedBlock(f.body, s);
    --s.loopDepth;
    break;
  }
  case StmtKind::Switch: {
    auto &w = static_cast<SwitchStmt &>(st);
    expr(*w.value, s);
    ++s.switchDepth;
    std::set<int64_t> ints;
    std::set<std::string> strings;
    for (auto &c : w.cases) {
      if (c.value) {
        expr(*c.value, s);
        ExprKind k = c.value->kind;
        bool fresh = true;
        if (k == ExprKind::Number && static_cast<const NumberExpr &>(*c.value).isInt)
          fresh = ints.insert(static_cast<const NumberExpr &>(*c.value).ival).second;
        else if (k == ExprKind::String)
          fresh = strings.insert(static_cast<const StringExpr &>(*c.value).value).second;
        else
          diag_.error(c.value->loc, "CASE requires either numeric or string constant");
        if (!fresh) diag_.error(c.value->loc, "duplicate CASE value");
      }
      nestedBlock(c.body, s);
    }
    --s.switchDepth;
    break;
  }
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
  case StmtKind::Exit: // leaves the innermost loop or SWITCH
    if (!s.loopDepth && !s.switchDepth) diag_.error(st.loc, "EXIT outside of a loop");
    break;
  case StmtKind::Loop:
    if (!s.loopDepth) diag_.error(st.loc, "LOOP outside of a loop");
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
  case ExprKind::Ref: {
    // A variable passed by reference is shared with the callee: a function's
    // LOCAL or parameter is marked; a codeblock's parameters, STATICs and
    // captured variables are reached with a run-time test anyway.
    Expr &target = *static_cast<RefExpr &>(e).target;
    lvalue(target, s);
    if (target.kind == ExprKind::Var) {
      auto &v = static_cast<VarExpr &>(target);
      if (v.where == VarWhere::Frame && !frames_.back().block) markBoxed(frames_.back(), v.slot);
    }
    break;
  }
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
