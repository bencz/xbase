#pragma once

#include "codegen/FramePlan.h"
#include "common/Diagnostics.h"
#include "xir/IR.h"

#include <llvm/IR/DataLayout.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include <initializer_list>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace xb {

// Lowers an xIR module into an in-memory LLVM module (docs/xir.md, phase 1).
//
// Execution model (see docs/runtime.md):
//  * every item lives in an item frame obtained from the runtime (GC roots,
//    stable addresses); FramePlan decides which item goes where;
//  * operations call the runtime;
//  * after an operation that can fail, ctx->pending is tested and the
//    function leaves through the operation's unwind block (its exit).
class CodeGen {
public:
  CodeGen(llvm::LLVMContext &ctx, llvm::Module &mod, Diagnostics &diag);
  bool run(const xir::Module &m, bool emitEntry);

private:
  // Fields of xb_item, in the LLVM struct that mirrors it.
  enum ItemField : unsigned { FType = 0, FWidth = 1, FDec = 2, FValue = 3 };

  // CodeGen.cpp: types, runtime declarations, the module, functions and
  // their frames, item and basic block helpers.
  void initTypes();
  llvm::FunctionCallee rt(const char *name, llvm::Type *ret, std::initializer_list<llvm::Type *> params);
  void addAbiAttributes(llvm::Function *f);
  llvm::Function *declare(const xir::Function &f);
  llvm::Function *externFunction(const xir::Callee &c);
  void genFunction(const xir::Function &f);
  void genModuleRegistration(const xir::Module &m);
  void genEntry(const xir::Function &start);
  llvm::BasicBlock *newBlock(const char *name);
  void copyItem(llvm::Value *dst, llvm::Value *src);
  void setNil(llvm::Value *dst);
  void storeTagged(llvm::Value *dst, uint32_t tag, llvm::Value *v);
  void storeLogical(llvm::Value *dst, llvm::Value *bit);
  void checkPending(llvm::BasicBlock *unwind);

  // Literals.cpp
  llvm::StructType *strType(llvm::Type *data);
  llvm::Constant *literalItem(uint32_t type, uint16_t width, uint16_t dec, llvm::Constant *payload);
  llvm::Constant *literal(const xir::Constant &c);
  llvm::Constant *stringLiteral(const std::string &s);

  // Variables.cpp: where variables are, references, write barriers.
  llvm::Value *slot(uint32_t idx);
  llvm::Value *staticAddress(uint32_t idx);
  llvm::Value *capturedCellAddress(llvm::Value *block, uint32_t index);
  llvm::Value *capturedCell(const xir::Var &v);
  llvm::Value *rawAddress(const xir::Var &v);
  llvm::Value *varAddress(const xir::Var &v, xir::Access a, llvm::BasicBlock *unwind);
  llvm::Value *refTarget(llvm::Value *ref, llvm::BasicBlock *unwind);
  llvm::Value *derefIfRef(llvm::Value *item, llvm::BasicBlock *unwind, bool inCellLikely);
  llvm::BasicBlock *beginUnlikely(llvm::Value *cond, const char *name);
  void endUnlikely(llvm::BasicBlock *join);
  llvm::Value *holdsRef(llvm::Value *item);
  void refWritten(llvm::Value *item, bool inCellLikely);
  void cellWritten(llvm::Value *cell);
  void writeBarrier(const xir::Var &v, xir::Access a);
  void refOf(const xir::Var &v, llvm::Value *dst);

  // Instructions.cpp
  void emit(const xir::Instruction &i);
  void emitBranch(const xir::Target &t);
  llvm::Value *addr(const xir::Value *v);
  llvm::Value *dest(const xir::Instruction &i);
  llvm::Value *value(const xir::Value *v);
  llvm::Value *testTruth(llvm::Value *item, llvm::BasicBlock *unwind);
  llvm::Value *operandGroup(const xir::Instruction &i);
  llvm::BasicBlock *unwindOf(const xir::Instruction &i);

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
  llvm::FunctionType *initFnTy_ = nullptr;
  llvm::StructType *blockTy_ = nullptr;     // xb_block: { fn, ncells, cells[] }
  uint64_t itemSize_ = 0;
  llvm::Align itemAlign_;
  uint64_t unionSize_ = 0;

  // runtime entry points
  llvm::FunctionCallee rtFramePush_, rtFramePop_, rtArgsCopy_, rtCondError_, rtForTest_, rtAbiCheck_;
  llvm::FunctionCallee rtNeg_, rtNot_, rtNumDefaultDec_, rtPending_;
  llvm::FunctionCallee rtArrayNew_, rtIndex_, rtIndexSet_, rtCellOf_, rtBlockNew_;
  llvm::FunctionCallee rtEnumLen_, rtEachShared_, rtEachEnter_, rtEachMove_, rtEachLeave_, rtRefTarget_, rtSwitchCheck_;
  llvm::FunctionCallee rtSwitchMatch_;
  llvm::FunctionCallee rtModuleRegister_, rtModulesInit_, rtGcRemember_, rtRefWritten_, rtRefOf_, rtRefElem_;
  std::map<BinOp, llvm::FunctionCallee> rtBin_;

  // module state
  std::unordered_map<const xir::Function *, llvm::Function *> functions_;
  llvm::GlobalVariable *statics_ = nullptr;   // the module's STATICs
  std::unordered_map<std::string, llvm::Constant *> strLits_;
  std::unordered_map<std::string, llvm::Constant *> numLits_;
  llvm::Constant *nilLit_ = nullptr;
  llvm::Constant *trueLit_ = nullptr, *falseLit_ = nullptr;

  // the function being generated
  llvm::Function *fn_ = nullptr;
  llvm::Value *fnCtx_ = nullptr, *fnRet_ = nullptr, *fnBase_ = nullptr, *fnSelf_ = nullptr;
  llvm::BasicBlock *fnExit_ = nullptr;
  const FramePlan *plan_ = nullptr;
  std::unordered_map<const xir::BasicBlock *, llvm::BasicBlock *> blocks_;
  std::unordered_map<const xir::Value *, llvm::Value *> values_; // machine values; addresses of Place homes
};

} // namespace xb
