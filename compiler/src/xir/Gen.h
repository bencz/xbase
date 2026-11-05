#pragma once

#include "parser/AST.h"
#include "xir/Builder.h"
#include "xir/IR.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace xb::xir {

// Lowers a checked syntax tree (after call resolution, folding and Sema) to
// xIR. It decides evaluation order and how every variable is reached; the
// copies Clipper's order requires follow from SSA (a load is the value at
// that point) and are made only where needed when lowering to LLVM.
class Generator {
public:
  std::unique_ptr<Module> run(const xb::Module &ast);

private:
  // Gen.cpp: module, functions, codeblocks, variables
  Function *declare(const FuncDecl &f);
  void genFunction(const FuncDecl &f, Function *fn);
  void genModuleInit(const xb::Module &ast);
  Function *genCodeblockFunction(const BlockExpr &b);
  Var *frameVar(uint32_t slot);
  Var *varOf(const VarExpr &v);
  Access accessOf(const VarExpr &v);
  static Access accessOf(const Var &v);
  bool isEachVar(const VarExpr &v) const;
  Value *load(const VarExpr &v);
  void store(const VarExpr &v, Value *value);

  // GenStmt.cpp
  void genBlock(const Block &b);
  void genStmt(const Stmt &s);
  void genLocals(const LocalStmt &s);
  void genIf(const IfStmt &s);
  void genWhile(const WhileStmt &s);
  void genFor(const ForStmt &s);
  void genForEach(const ForEachStmt &s);
  struct EachVar;
  Value *eachInCell(const EachVar &v);
  void genSwitch(const SwitchStmt &s);
  void genReturn(const ReturnStmt &s);
  void startDeadBlock();

  // GenExpr.cpp
  Value *genExpr(const Expr &e);
  void genDiscard(const Expr &e);
  Value *genCond(const Expr &e);
  Value *genCall(const CallExpr &c);
  Value *genAssign(const AssignExpr &a);
  Value *genIncDec(const IncDecExpr &e);
  Value *genIndexAssign(const IndexExpr &target, const AssignExpr *assign, const IncDecExpr *incdec);
  Value *genIIf(const IIfExpr &e);
  Value *genCodeblock(const BlockExpr &b);

  std::unique_ptr<Module> mod_;
  std::unordered_map<std::string, Function *> funcs_; // the module's program functions
  unsigned blockCount_ = 0;

  // The function being generated (saved while a codeblock's is generated).
  struct Loop {
    BasicBlock *exitTo;
    BasicBlock *loopTo;            // null inside a SWITCH that is not in a loop
  };
  struct EachVar {                 // the variable of a FOR EACH loop being generated
    const VarExpr *var;
    Value *inCell;                 // Bool: the loop works inside the variable's cell
    Value *saved;                  // its value before the loop
  };
  struct FnState {
    Function *fn = nullptr;
    std::unique_ptr<Builder> b;
    std::vector<Loop> loops;
    std::vector<EachVar> eachVars;
    std::string scope;             // name prefix for codeblock functions
  };
  FnState cur_;
};

} // namespace xb::xir
