#pragma once

#include "parser/AST.h"
#include "common/Diagnostics.h"

#include <llvm/IR/DataLayout.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace xb {

// Lowers the AST of one module into an in-memory LLVM module.
//
// Execution model (see docs/runtime.md):
//  * every value is an xb_item; params, LOCALs and temporaries live in an
//    item frame obtained from the runtime (GC roots, stable addresses);
//  * operators call the runtime (fast paths get inlined later);
//  * after any call that can fail, ctx->pending is tested and the function
//    unwinds to its exit block when it is set.
class CodeGen {
public:
  CodeGen(llvm::LLVMContext &ctx, llvm::Module &mod, Diagnostics &diag);
  bool run(const Module &m, bool emitEntry);

private:
  void initTypes();
  llvm::FunctionCallee rt(const char *name, llvm::Type *ret, std::initializer_list<llvm::Type *> params);
  llvm::Function *declareXbFunc(const std::string &xbName, bool internal);
  void addAbiAttributes(llvm::Function *f);
  llvm::StructType *strType(llvm::Type *data);
  static std::string mangle(const std::string &xbName) { return "xbf_" + xbName; }

  llvm::Constant *literalItem(uint32_t type, uint16_t width, uint16_t dec, llvm::Constant *payload);
  llvm::Constant *nilLiteral();
  llvm::Constant *logicalLiteral(bool v);
  llvm::Constant *numberLiteral(const NumberExpr &n);
  llvm::Constant *intLiteral(int64_t v);
  llvm::Constant *stringLiteral(const std::string &s);

  void genFunc(const FuncDecl &f);
  void genEntry(const FuncDecl &start);
  void genModuleInit(const Module &m);
  llvm::Value *staticAddress(uint32_t idx);
  llvm::Value *entryAlloca(llvm::Type *ty, const char *name);
  void genForEach(const ForEachStmt &st);
  bool isBoxed(const VarExpr &v) const;
  bool isEachVar(const VarExpr &v) const;
  llvm::Value *refTarget(llvm::Value *ref);
  llvm::Value *derefIfRef(llvm::Value *item);
  void genSwitch(const SwitchStmt &st);
  llvm::Value *slot(uint32_t idx);
  uint32_t allocTemps(uint32_t n);
  llvm::Value *tempSlot() { return slot(allocTemps(1)); }

  void copyItem(llvm::Value *dst, llvm::Value *src);
  void setNil(llvm::Value *dst);
  void storeLogical(llvm::Value *dst, llvm::Value *i1);
  void checkPending();
  void startDeadBlock();
  llvm::BasicBlock *newBlock(const char *name);

  void genBlock(const Block &b);
  void genStmt(const Stmt &s);

  void genExpr(const Expr &e, llvm::Value *dst);
  void genDiscard(const Expr &e);
  llvm::Value *genOperand(const Expr &e);
  std::pair<llvm::Value *, llvm::Value *> genOperands(const Expr &lhs, const Expr &rhs);
  llvm::Value *genCond(const Expr &e);
  llvm::Value *testTruth(llvm::Value *item);
  void genBinary(BinOp op, llvm::Value *dst, llvm::Value *a, llvm::Value *b);
  void genCall(const CallExpr &c, llvm::Value *dst);
  void genAssign(const AssignExpr &a, llvm::Value *dst);
  void genIncDec(const IncDecExpr &e, llvm::Value *dst);
  void genIIf(const IIfExpr &e, llvm::Value *dst);
  void genArrayLit(const ArrayLitExpr &a, llvm::Value *dst);
  void genIndexAssign(const IndexExpr &target, const AssignExpr *assign, const IncDecExpr *incdec, llvm::Value *dst);
  void genCodeblock(const BlockExpr &b, llvm::Value *dst);
  llvm::Function *genCodeblockFunction(const BlockExpr &b);
  void boxCaptured(const std::vector<bool> &boxed);
  void beginFrame(llvm::Value *argc, llvm::Value *argv, uint32_t nparams, uint32_t nlocals,
                  const std::vector<bool> &boxed);
  void endFrame();
  llvm::Value *varAddress(const Expr &e);
  llvm::Value *cellOfSlot(uint32_t slot);
  llvm::Value *capturedCellAddress(llvm::Value *block, uint32_t index);

  llvm::LLVMContext &ctx_;
  llvm::Module &mod_;
  const llvm::DataLayout &dl_;
  Diagnostics &diag_;
  llvm::IRBuilder<> b_;

  // types
  llvm::StructType *itemTy_ = nullptr;
  llvm::IntegerType *i8_, *i16_, *i32_, *i64_, *intPtrTy_;
  llvm::PointerType *ptrTy_;
  llvm::FunctionType *xbFuncTy_ = nullptr;
  llvm::FunctionType *blockFnTy_ = nullptr;
  llvm::StructType *blockTy_ = nullptr;     // xb_block: { fn, ncells, cells[] }
  uint64_t itemSize_ = 0;
  llvm::Align itemAlign_;
  uint64_t unionSize_ = 0;

  // runtime entry points
  llvm::FunctionCallee rtFramePush_, rtFramePop_, rtArgsCopy_, rtCondError_, rtForTest_, rtAbiCheck_;
  llvm::FunctionCallee rtNeg_, rtNot_, rtNumDefaultDec_, rtPending_;
  llvm::FunctionCallee rtArrayNew_, rtIndex_, rtIndexSet_, rtCellBox_, rtBlockNew_;
  llvm::FunctionCallee rtEnumLen_, rtEnumRefNew_, rtEnumRefMove_, rtRefTarget_, rtSwitchCheck_, rtSwitchMatch_;
  llvm::FunctionCallee rtModuleRegister_, rtModulesInit_;
  std::map<BinOp, llvm::FunctionCallee> rtBin_;

  // module state
  std::unordered_map<std::string, llvm::Function *> funcs_;
  llvm::GlobalVariable *statics_ = nullptr;   // the module's STATICs
  std::unordered_map<std::string, llvm::Constant *> strLits_;
  std::unordered_map<std::string, llvm::Constant *> numLits_;
  llvm::Constant *nilLit_ = nullptr;
  llvm::Constant *trueLit_ = nullptr, *falseLit_ = nullptr;

  // function state
  struct Loop {
    llvm::BasicBlock *exitTo;
    llvm::BasicBlock *loopTo;
  };
  llvm::Function *fn_ = nullptr;
  llvm::Value *fnCtx_ = nullptr, *fnRet_ = nullptr, *fnBase_ = nullptr;
  llvm::CallInst *fnPush_ = nullptr;
  llvm::BasicBlock *fnExit_ = nullptr;
  uint32_t numLocals_ = 0, temps_ = 0, maxSlots_ = 0;
  std::vector<Loop> loops_;
  struct EachVar {             // the variable of a FOR EACH loop being generated
    const VarExpr *var;
    llvm::Value *raw;          // its slot or STATIC storage, which holds the reference (or the cell)
    llvm::Value *saved;        // its value before the loop
  };
  std::vector<EachVar> eachVars_;
  const std::vector<bool> *fnBoxed_ = nullptr; // frame slots holding cells
  llvm::Value *fnSelf_ = nullptr;              // the codeblock, inside a block function
  unsigned blockCount_ = 0;

  // Everything above that describes the function being generated; saved
  // while a codeblock's function is generated in the middle of it.
  struct FnState {
    llvm::Function *fn;
    llvm::Value *ctx, *ret, *base, *self;
    llvm::CallInst *push;
    llvm::BasicBlock *exit;
    uint32_t numLocals, temps, maxSlots;
    std::vector<Loop> loops;
    std::vector<EachVar> eachVars;
    const std::vector<bool> *boxed;
    llvm::IRBuilderBase::InsertPoint ip;
  };
  FnState saveFn();
  void restoreFn(FnState s);
};

} // namespace xb
