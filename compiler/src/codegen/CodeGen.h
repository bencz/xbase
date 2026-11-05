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
#include <unordered_set>
#include <utility>
#include <vector>

namespace xb {

// Lowers an xIR module into an in-memory LLVM module (docs/xir.md, phase 1).
//
// Execution model (see docs/runtime.md):
//  * every item lives in the function's item frame, an alloca linked into
//    the context's chain (GC roots); FramePlan decides which item goes where;
//  * operations call the runtime;
//  * after an operation that can fail, ctx->pending is tested and the
//    function leaves through the operation's unwind block (its exit).
class CodeGen {
public:
  CodeGen(llvm::LLVMContext &ctx, llvm::Module &mod, Diagnostics &diag);
  // Adds one module of the program to the LLVM module; several modules may
  // share it (one object for several sources), each adding its own functions,
  // STATICs and registration.
  void run(const xir::Module &m);
  // Then: the program entry calling `start` (none without one), and the check
  // of the LLVM module.
  bool finish(const xir::Function *start);

private:
  // Fields of xb_item, xb_frame and the head of xb_ctx, in the LLVM structs
  // that mirror them.
  enum ItemField : unsigned { FType = 0, FWidth = 1, FDec = 2, FValue = 3 };
  enum FrameField : unsigned { FramePrev = 0, FrameCount = 1, FrameSlots = 2 };
  enum CtxField : unsigned { CtxPending = 0, CtxExitCode = 1, CtxFrames = 2, CtxDecimals = 3 };
  enum ArrayField : unsigned { ArrayLen = 0, ArrayCap = 1, ArrayItems = 2 };

  // CodeGen.cpp: types, runtime declarations, the module, functions and
  // their frames, item and basic block helpers.
  void initTypes();
  llvm::StructType *frameType(uint32_t nslots);
  void pushFrame(uint32_t nslots, uint32_t nparams, llvm::Value *argc, llvm::Value *argv);
  void popFrame();
  llvm::FunctionCallee rt(const char *name, llvm::Type *ret, std::initializer_list<llvm::Type *> params);
  void addAbiAttributes(llvm::Function *f);
  llvm::Function *declare(const xir::Function &f);
  llvm::Function *externFunction(const xir::Callee &c);
  void genFunction(const xir::Function &f);
  void genModuleRegistration(const xir::Module &m);
  void genEntry(const xir::Function &start);
  llvm::BasicBlock *newBlock(const char *name);
  unsigned headerShift(ItemField f);
  unsigned valueShift(unsigned bits);
  llvm::Constant *headerWord(uint32_t tag, uint16_t width, uint16_t dec);
  llvm::Value *loadHeader(llvm::Value *item);
  llvm::Value *itemType(llvm::Value *item);
  llvm::Value *typeOf(const xir::Value *v, llvm::Value *item);
  llvm::Value *payloadWord(llvm::Value *v);
  void storeWords(llvm::Value *dst, llvm::Value *header, llvm::Value *payload);
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

  // FastPaths.cpp: integer cases of operators inline
  llvm::Value *isInt(llvm::Value *item);
  llvm::Value *intValue(llvm::Value *item);
  void storeInt(llvm::Value *dst, llvm::Value *v);
  void storeDouble(llvm::Value *dst, llvm::Value *v, llvm::Value *dec);
  llvm::Value *isNumberType(llvm::Value *type);
  llvm::Value *bothInts(llvm::Value *ta, llvm::Value *tb);
  llvm::Value *exactNumbers(llvm::Value *a, llvm::Value *ta, llvm::Value *b, llvm::Value *tb);
  llvm::Value *exactAsDouble(llvm::Value *item, llvm::Value *type);
  llvm::Value *numberDec(llvm::Value *item, llvm::Value *type);
  llvm::Value *numberAsDouble(llvm::Value *item, llvm::Value *type);
  void branchLikely(llvm::Value *cond, llvm::BasicBlock *fast, llvm::BasicBlock *slow);
  void findFusedComparisons(const xir::BasicBlock &b);
  static llvm::CmpInst::Predicate llvmPredicate(xir::IntPred p);
  void emitBinary(const xir::Instruction &i);
  void emitDivMod(const xir::Instruction &i, llvm::Value *a, llvm::Value *b, llvm::Value *d);
  void emitForTest(const xir::Instruction &i);
  bool emitBitCall(const xir::Instruction &i);
  llvm::Value *inlineElement(const xir::Instruction &i, llvm::Value *base, llvm::Value *idx, llvm::BasicBlock *slow,
                             llvm::Value *&array, llvm::Value *&pos);
  void emitIndex(const xir::Instruction &i);
  void emitIndexSet(const xir::Instruction &i);

  // Instructions.cpp
  void emit(const xir::Instruction &i);
  void emitBranch(const xir::Target &t);
  llvm::Value *addr(const xir::Value *v);
  llvm::AllocaInst *entryAlloca(const char *name);
  llvm::Value *scratchItem(unsigned k);
  llvm::Value *callArg(llvm::Value *item, unsigned k);
  struct CallResult {
    llvm::Value *arg;   // what the call writes
    llvm::Value *local; // the Local home it is copied to after the call, or null
  };
  CallResult callResult(llvm::Value *item, unsigned k);
  void callDone(const CallResult &r);
  void callItemOp(llvm::FunctionCallee fn, llvm::Value *result, std::initializer_list<llvm::Value *> operands);
  llvm::Value *dest(const xir::Instruction &i);
  llvm::Value *value(const xir::Value *v);
  llvm::Value *testTruth(llvm::Value *item, llvm::BasicBlock *unwind, llvm::Value *type);
  llvm::Value *logicalValue(llvm::Value *item);
  llvm::Value *operandGroup(const xir::Instruction &i);

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
  llvm::StructType *arrayTy_ = nullptr;     // xb_array: { len, cap, items } (its items follow)
  llvm::StructType *ctxHeadTy_ = nullptr;   // the first fields of xb_ctx: { pending, exitcode, frames, decimals }
  llvm::GlobalVariable *nilItem_ = nullptr; // a NIL item: missing arguments
  uint64_t itemSize_ = 0;
  llvm::Align itemAlign_;
  uint64_t unionSize_ = 0;

  // runtime entry points
  llvm::FunctionCallee rtCondError_, rtForTest_, rtAbiCheck_;
  llvm::FunctionCallee rtNeg_, rtNot_, rtNumDefaultDec_, rtPending_;
  llvm::FunctionCallee rtArrayNew_, rtIndex_, rtIndexSet_, rtWriteItem_, rtCellOf_, rtBlockNew_;
  llvm::FunctionCallee rtEnumLen_, rtEachShared_, rtEachEnter_, rtEachMove_, rtEachLeave_, rtRefTarget_, rtSwitchCheck_;
  llvm::FunctionCallee rtSwitchMatch_;
  llvm::FunctionCallee rtModuleRegister_, rtModulesInit_, rtGcRemember_, rtRefWritten_, rtRefOf_, rtRefElem_;
  std::map<BinOp, llvm::FunctionCallee> rtBin_;

  // state of the object (every module added)
  std::unordered_map<const xir::Function *, llvm::Function *> functions_;
  llvm::GlobalVariable *statics_ = nullptr;   // the STATICs of the module being added
  std::unordered_map<std::string, llvm::Constant *> strLits_;
  std::unordered_map<std::string, llvm::Constant *> numLits_;
  llvm::Constant *nilLit_ = nullptr;
  llvm::Constant *trueLit_ = nullptr, *falseLit_ = nullptr;

  // the function being generated
  llvm::Function *fn_ = nullptr;
  llvm::Value *fnCtx_ = nullptr, *fnRet_ = nullptr, *fnBase_ = nullptr, *fnSelf_ = nullptr, *fnFrame_ = nullptr;
  llvm::BasicBlock *fnExit_ = nullptr;
  const FramePlan *plan_ = nullptr;
  std::unordered_map<const xir::BasicBlock *, llvm::BasicBlock *> blocks_;
  std::unordered_map<const xir::Value *, llvm::Value *> values_; // machine values; addresses of Place homes
  std::unordered_map<const xir::Instruction *, llvm::Value *> fusedTruth_; // truth tests a comparison answered
  std::unordered_map<const xir::Value *, llvm::AllocaInst *> locals_;       // Local homes
  std::unordered_set<const llvm::Value *> localItems_;                      // their addresses
  std::vector<llvm::AllocaInst *> scratch_;                                 // runtime call operands
  std::unordered_set<const xir::Instruction *> fusedComparisons_;          // in the block being lowered
};

} // namespace xb
