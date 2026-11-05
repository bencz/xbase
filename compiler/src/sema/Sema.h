#pragma once

#include "parser/AST.h"
#include "common/Diagnostics.h"

#include <string>
#include <unordered_map>
#include <vector>

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
  void codeblock(BlockExpr &b, Scope &s);
  bool resolve(VarExpr &v);

  struct Frame {                 // a function or codeblock being analyzed
    std::unordered_map<std::string, uint32_t> slots;
    std::vector<bool> *boxed;    // the function's table; null for a codeblock
    BlockExpr *block;            // null for the function
  };
  std::vector<Frame> frames_;
  Diagnostics &diag_;
  FuncDecl *curFunc_ = nullptr;
};

} // namespace xb
