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
  // `runtimeLibrary`: a module of the runtime (xbc --runtime), whose STATIC
  // functions and codeblocks the call stack does not show.
  explicit Generator(bool runtimeLibrary = false) : runtimeLibrary_(runtimeLibrary) {}
  std::unique_ptr<Module> run(const xb::Module &ast);

private:
  // Gen.cpp: module, functions, codeblocks, variables
  Function *declare(const FuncDecl &f);
  void genFunction(const FuncDecl &f, Function *fn);
  void genBody(const FuncDecl &f, Function *fn, Function *byRef, const std::vector<uint32_t> &plain);
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
  void genMemvars(const LocalStmt &s);
  void genIf(const IfStmt &s);
  void genWhile(const WhileStmt &s);
  void genFor(const ForStmt &s);
  void genForEach(const ForEachStmt &s);
  struct EachVar;
  Value *eachInCell(const EachVar &v);
  void genSwitch(const SwitchStmt &s);
  void initLocal(const LocalDecl &d, Value *value);
  void genSequence(const SequenceStmt &s);
  void exitCleanups(size_t loops);
  void unwindOnward();
  void genReturn(const ReturnStmt &s);
  void startDeadBlock();

  // GenExpr.cpp
  Value *genExpr(const Expr &e);
  void genDiscard(const Expr &e);
  Value *genCond(const Expr &e);
  Value *genCall(const CallExpr &c);
  std::vector<Value *> genArgs(const std::vector<ExprPtr> &exprs);
  Value *genAssign(const AssignExpr &a);
  Value *genIncDec(const IncDecExpr &e);
  Value *genIndexAssign(const IndexExpr &target, const AssignExpr *assign, const IncDecExpr *incdec);
  Value *genSendAssign(const SendExpr &target, const AssignExpr *assign, const IncDecExpr *incdec);
  Value *genMacroAssign(const MacroExpr &target, const AssignExpr *assign, const IncDecExpr *incdec);
  Value *genLogic(const LogicExpr &l);
  Value *genIIf(const IIfExpr &e);
  Value *genCodeblock(const BlockExpr &b);

  bool runtimeLibrary_;
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
  struct Cleanup {                 // what a RETURN, EXIT or LOOP that leaves a sequence does first
    Value *oldBlock;               // the error block to put back (WITH), or null
    const Block *always;           // else the ALWAYS code to run
    BasicBlock *unwind;            // where that code unwinds: the sequence's own unwind block
    size_t loops;                  // the loops around the sequence (FnState::loops)
  };
  struct FnState {
    Function *fn = nullptr;
    std::unique_ptr<Builder> b;
    std::vector<Loop> loops;
    std::vector<EachVar> eachVars;
    std::vector<Cleanup> cleanups;
    std::string scope;             // name prefix for codeblock functions
    std::string function;          // the xBase function being generated (its codeblocks' too)
    std::unordered_map<uint32_t, Var *> paramLocals; // parameter slot -> the LOCAL standing for it
    int32_t classIndex = -1;       // a method's class (Module::classes), for its codeblocks too; -1 elsewhere
  };
  // A message sent from the current function: from a method (or its codeblocks), with its class.
  Instruction *sendFrom(const std::string &message, Value *object, const std::vector<Value *> &args,
                        bool super = false);
  void declareClasses(const xb::Module &ast);
  std::unordered_map<const ClassDecl *, int32_t> classIndex_;
  FnState cur_;
};

} // namespace xb::xir
