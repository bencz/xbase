// Call resolution against the module's functions and EXTERN FUNCTION declarations.
#include "sema/Calls.h"

#include "common/Text.h"

namespace xb {

void CallResolver::run(Module &m) {
  for (const ExternDecl &d : m.externs) {
    auto [it, fresh] = externs_.emplace(d.name, &d);
    if (!fresh) {
      const ExternDecl &p = *it->second;
      if (p.minArgs != d.minArgs || p.maxArgs != d.maxArgs || p.native != d.native || p.reserved != d.reserved ||
          p.checked != d.checked || p.fold != d.fold || p.abbrev != d.abbrev) {
        diag_.error(d.loc, "EXTERN FUNCTION '" + d.name + "' conflicts with an earlier declaration");
        diag_.note(p.loc, "previous declaration is here");
      }
      continue;
    }
    if (d.abbrev) abbreviable_.push_back(&d);
    if (d.fold && !foldable_(d.name))
      diag_.error(d.loc, "FOLD: the compiler cannot evaluate '" + d.name + "'");
  }
  for (auto &f : m.funcs) {
    const ExternDecl *d = findExtern(f->name, f->loc);
    if (d && d->reserved)
      diag_.error(f->loc, "redefinition of predefined function " + d->name + " as '" + f->name + "'");
    funcs_.insert(f->name);
  }
  for (LocalDecl &d : m.fileStatics)
    if (d.init) expr(*d.init);
  for (auto &f : m.funcs) block(f->body);
}

// The declaration a name designates: the exact name, or an abbreviation of a
// declaration that allows one (at least `abbrev` letters), as keywords are.
// An abbreviation that fits several declarations is reported at `loc`.
const ExternDecl *CallResolver::findExtern(const std::string &name, SourceLoc loc) const {
  auto it = externs_.find(name);
  if (it != externs_.end()) return it->second;
  const ExternDecl *found = nullptr;
  for (const ExternDecl *d : abbreviable_) {
    if (!keywordMatches(name, d->name, d->abbrev)) continue;
    if (found) {
      diag_.error(loc, "'" + name + "' abbreviates both " + found->name + " and " + d->name);
      return found;
    }
    found = d;
  }
  return found;
}

void CallResolver::call(CallExpr &c) {
  const ExternDecl *d = findExtern(c.name, c.loc);
  if (!c.declaredOnly && funcs_.count(c.name)) { // the module's own functions come first
    if (d && d->name == c.name) checkArgs(c, *d);   // a CHECKED name keeps its argument count when redefined
    return;
  }
  if (!d) {
    diag_.error(c.loc, "function '" + c.name + "' is not declared (define it, or declare it with EXTERN FUNCTION)");
    return;
  }
  c.name = d->name;
  c.callee = d;
  checkArgs(c, *d);
}

void CallResolver::checkArgs(const CallExpr &c, const ExternDecl &d) {
  size_t n = c.args.size();
  if (d.checked && (n < d.minArgs || (d.maxArgs != ExternDecl::kUnlimited && n > d.maxArgs))) {
    std::string expected = d.minArgs == d.maxArgs ? std::to_string(d.minArgs)
                           : d.maxArgs == ExternDecl::kUnlimited
                               ? "at least " + std::to_string(d.minArgs)
                               : "from " + std::to_string(d.minArgs) + " to " + std::to_string(d.maxArgs);
    diag_.error(c.loc, "incorrect number of arguments in " + d.name + ": passed " + std::to_string(n) +
                           ", expected " + expected);
  }
}

void CallResolver::expr(Expr &e) {
  switch (e.kind) {
  case ExprKind::Nil: case ExprKind::Logical: case ExprKind::Number: case ExprKind::String: case ExprKind::Var:
    break;
  case ExprKind::Call: {
    auto &c = static_cast<CallExpr &>(e);
    for (auto &a : c.args) expr(*a);
    call(c);
    break;
  }
  case ExprKind::Unary:
    expr(*static_cast<UnaryExpr &>(e).operand);
    break;
  case ExprKind::Binary: {
    auto &b = static_cast<BinaryExpr &>(e);
    expr(*b.lhs);
    expr(*b.rhs);
    break;
  }
  case ExprKind::And: case ExprKind::Or: {
    auto &b = static_cast<LogicExpr &>(e);
    expr(*b.lhs);
    expr(*b.rhs);
    break;
  }
  case ExprKind::Assign: {
    auto &a = static_cast<AssignExpr &>(e);
    expr(*a.target);
    expr(*a.value);
    break;
  }
  case ExprKind::IncDec:
    expr(*static_cast<IncDecExpr &>(e).target);
    break;
  case ExprKind::ArrayLit:
    for (auto &el : static_cast<ArrayLitExpr &>(e).elems) expr(*el);
    break;
  case ExprKind::Index: {
    auto &ix = static_cast<IndexExpr &>(e);
    expr(*ix.base);
    expr(*ix.index);
    break;
  }
  case ExprKind::Block:
    for (auto &b : static_cast<BlockExpr &>(e).body) expr(*b);
    break;
  case ExprKind::IIf: {
    auto &i = static_cast<IIfExpr &>(e);
    expr(*i.cond);
    expr(*i.then);
    expr(*i.otherwise);
    break;
  }
  }
}

void CallResolver::block(Block &b) {
  for (auto &st : b) {
    switch (st->kind) {
    case StmtKind::Expr:
      expr(*static_cast<ExprStmt &>(*st).expr);
      break;
    case StmtKind::Local: case StmtKind::Static:
      for (LocalDecl &d : static_cast<LocalStmt &>(*st).decls)
        if (d.init) expr(*d.init);
      break;
    case StmtKind::Return: {
      auto &r = static_cast<ReturnStmt &>(*st);
      if (r.value) expr(*r.value);
      break;
    }
    case StmtKind::If: {
      auto &i = static_cast<IfStmt &>(*st);
      for (auto &arm : i.arms) {
        expr(*arm.cond);
        block(arm.body);
      }
      block(i.elseBody);
      break;
    }
    case StmtKind::While: {
      auto &w = static_cast<WhileStmt &>(*st);
      expr(*w.cond);
      block(w.body);
      break;
    }
    case StmtKind::For: {
      auto &f = static_cast<ForStmt &>(*st);
      expr(*f.start);
      expr(*f.limit);
      if (f.step) expr(*f.step);
      block(f.body);
      break;
    }
    case StmtKind::ForEach: {
      auto &f = static_cast<ForEachStmt &>(*st);
      for (auto &c : f.collections) expr(*c);
      block(f.body);
      break;
    }
    case StmtKind::Switch: {
      auto &w = static_cast<SwitchStmt &>(*st);
      expr(*w.value);
      for (auto &c : w.cases) {
        if (c.value) expr(*c.value);
        block(c.body);
      }
      break;
    }
    case StmtKind::Exit: case StmtKind::Loop:
      break;
    }
  }
}

} // namespace xb
