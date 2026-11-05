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
  void codeblock(BlockExpr &b, Scope &s);
  bool resolve(VarExpr &v);
  struct Frame;
  void markBoxed(Frame &f, uint32_t slot);
  void declare(LocalDecl &d, bool isStatic, Scope &s);
  bool constantInit(const Expr &e, unsigned blockDepth) const;

  struct Binding {
    VarWhere where;              // Frame or Static
    uint32_t index;
  };
  struct Frame {                 // the module (file STATICs), a function or a codeblock
    std::unordered_map<std::string, Binding> names;
    std::vector<bool> *boxed;    // the function's table; null for the module and codeblocks
    BlockExpr *block;            // the codeblock, or null
  };
  std::vector<Frame> frames_;
  Module *module_ = nullptr;
  Diagnostics &diag_;
  FuncDecl *curFunc_ = nullptr;
};

} // namespace xb
