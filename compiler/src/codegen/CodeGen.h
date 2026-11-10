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
  // Modules of the runtime (xbc --runtime): public functions are runtime functions (xbf_NAME, in the runtime's
  // symbol table already, so not registered), and their frames keep line 0, as the runtime's C functions show.
  void setRuntimeLibrary(bool on) { runtimeLibrary_ = on; }
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
  enum FrameField : unsigned {
    FramePrev = 0,
    FrameProc = 1,
    FrameCount = 2,
    FrameLine = 3,
    FrameArgv = 4,
    FrameArgc = 5,
    FrameSlots = 6
  };
  enum CtxField : unsigned { CtxPending = 0, CtxExitCode = 1, CtxFrames = 2, CtxDecimals = 3 };
  enum ArrayField : unsigned { ArrayLen = 0, ArrayCap = 1, ArrayItems = 2 };
  enum StrField : unsigned { StrFlags = 0, StrHash = 1, StrLen = 2, StrData = 3 }; // xb_str
  // Classes of memory compiled code reaches, which never overlap (TBAA):
  // items outside the heap (frames, Local and scratch items, STATIC storage,
  // literals), items in the heap (elements, cells, reference targets), array
  // headers, codeblocks' cell pointers, the context's head, frame links, the
  // collector's state bytes, and strings (compiled code reads them, and
  // writes the strings an `append` grows: bytes, length, flags, room).
  // Unknown memory is not tagged.
  enum class Mem : uint8_t {
    Unknown, Item, HeapItem, ArrayHeader, BlockCells, Ctx, FrameLink, GcState, String, Count_
  };

  // CodeGen.cpp: types, runtime declarations, the module, functions and
  // their frames, item and basic block helpers.
  void initTypes();
  llvm::StructType *frameType(uint32_t nslots);
  void pushFrame(uint32_t nslots, uint32_t nparams, llvm::Value *argc, llvm::Value *argv, llvm::Constant *proc,
                 bool keepArgs);
  void directEntries(const xir::Module &m);
  void genThunk(const xir::Function &f);
  void cacheWords(const xir::Value *v);
  llvm::Value *argItem(llvm::Value *argc, llvm::Value *argv, uint32_t k);
  llvm::Constant *procOf(const xir::Function &f);
  void markLine(llvm::Instruction *i) const;
  void placeLines();
  // `prev`: the frame the chain has now, when known (else it is read from the context).
  void linkFrame(llvm::Value *frame, llvm::Value *nslots, llvm::Value *proc, llvm::Value *prev = nullptr);
  void unlinkFrame(llvm::Value *frame, llvm::Value *prev = nullptr);
  llvm::FunctionCallee rt(const char *name, llvm::Type *ret, std::initializer_list<llvm::Type *> params);
  void addAbiAttributes(llvm::Function *f);
  llvm::Function *declare(const xir::Function &f);
  llvm::Function *externFunction(const xir::Callee &c);
  void genFunction(const xir::Function &f);
  void genModuleRegistration(const xir::Module &m);
  void genRequests(const xir::Module &m);
  void genEntry(const xir::Function &start);
  llvm::BasicBlock *newBlock(const char *name);
  unsigned headerShift(ItemField f);
  unsigned valueShift(unsigned bits);
  llvm::Constant *headerWord(uint32_t tag, uint16_t width, uint16_t dec);
  llvm::Value *heapItem(llvm::Value *addr);
  Mem itemMemory(llvm::Value *addr);
  // The alias information of a load or store of memory class `m`.
  template <class Access> Access *tag(Access *access, Mem m) {
    if (llvm::MDNode *t = tbaa_[(size_t)m]) access->setMetadata(llvm::LLVMContext::MD_tbaa, t);
    return access;
  }
  llvm::LoadInst *loadFrom(llvm::Type *t, llvm::Value *addr, Mem m, const llvm::Twine &name = "");
  llvm::Value *loadHeader(llvm::Value *item);
  llvm::Value *itemType(llvm::Value *item);
  llvm::Value *typeOf(const xir::Value *v, llvm::Value *item);
  llvm::Value *payloadWord(llvm::Value *v);
  void storeWords(llvm::Value *dst, llvm::Value *header, llvm::Value *payload);
  struct Words {
    llvm::Value *header, *payload; // the two words of an item
  };
  Words loadWords(llvm::Value *item);
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
  llvm::Constant *messageName(const std::string &message);

  // Variables.cpp: where variables are, references, write barriers.
  llvm::Value *slot(uint32_t idx);
  llvm::Value *staticAddress(uint32_t idx);
  llvm::Value *capturedCellAddress(llvm::Value *block, uint32_t index);
  llvm::Value *capturedCell(const xir::Var &v);
  llvm::Value *rawAddress(const xir::Var &v);
  llvm::Value *varAddress(const xir::Var &v, xir::Access a, llvm::BasicBlock *unwind);
  llvm::Value *refTarget(llvm::Value *ref, llvm::BasicBlock *unwind);
  llvm::Value *derefIfRef(llvm::Value *item, llvm::BasicBlock *unwind);
  llvm::BasicBlock *beginUnlikely(llvm::Value *cond, const char *name);
  void endUnlikely(llvm::BasicBlock *join);
  llvm::Value *holdsRef(llvm::Value *item);
  void refWritten(llvm::Value *item);
  void cellWritten(llvm::Value *cell);
  void writeBarrier(const xir::Var &v, xir::Access a);
  void refOf(const xir::Var &v, llvm::Value *dst);

  // FastPaths.cpp: integer cases of operators inline
  llvm::Value *isInt(llvm::Value *item);
  llvm::Value *isIntType(llvm::Value *type);
  llvm::Value *intValue(llvm::Value *item);
  llvm::Value *itemPointer(llvm::Value *item, const char *name);
  llvm::Value *arrayLength(llvm::Value *array);
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
  llvm::Value *doubleRemainder(llvm::Value *x, llvm::Value *y);
  void emitForTest(const xir::Instruction &i);
  bool emitBitCall(const xir::Instruction &i);
  // InlineCalls.cpp: Len, Asc, Chr, SubStr( c, n, 1 ), Int, PCount and PValue( n ) in place.
  void findFusedCalls(const xir::BasicBlock &b);
  bool emitInlineCall(const xir::Instruction &i);
  llvm::Value *strLength(llvm::Value *str);
  llvm::Value *strByte(llvm::Value *str, llvm::Value *pos);
  llvm::Value *byteString(llvm::Value *c);
  void shareString(llvm::Value *item);
  llvm::Value *isGrowing(llvm::Value *flags);
  void emitAppend(const xir::Instruction &i);
  void findSharingLoads(const xir::Function &f, const xir::CFG &cfg);
  void shareOnEdge(const xir::BasicBlock *from, const xir::BasicBlock *to);
  void emitBitOp(xb_bit_op op, llvm::Value *x, llvm::Value *tx, llvm::Value *y, llvm::Value *ty, llvm::Value *d,
                 llvm::function_ref<void()> slow);
  void emitBitOperator(const xir::Instruction &i, const xb_bit_function &f);
  void emitNeg(const xir::Instruction &i);
  llvm::Value *numberAsInteger(llvm::Value *item, llvm::Value *type);
  llvm::Value *bitApply(xb_bit_op op, llvm::Value *x, llvm::Value *y);
  llvm::Value *inlineElement(const xir::Instruction &i, llvm::Value *base, llvm::Value *idx, llvm::BasicBlock *slow,
                             llvm::Value *&array, llvm::Value *&pos);
  void emitIndex(const xir::Instruction &i);
  void emitIndexSet(const xir::Instruction &i);

  // Instructions.cpp
  void emit(const xir::Instruction &i);
  void emitDirectCall(const xir::Instruction &i, llvm::Value *d);
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
  void withRest(const xir::Instruction &i, llvm::Value *group,
                llvm::function_ref<void(llvm::Value *argc, llvm::Value *argv)> emit);

  llvm::LLVMContext &ctx_;
  llvm::Module &mod_;
  const llvm::DataLayout &dl_;
  Diagnostics &diag_;
  // Every call it creates is marked with the source line of the xIR
  // instruction being lowered (lineKind_), for placeLines.
  llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderCallbackInserter> b_;
  unsigned lineKind_ = 0;

  llvm::MDNode *tbaa_[(size_t)Mem::Count_] = {}; // access tags, by class (none for Unknown)

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
  llvm::GlobalVariable *strBytes_ = nullptr; // xb_str_bytes, declared when first used
  uint64_t itemSize_ = 0;
  llvm::Align itemAlign_;
  uint64_t unionSize_ = 0;

  // runtime entry points
  llvm::FunctionCallee rtCondError_, rtForTest_, rtAbiCheck_;
  llvm::FunctionCallee rtNeg_, rtNot_, rtBitNot_, rtAppend_, rtNumDefaultDec_, rtPending_;
  llvm::FunctionCallee rtSend_, rtSeqCatch_, rtSeqValue_, rtHashNew_, rtEachKey_;
  llvm::FunctionCallee rtArgsRest_;
  llvm::FunctionCallee rtArrayNew_, rtIndex_, rtIndexSet_, rtWriteItem_, rtCellOf_, rtBlockNew_;
  llvm::FunctionCallee rtEnumLen_, rtEachShared_, rtEachEnter_, rtEachMove_, rtEachLeave_, rtRefTarget_, rtSwitchCheck_;
  llvm::FunctionCallee rtSwitchMatch_;
  llvm::FunctionCallee rtModuleRegister_, rtProgramRun_, rtGcRemember_, rtRefWritten_, rtRefOf_, rtRefElem_;
  std::map<BinOp, llvm::FunctionCallee> rtBin_;

  // state of the object (every module added)
  std::unordered_map<const xir::Function *, llvm::Function *> functions_;
  // Functions with a direct entry (directEntries): their body, which takes the parameters as items in registers
  // (header and value word of each); functions_ holds their dynamic entry, a thunk (genThunk).
  std::unordered_map<const xir::Function *, llvm::Function *> direct_;

  llvm::GlobalVariable *statics_ = nullptr;   // the STATICs of the module being added
  llvm::Constant *file_ = nullptr;            // its source file name (xb_proc.file), or null
  std::unordered_map<std::string, llvm::Constant *> strLits_;
  std::unordered_map<std::string, llvm::Constant *> numLits_;
  std::unordered_map<std::string, llvm::Constant *> messages_;
  llvm::Constant *nilLit_ = nullptr;
  llvm::Constant *trueLit_ = nullptr, *falseLit_ = nullptr;

  // the function being generated
  llvm::Function *fn_ = nullptr;
  llvm::Value *fnCtx_ = nullptr, *fnRet_ = nullptr, *fnBase_ = nullptr, *fnSelf_ = nullptr, *fnFrame_ = nullptr;
  uint32_t fnParams_ = 0; // the function's parameters: the arguments past them are what `...` passes on
  // `...` passes its arguments on in place (no copy): the function's one args.rest, outside loops, in a call that
  // takes references, in a function that reads its arguments nowhere else (PCount(), PValue()).
  bool forwardInPlace_ = false;
  bool fnDirect_ = false; // the function being generated is a direct body: no argc and argv
  // A direct body's caller's frame (its argument): linking and unlinking the frame read no memory, which would make
  // every call wait for the store of the call before (the chain of the context's frames). Null otherwise.
  llvm::Value *fnPrev_ = nullptr;
  bool runtimeLibrary_ = false;
  llvm::BasicBlock *fnExit_ = nullptr;
  llvm::BasicBlock *unwind_ = nullptr; // where an operation of the current block that fails goes (xir unwind)
  uint32_t line_ = 0;                  // the source line of the xIR instruction being lowered (0: none)
  std::unordered_set<const llvm::Value *> quietCalls_; // runtime functions that never show the call stack
  const FramePlan *plan_ = nullptr;
  std::unordered_map<const xir::BasicBlock *, llvm::BasicBlock *> blocks_;
  std::unordered_map<const xir::Value *, llvm::Value *> values_; // machine values; addresses of Place homes
  std::unordered_map<const xir::Instruction *, llvm::Value *> fusedTruth_; // truth tests a comparison answered
  std::unordered_map<const xir::Value *, llvm::AllocaInst *> locals_;       // Local homes
  std::unordered_set<const llvm::Value *> localItems_;                      // their addresses
  std::unordered_set<const llvm::Value *> heapItems_;                       // addresses of items in the heap
  std::vector<llvm::AllocaInst *> scratch_;                                 // runtime call operands
  std::unordered_set<const xir::Instruction *> sharingLoads_;              // loads that end an `append`'s growth
  std::map<std::pair<const xir::BasicBlock *, const xir::BasicBlock *>, std::vector<const xir::Var *>>
      shareOnEdge_; // and the edges (from, to) that do it instead of the loads after them
  std::unordered_set<const xir::Instruction *> fusedCalls_;                // Asc() calls lowered with their SubStr()
  std::unordered_set<const xir::Instruction *> fusedComparisons_;          // in the block being lowered
  // The words of the values held in frame slots, read once right after their definition (cacheWords): a slot holds
  // its value unchanged while the value lives, so its header (which the collector never changes) and its integer
  // payload are served from these instead of the frame, which LLVM must reload after every call (the frame is
  // linked into the context). Pointers are still read from the slot. `wordsAt_` maps the addresses addr() returned
  // for them: slot() makes a new address at every call, so an address stands for one read of one value.
  std::unordered_map<const xir::Value *, Words> words_;
  std::unordered_map<const llvm::Value *, Words> wordsAt_;
};

} // namespace xb
