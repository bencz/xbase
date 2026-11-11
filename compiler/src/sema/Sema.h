#pragma once

#include "parser/AST.h"
#include "common/Diagnostics.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace xb {

// Resolves names (params / LOCALs -> frame slots, STATICs, captures; any other
// name is a memory variable) and checks the structural rules the parser
// cannot see.
class Sema {
public:
  // A name nothing declares (no LOCAL, STATIC, MEMVAR, PRIVATE, PUBLIC or
  // PARAMETERS) is a memory variable, as in Clipper; the compiler warns about
  // it, says nothing, or rejects it.
  enum class Undeclared : uint8_t { Warn, Ignore, Error };
  explicit Sema(Diagnostics &diag, Undeclared undeclared = Undeclared::Warn) : diag_(diag), undeclared_(undeclared) {}
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
  bool declaredVariable(const std::string &name) const;
  struct Frame;
  void markBoxed(Frame &f, uint32_t slot);
  void declare(LocalDecl &d, bool isStatic, Scope &s);
  void declareMemvar(const LocalDecl &d);
  void createdMemvar(const LocalDecl &d);
  bool memvarCreated(const std::string &name) const;
  bool constantInit(const Expr &e, unsigned blockDepth) const;

  struct Binding {
    VarWhere where;              // Frame or Static
    uint32_t index;
  };
  struct Frame {                 // the module (file STATICs), a function or a codeblock
    std::unordered_map<std::string, Binding> names;
    // MEMVAR names: memory variables, before any STATIC of an enclosing frame. PRIVATE, PUBLIC and PARAMETERS names
    // are not declarations (the reference keeps resolving them as before), only no longer undeclared.
    std::unordered_set<std::string> memvars, created;
    std::vector<bool> *boxed;    // the function's table; null for the module and codeblocks
    BlockExpr *block;            // the codeblock, or null
  };
  std::vector<Frame> frames_;
  Module *module_ = nullptr;
  Diagnostics &diag_;
  Undeclared undeclared_;
  FuncDecl *curFunc_ = nullptr;
};

} // namespace xb
