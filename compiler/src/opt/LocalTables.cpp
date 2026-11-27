// local-tables: a function's LOCAL initialized with an array literal of constants (numbers, logicals, NIL) that the
// function only reads as a table (VariableFacts: t[i] and Len( t ); never assigned after its declaration, captured,
// passed or assigned anywhere, its elements never assigned nor passed by reference): LocalDecl::table. Each call would
// make the same array, which nothing but those reads sees: the code generator makes the variable a table of its call
// (xir::Var::items, docs/xir.md), read from constant data, whose call makes its own array only where a read fails (an
// index out of bounds, not a number): the error block gets that array (e:args), which may change it, and the call's
// reads after it read it, as each call's own array would be.
#include "opt/AstPasses.h"
#include "opt/Fold.h"
#include "opt/VariableFacts.h"

namespace xb {

namespace {

// An element a table holds as constant data: NIL, a logical or a number whose decimals are known now (a folded
// quotient's are SET DECIMALS' when it runs: an instruction, numDefaultDec).
bool constantElement(const Expr &x) {
  if (x.kind == ExprKind::Nil || x.kind == ExprKind::Logical) return true;
  if (x.kind != ExprKind::Number) return false;
  const auto &n = static_cast<const NumberExpr &>(x);
  return n.isInt || n.dec != kDefaultDecimals;
}

bool constantTable(const Expr &e) {
  if (e.kind != ExprKind::ArrayLit) return false;
  const auto &a = static_cast<const ArrayLitExpr &>(e);
  if (a.elems.empty()) return false;
  for (const ExprPtr &x : a.elems)
    if (!constantElement(*x)) return false;
  return true;
}

bool markTables(FuncDecl &f) {
  VariableFacts facts = variableFacts(f);
  bool changed = false;
  for (auto &s : f.body) {
    if (s->kind != StmtKind::Local) continue;
    for (LocalDecl &d : static_cast<LocalStmt &>(*s).decls) {
      bool table = d.init && constantTable(*d.init) && !VariableFacts::has(facts.written, d.slot) &&
                   !VariableFacts::has(facts.captured, d.slot) && !VariableFacts::has(facts.otherUse, d.slot);
      changed |= table != d.table;
      d.table = table;
    }
  }
  return changed;
}

} // namespace

bool markLocalTables(Module &m) {
  bool changed = false;
  for (auto &f : m.funcs) changed |= markTables(*f);
  return changed;
}

} // namespace xb
