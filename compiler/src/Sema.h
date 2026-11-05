#pragma once

#include "AST.h"
#include "Diagnostics.h"

namespace xb {

// Resolves names (params / LOCALs -> frame slots) and checks the structural
// rules the parser cannot see. Undeclared variables will become MEMVAR/FIELD
// references once those exist; for now they are errors.
class Sema {
public:
  explicit Sema(Diagnostics &diag) : diag_(diag) {}
  void run(Module &m);

private:
  struct Scope;
  void func(FuncDecl &f);
  void block(Block &b, Scope &s);
  void nestedBlock(Block &b, Scope &s);
  void stmt(Stmt &st, Scope &s);
  void expr(Expr &e, Scope &s);
  void lvalue(Expr &e, Scope &s);

  Diagnostics &diag_;
  FuncDecl *curFunc_ = nullptr;
};

} // namespace xb
