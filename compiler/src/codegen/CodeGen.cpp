// Types, runtime declarations, the module, functions and their frames, item and basic block helpers.
#include "codegen/CodeGen.h"

#include "xbrt.h" // constants only (type tags, ABI version) - never layouts

#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/MDBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace xb {

CodeGen::CodeGen(LLVMContext &ctx, llvm::Module &mod, Diagnostics &diag)
    : ctx_(ctx), mod_(mod), dl_(mod.getDataLayout()), diag_(diag),
      b_(ctx, ConstantFolder(), IRBuilderCallbackInserter([this](Instruction *i) { markLine(i); })),
      lineKind_(ctx.getMDKindID("xb.line")) {
  initTypes();
}

// The runtime structs, described for the *target* DataLayout. Sizes and
// offsets come out exactly as the C compiler lays them out for that target,
// whatever its word size or endianness.
void CodeGen::initTypes() {
  i8_ = Type::getInt8Ty(ctx_);
  i16_ = Type::getInt16Ty(ctx_);
  i32_ = Type::getInt32Ty(ctx_);
  i64_ = Type::getInt64Ty(ctx_);
  ptrTy_ = PointerType::getUnqual(ctx_);
  intPtrTy_ = dl_.getIntPtrType(ctx_);

  // union { int32 l; int64 i; double d; xb_str *s; void *p; } - represented
  // by its most aligned member; all members start at offset 0, so stores of
  // any member type at the field address match C on both endiannesses.
  Type *members[] = {i64_, Type::getDoubleTy(ctx_), ptrTy_};
  Type *rep = i64_;
  for (Type *m : members) {
    unionSize_ = std::max<uint64_t>(unionSize_, dl_.getTypeAllocSize(m));
    if (dl_.getABITypeAlign(m) > dl_.getABITypeAlign(rep)) rep = m;
  }
  if (dl_.getTypeAllocSize(rep) != unionSize_)
    diag_.error({}, "unsupported target: pointer wider than 64 bits");

  itemTy_ = StructType::create(ctx_, {i32_, i16_, i16_, rep}, "xb_item");
  itemSize_ = dl_.getTypeAllocSize(itemTy_);
  itemAlign_ = dl_.getABITypeAlign(itemTy_);

  Type *voidTy = Type::getVoidTy(ctx_);
  xbFuncTy_ = FunctionType::get(voidTy, {ptrTy_, ptrTy_, i32_, ptrTy_}, false);
  blockFnTy_ = FunctionType::get(voidTy, {ptrTy_, ptrTy_, i32_, ptrTy_, ptrTy_}, false);
  initFnTy_ = FunctionType::get(voidTy, {ptrTy_}, false);
  blockTy_ = StructType::create(ctx_, {ptrTy_, i32_, ArrayType::get(ptrTy_, 0)}, "xb_block");
  arrayTy_ = StructType::create(ctx_, {intPtrTy_, intPtrTy_, ptrTy_}, "xb_array"); // size_t is pointer-sized
  MDBuilder md(ctx_);
  MDNode *root = md.createTBAARoot("xbase");
  static const std::pair<Mem, const char *> kMemNames[] = {
      {Mem::Item, "item"},           {Mem::HeapItem, "heap item"}, {Mem::ArrayHeader, "array header"},
      {Mem::BlockCells, "block cells"}, {Mem::Ctx, "context"},     {Mem::FrameLink, "frame link"},
      {Mem::GcState, "gc state"},       {Mem::String, "string"},
  };
  static_assert(std::size(kMemNames) + 1 == (size_t)Mem::Count_, "one name per memory class but Unknown");
  for (auto [m, name] : kMemNames) {
    MDNode *type = md.createTBAAScalarTypeNode(name, root);
    tbaa_[(size_t)m] = md.createTBAAStructTagNode(type, type, 0);
  }
  ctxHeadTy_ = StructType::create(ctx_, {i32_, i32_, ptrTy_, i32_}, "xb_ctx_head"); // the [ABI] fields of xb_ctx
  nilItem_ = new GlobalVariable(mod_, itemTy_, true, GlobalValue::InternalLinkage, Constant::getNullValue(itemTy_),
                                "xb.nil");
  nilItem_->setAlignment(itemAlign_);
  rtCondError_ = rt("xb_cond_error", i32_, {ptrTy_, ptrTy_});
  rtForTest_ = rt("xb_for_test", i32_, {ptrTy_, ptrTy_, ptrTy_, ptrTy_});
  rtAbiCheck_ = rt("xb_abi_check", voidTy, {i32_, i32_, i32_, i32_, i32_, i32_, i32_, i32_});
  rtNeg_ = rt("xb_op_neg", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtNot_ = rt("xb_op_not", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtBitNot_ = rt("xb_op_bitnot", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtAppend_ = rt("xb_op_append", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtNumDefaultDec_ = rt("xb_num_default_dec", voidTy, {ptrTy_, ptrTy_, Type::getDoubleTy(ctx_)});
  rtPending_ = rt("xb_pending", i32_, {ptrTy_});
  rtSend_ = rt("xb_op_send", voidTy, {ptrTy_, ptrTy_, ptrTy_, i32_, ptrTy_});
  rtSeqCatch_ = rt("xb_seq_catch", i32_, {ptrTy_});
  rtSeqValue_ = rt("xb_seq_value", voidTy, {ptrTy_, ptrTy_});
  rtHashNew_ = rt("xb_hash_new", voidTy, {ptrTy_, ptrTy_, i32_, ptrTy_});
  rtEachKey_ = rt("xb_each_key", voidTy, {ptrTy_, ptrTy_, ptrTy_, i64_});
  rtArrayNew_ = rt("xb_array_new", voidTy, {ptrTy_, ptrTy_, i32_, ptrTy_});
  rtArgsRest_ = rt("xb_args_rest", i32_, {ptrTy_, ptrTy_, ptrTy_, i32_, i32_, i32_});
  rtIndex_ = rt("xb_op_index", voidTy, {ptrTy_, ptrTy_, ptrTy_, ptrTy_});
  rtIndexSet_ = rt("xb_op_index_set", voidTy, {ptrTy_, ptrTy_, ptrTy_, ptrTy_});
  rtWriteItem_ = rt("xb_gc_write_item", voidTy, {ptrTy_, intPtrTy_});
  rtCellOf_ = rt("xb_cell_of", ptrTy_, {ptrTy_, ptrTy_});
  rtBlockNew_ = rt("xb_block_new", ptrTy_, {ptrTy_, ptrTy_, ptrTy_, i32_});
  rtEnumLen_ = rt("xb_enum_len", i64_, {ptrTy_, ptrTy_});
  rtEachShared_ = rt("xb_each_shared", i32_, {ptrTy_, ptrTy_});
  rtEachEnter_ = rt("xb_each_enter", voidTy, {ptrTy_, ptrTy_, ptrTy_, i32_, ptrTy_});
  rtEachMove_ = rt("xb_each_move", voidTy, {ptrTy_, ptrTy_, i32_, i64_});
  rtEachLeave_ = rt("xb_each_leave", voidTy, {ptrTy_, i32_, ptrTy_});
  rtRefTarget_ = rt("xb_ref_target", ptrTy_, {ptrTy_, ptrTy_});
  rtRefWritten_ = rt("xb_ref_written", voidTy, {ptrTy_});
  rtRefOf_ = rt("xb_ref_of", voidTy, {ptrTy_, ptrTy_, ptrTy_, i32_});
  rtRefElem_ = rt("xb_ref_elem", voidTy, {ptrTy_, ptrTy_, ptrTy_, ptrTy_});
  rtGcRemember_ = rt("xb_gc_remember", voidTy, {ptrTy_});
  rtSwitchCheck_ = rt("xb_switch_check", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtSwitchMatch_ = rt("xb_switch_match", i32_, {ptrTy_, ptrTy_});
  rtModuleRegister_ = rt("xb_module_register", voidTy, {ptrTy_, ptrTy_, i32_, ptrTy_, i32_, i32_, ptrTy_, i32_});
  rtProgramRun_ = rt("xb_program_run", voidTy, {ptrTy_, ptrTy_, i32_, ptrTy_});
  for (FunctionCallee c : {rtPending_, rtWriteItem_, rtGcRemember_, rtRefWritten_, rtCellOf_, rtBlockNew_, rtArrayNew_,
                           rtEachShared_, rtEachEnter_, rtEachLeave_, rtRefOf_, rtNumDefaultDec_, rtSwitchMatch_,
                           rtSeqCatch_, rtSeqValue_, rtAbiCheck_, rtModuleRegister_})
    quietCalls_.insert(c.getCallee());
  static const std::pair<BinOp, const char *> bins[] = {
      {BinOp::Add, "xb_op_add"}, {BinOp::Sub, "xb_op_sub"},       {BinOp::Mul, "xb_op_mul"},
      {BinOp::Div, "xb_op_div"}, {BinOp::Mod, "xb_op_mod"},       {BinOp::Pow, "xb_op_pow"},
      {BinOp::Eq, "xb_op_eq"},   {BinOp::ExactEq, "xb_op_exacteq"}, {BinOp::NotEq, "xb_op_ne"},
      {BinOp::Lt, "xb_op_lt"},   {BinOp::Le, "xb_op_le"},         {BinOp::Gt, "xb_op_gt"},
      {BinOp::Ge, "xb_op_ge"},   {BinOp::In, "xb_op_in"},         {BinOp::BitAnd, "xb_op_bitand"},
      {BinOp::BitOr, "xb_op_bitor"}, {BinOp::BitXor, "xb_op_bitxor"}, {BinOp::Shl, "xb_op_shl"},
      {BinOp::Shr, "xb_op_shr"},
  };
  for (auto &[op, name] : bins) rtBin_[op] = rt(name, voidTy, {ptrTy_, ptrTy_, ptrTy_, ptrTy_});
}

// Some ABIs (s390x, PowerPC 64, RISC-V, LoongArch, ...) require 32-bit
// arguments and results to be extended to the register width by the side the
// C ABI names. The runtime contract makes every 32-bit parameter uint32_t and
// every 32-bit result int32_t (see xbrt.h), so the extension kinds are fixed.
void CodeGen::addAbiAttributes(Function *f) {
  const Triple &triple = mod_.getTargetTriple();
  Attribute::AttrKind param = TargetLibraryInfo::getExtAttrForI32Param(triple, /*Signed=*/false);
  Attribute::AttrKind result = TargetLibraryInfo::getExtAttrForI32Return(triple, /*Signed=*/true);
  for (Argument &a : f->args())
    if (a.getType() == i32_ && param != Attribute::None) a.addAttr(param);
  if (f->getReturnType() == i32_ && result != Attribute::None) f->addRetAttr(result);
  f->addFnAttr(Attribute::NoUnwind);
}

FunctionCallee CodeGen::rt(const char *name, Type *ret, std::initializer_list<Type *> params) {
  FunctionCallee c = mod_.getOrInsertFunction(name, FunctionType::get(ret, params, false));
  if (auto *f = dyn_cast<Function>(c.getCallee())) addAbiAttributes(f);
  return c;
}

// The LLVM function of an xIR function: public program functions are
// xbp_NAME (xbf_NAME in a module of the runtime; see the symbol prefixes in
// xbrt.h), declared already when another module of the object called it
// first; STATIC functions, codeblocks and the module initialization are
// internal, with names no other module looks up.
Function *CodeGen::declare(const xir::Function &f) {
  bool program = f.kind == xir::Function::Kind::Program;
  if (program && !f.internal) {
    std::string sym = (runtimeLibrary_ ? XB_RUNTIME_FUNC_PREFIX : XB_PROGRAM_FUNC_PREFIX) + f.name;
    if (Function *fn = mod_.getFunction(sym)) {
      if (!fn->isDeclaration()) diag_.error({}, "internal compiler error: function " + f.name + " defined twice");
      return fn;
    }
    Function *fn = Function::Create(xbFuncTy_, GlobalValue::ExternalLinkage, sym, mod_);
    addAbiAttributes(fn);
    return fn;
  }
  FunctionType *ty = program ? xbFuncTy_ : f.kind == xir::Function::Kind::Block ? blockFnTy_ : initFnTy_;
  Function *fn = Function::Create(ty, GlobalValue::InternalLinkage, "xb." + f.name, mod_);
  addAbiAttributes(fn);
  return fn;
}

// A function of the runtime (NATIVE) or of another module of the program.
Function *CodeGen::externFunction(const xir::Callee &c) {
  std::string sym = (c.native ? XB_RUNTIME_FUNC_PREFIX : XB_PROGRAM_FUNC_PREFIX) + c.name;
  Function *f = mod_.getFunction(sym);
  if (!f) {
    f = Function::Create(xbFuncTy_, GlobalValue::ExternalLinkage, sym, mod_);
    addAbiAttributes(f);
  }
  return f;
}

void CodeGen::run(const xir::Module &m) {
  statics_ = nullptr;
  if (!m.statics.empty()) {
    auto *ty = ArrayType::get(itemTy_, m.statics.size());
    auto *gv = new GlobalVariable(mod_, ty, false, GlobalValue::InternalLinkage, ConstantAggregateZero::get(ty),
                                  "xb.statics"); // all NIL
    gv->setAlignment(itemAlign_);
    statics_ = gv;
  }
  for (auto &f : m.functions) functions_[f.get()] = declare(*f);
  for (auto &f : m.functions) genFunction(*f);
  genModuleRegistration(m); // its public functions go into the symbol table
}

bool CodeGen::finish(const xir::Function *start) {
  if (start) genEntry(*start);
  std::string err;
  raw_string_ostream os(err);
  if (verifyModule(mod_, &os)) {
    diag_.error({}, "internal compiler error: invalid LLVM module:\n" + os.str());
    return false;
  }
  return diag_.errorCount() == 0;
}

// Prologue: the item frame (sized by the frame plan), the parameters copied
// into it; the body; the exit block, where every path ends, unlinks the frame.
void CodeGen::genFunction(const xir::Function &f) {
  fn_ = functions_.at(&f);
  fn_->setUWTableKind(UWTableKind::Async); // backtraces / profilers
  FramePlan plan(f);
  plan_ = &plan;
  blocks_.clear();
  values_.clear();
  fusedTruth_.clear();
  findSharingLoads(f, plan.cfg());
  locals_.clear();
  localItems_.clear();
  heapItems_.clear();
  scratch_.clear();

  BasicBlock *entry = BasicBlock::Create(ctx_, "entry", fn_);
  b_.SetInsertPoint(entry);
  fnCtx_ = fn_->getArg(0);
  fnCtx_->setName("ctx");
  fnRet_ = nullptr;
  fnSelf_ = nullptr;
  fnParams_ = f.numParams;
  size_t rests = 0;
  const xir::BasicBlock *restBlock = nullptr;
  for (auto &b : f.blocks)
    for (auto &inst : b->insts)
      if (inst->op == xir::Op::ArgsRest) {
        ++rests;
        restBlock = b.get();
      }
  // Once per call: in a loop, a callee's change (PValue()) would reach the next pass.
  forwardInPlace_ = rests == 1 && !f.readsArgs && !xir::loopDepths(plan.cfg()).count(restBlock);
  Value *argc = nullptr, *argv = nullptr;
  if (f.kind != xir::Function::Kind::ModuleInit) {
    fnRet_ = fn_->getArg(1);
    argc = fn_->getArg(2);
    argv = fn_->getArg(3);
    fnRet_->setName("ret");
    argc->setName("argc");
    argv->setName("argv");
  }
  if (f.kind == xir::Function::Kind::Block) {
    fnSelf_ = fn_->getArg(4);
    fnSelf_->setName("self");
  }
  pushFrame(plan.frameSize(), f.numParams, argc, argv, procOf(f), f.readsArgs && argc);
  fnExit_ = BasicBlock::Create(ctx_, "exit", fn_);

  // Blocks in reverse postorder; machine-value block parameters become PHIs.
  for (xir::BasicBlock *xb : plan.cfg().order()) {
    BasicBlock *bb = BasicBlock::Create(ctx_, xb->name, fn_, fnExit_);
    blocks_[xb] = bb;
    b_.SetInsertPoint(bb);
    for (auto &p : xb->params)
      if (p->type() != xir::Type::Item)
        values_[p.get()] = b_.CreatePHI(p->type() == xir::Type::Bool ? b_.getInt1Ty() : (Type *)i64_, 2);
  }
  b_.SetInsertPoint(entry);
  b_.CreateBr(blocks_.at(f.entry()));
  for (xir::BasicBlock *xb : plan.cfg().order()) {
    b_.SetInsertPoint(blocks_.at(xb));
    unwind_ = xb->unwind ? blocks_.at(xb->unwind) : fnExit_;
    findFusedComparisons(*xb);
    findFusedCalls(*xb);
    for (auto &i : xb->insts) {
      line_ = i->loc.line;
      emit(*i);
    }
  }
  line_ = 0;
  b_.SetInsertPoint(fnExit_);
  unlinkFrame(fnFrame_);
  b_.CreateRetVoid();
  placeLines();
  plan_ = nullptr;
}

// The frame of a compiled function (xb_frame in xbrt.h): an alloca linked into
// the context's chain. Parameters come from argv (NIL past argc), the other
// slots start as NIL; all of them are valid before anything can allocate.
StructType *CodeGen::frameType(uint32_t nslots) {
  return StructType::get(ctx_, {ptrTy_, ptrTy_, i32_, i32_, ptrTy_, i32_, ArrayType::get(itemTy_, nslots)});
}

// The function's xb_proc, or null when the call stack does not show it.
Constant *CodeGen::procOf(const xir::Function &f) {
  if (f.stackName.empty()) return ConstantPointerNull::get(ptrTy_);
  Constant *name = b_.CreateGlobalString(f.stackName, ".xb.procname", 0, &mod_);
  auto *ty = StructType::get(ctx_, {ptrTy_, i32_, i32_});
  Constant *nparams = ConstantInt::get(i32_, f.numParams);
  Constant *flags = ConstantInt::get(i32_, (f.readsArgs ? XB_PROC_ARGS : 0) | (f.hidden ? XB_PROC_HIDDEN : 0));
  auto *proc = new GlobalVariable(mod_, ty, true, GlobalValue::PrivateLinkage,
                                  ConstantStruct::get(ty, {name, nparams, flags}), ".xb.proc");
  proc->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
  return proc;
}

void CodeGen::markLine(Instruction *i) const {
  if (line_ && !runtimeLibrary_ && isa<CallInst>(i))
    i->setMetadata(lineKind_, MDNode::get(ctx_, ConstantAsMetadata::get(ConstantInt::get(i32_, line_))));
}

// The frame's line, stored right before each call that can see the call stack
// (a function of the program, a send, a runtime function or operator, which
// may raise an error): only there can it be read, so the fast paths of
// operations store nothing, and their slow paths, which are cold, do. A block
// stores a line once until another one is needed.
void CodeGen::placeLines() {
  for (BasicBlock &bb : *fn_) {
    uint32_t stored = 0;
    for (Instruction &inst : bb) {
      auto *call = dyn_cast<CallInst>(&inst);
      if (!call) continue;
      MDNode *md = call->getMetadata(lineKind_);
      if (!md) continue;
      call->setMetadata(lineKind_, nullptr);
      const Function *callee = call->getCalledFunction();
      if (callee && (callee->isIntrinsic() || quietCalls_.count(callee))) continue;
      auto line = (uint32_t)mdconst::extract<ConstantInt>(md->getOperand(0))->getZExtValue();
      if (line == stored) continue;
      Value *at = GetElementPtrInst::CreateInBounds(frameType(0), fnFrame_,
                                                    {ConstantInt::get(i32_, 0), ConstantInt::get(i32_, FrameLine)},
                                                    "line", call->getIterator());
      auto *store = new StoreInst(ConstantInt::get(i32_, line), at, call->getIterator());
      tag(store, Mem::FrameLink);
      stored = line;
    }
  }
}

void CodeGen::pushFrame(uint32_t nslots, uint32_t nparams, Value *argc, Value *argv, Constant *proc, bool keepArgs) {
  StructType *ty = frameType(nslots);
  BasicBlock &entry = fn_->getEntryBlock(); // a static alloca, whatever block the frame is linked in
  AllocaInst *frame = IRBuilder<>(&entry, entry.getFirstInsertionPt()).CreateAlloca(ty, nullptr, "frame");
  fnBase_ = b_.CreateConstInBoundsGEP2_32(ty, frame, 0, FrameSlots, "slots");
  for (uint32_t k = 0; k < nparams; ++k) {
    Value *given = b_.CreateICmpULT(ConstantInt::get(i32_, k), argc);
    Value *src = b_.CreateSelect(given, b_.CreateConstInBoundsGEP1_32(itemTy_, argv, k), nilItem_);
    copyItem(slot(k), src);
  }
  if (nslots > nparams)
    b_.CreateMemSet(slot(nparams), b_.getInt8(0), (uint64_t)(nslots - nparams) * itemSize_, itemAlign_);
  if (keepArgs) { // only what PCount() and PValue() read; other frames leave the two fields unset
    tag(b_.CreateStore(argv, b_.CreateStructGEP(ty, frame, FrameArgv)), Mem::FrameLink);
    tag(b_.CreateStore(argc, b_.CreateStructGEP(ty, frame, FrameArgc)), Mem::FrameLink);
  }
  linkFrame(frame, ConstantInt::get(i32_, nslots), proc);
  fnFrame_ = frame;
}

// The header of a frame whose slots are valid (`nslots`, `proc`, line 0), and
// the frame linked into the context's chain.
void CodeGen::linkFrame(Value *frame, Value *nslots, Value *proc) {
  StructType *ty = frameType(0);
  tag(b_.CreateStore(nslots, b_.CreateStructGEP(ty, frame, FrameCount)), Mem::FrameLink);
  tag(b_.CreateStore(proc, b_.CreateStructGEP(ty, frame, FrameProc)), Mem::FrameLink);
  tag(b_.CreateStore(ConstantInt::get(i32_, 0), b_.CreateStructGEP(ty, frame, FrameLine)), Mem::FrameLink);
  Value *frames = b_.CreateStructGEP(ctxHeadTy_, fnCtx_, CtxFrames);
  tag(b_.CreateStore(loadFrom(ptrTy_, frames, Mem::Ctx, "prev"), b_.CreateStructGEP(ty, frame, FramePrev)),
      Mem::FrameLink);
  tag(b_.CreateStore(frame, frames), Mem::Ctx);
}

void CodeGen::unlinkFrame(Value *frame) {
  Value *prev = loadFrom(ptrTy_, b_.CreateStructGEP(frameType(0), frame, FramePrev), Mem::FrameLink, "prev");
  tag(b_.CreateStore(prev, b_.CreateStructGEP(ctxHeadTy_, fnCtx_, CtxFrames)), Mem::Ctx);
}

// The module's STATICs, with the function that gives them their initial values,
// and its INIT and EXIT procedures: a global constructor registers them before main() runs.
void CodeGen::genModuleRegistration(const xir::Module &m) {
  Function *init = m.init ? functions_.at(m.init) : nullptr;
  if (!init) { // no initializers: the STATICs are NIL
    init = Function::Create(initFnTy_, GlobalValue::InternalLinkage, "xb.module.init", mod_);
    addAbiAttributes(init);
    b_.SetInsertPoint(BasicBlock::Create(ctx_, "entry", init));
    b_.CreateRetVoid();
  }
  Function *reg = Function::Create(FunctionType::get(Type::getVoidTy(ctx_), {}, false), GlobalValue::InternalLinkage,
                                   "xb.module.register", mod_);
  reg->addFnAttr(Attribute::NoUnwind);
  b_.SetInsertPoint(BasicBlock::Create(ctx_, "entry", reg));
  std::vector<Constant *> procs;
  for (const auto *list : {&m.initProcs, &m.exitProcs})
    for (const xir::Function *f : *list) procs.push_back(functions_.at(f));
  Constant *table = ConstantPointerNull::get(ptrTy_);
  if (!procs.empty()) {
    auto *ty = ArrayType::get(ptrTy_, procs.size());
    table = new GlobalVariable(mod_, ty, true, GlobalValue::PrivateLinkage, ConstantArray::get(ty, procs),
                               "xb.module.procs");
  }
  // The public functions by name (xb_symbol: name, function), for lookups at run time.
  auto *symTy = StructType::get(ctx_, {ptrTy_, ptrTy_});
  std::vector<Constant *> symbols;
  for (auto &f : m.functions)
    if (f->kind == xir::Function::Kind::Program && !f->internal && !runtimeLibrary_)
      symbols.push_back(ConstantStruct::get(
          symTy, {b_.CreateGlobalString(f->name, ".xb.symname", 0, &mod_), functions_.at(f.get())}));
  Constant *symbolTable = ConstantPointerNull::get(ptrTy_);
  if (!symbols.empty()) {
    auto *ty = ArrayType::get(symTy, symbols.size());
    symbolTable = new GlobalVariable(mod_, ty, true, GlobalValue::PrivateLinkage, ConstantArray::get(ty, symbols),
                                     "xb.module.symbols");
  }
  Value *statics = statics_ ? static_cast<Value *>(statics_) : ConstantPointerNull::get(ptrTy_);
  b_.CreateCall(rtModuleRegister_, {init, statics, ConstantInt::get(i32_, m.statics.size()), table,
                                    ConstantInt::get(i32_, m.initProcs.size()),
                                    ConstantInt::get(i32_, m.exitProcs.size()), symbolTable,
                                    ConstantInt::get(i32_, symbols.size())});
  b_.CreateRetVoid();
  appendToGlobalCtors(mod_, reg, 65535);
}

// void xb_app_main(xb_ctx *ctx, uint32_t argc, xb_item *argv) - called by the
// runtime's main() with the command-line arguments, the start function's.
void CodeGen::genEntry(const xir::Function &start) {
  auto *ty = FunctionType::get(Type::getVoidTy(ctx_), {ptrTy_, i32_, ptrTy_}, false);
  auto *fn = Function::Create(ty, GlobalValue::ExternalLinkage, "xb_app_main", mod_);
  addAbiAttributes(fn);
  b_.SetInsertPoint(BasicBlock::Create(ctx_, "entry", fn));
  Value *ctx = fn->getArg(0);
  uint64_t strHdr = dl_.getStructLayout(strType(ArrayType::get(i8_, 0)))->getElementOffset(StrData);
  uint64_t blockCells = dl_.getStructLayout(blockTy_)->getElementOffset(2);
  uint64_t frameSlots = dl_.getStructLayout(frameType(0))->getElementOffset(FrameSlots);
  uint64_t ctxFrames = dl_.getStructLayout(ctxHeadTy_)->getElementOffset(CtxFrames);
  uint64_t ctxDecimals = dl_.getStructLayout(ctxHeadTy_)->getElementOffset(CtxDecimals);
  uint64_t arrayItems = dl_.getStructLayout(arrayTy_)->getElementOffset(ArrayItems);
  b_.CreateCall(rtAbiCheck_, {ConstantInt::get(i32_, XB_ABI_VERSION), ConstantInt::get(i32_, itemSize_),
                              ConstantInt::get(i32_, strHdr), ConstantInt::get(i32_, blockCells),
                              ConstantInt::get(i32_, frameSlots), ConstantInt::get(i32_, ctxFrames),
                              ConstantInt::get(i32_, ctxDecimals), ConstantInt::get(i32_, arrayItems)});
  b_.CreateCall(rtProgramRun_, {ctx, functions_.at(&start), fn->getArg(1), fn->getArg(2)});
  b_.CreateRetVoid();
}

BasicBlock *CodeGen::newBlock(const char *name) { return BasicBlock::Create(ctx_, name, fn_, fnExit_); }

// Items are written and copied as two 8-byte words, the header (type, width,
// decimals) and the value, never as narrower pieces read back whole: a load
// is then served by the store that wrote it (store-to-load forwarding).

// Where a field of the header (type, width, decimals) sits in the header
// word, as a bit position: its offset in xb_item in the target's byte order.
unsigned CodeGen::headerShift(ItemField f) {
  uint64_t offset = dl_.getStructLayout(itemTy_)->getElementOffset(f);
  uint64_t size = dl_.getTypeAllocSize(itemTy_->getElementType(f));
  return (unsigned)(dl_.isLittleEndian() ? offset * 8 : (8 - offset - size) * 8);
}

// Where a value member of `bits` bits (at the union's first byte) sits in the
// value word.
unsigned CodeGen::valueShift(unsigned bits) { return dl_.isLittleEndian() ? 0 : 64 - bits; }

// The header word of an item (as xb_header_word in xbrt.h makes it).
Constant *CodeGen::headerWord(uint32_t tag, uint16_t width, uint16_t dec) {
  uint64_t w = (uint64_t)tag << headerShift(FType) | (uint64_t)width << headerShift(FWidth) |
               (uint64_t)dec << headerShift(FDec);
  return ConstantInt::get(i64_, w);
}

// An address of an item in the heap (an element, a cell, a reference's
// target), for the alias information of the accesses through it. The set is
// keyed by the address value and lives as long as the function being
// lowered, during which no instruction is erased: a new value can never take
// the place of a registered one.
Value *CodeGen::heapItem(Value *addr) {
  heapItems_.insert(addr);
  return addr;
}

// The memory class of an item address: a heap item, as heapItem marked it
// (also through field offsets), an item outside the heap (a stack or global
// item of the module), or unknown (a pointer that may be either, an argument).
CodeGen::Mem CodeGen::itemMemory(Value *addr) {
  Value *v = addr;
  for (;;) {
    if (heapItems_.count(v)) return Mem::HeapItem;
    auto *gep = dyn_cast<GEPOperator>(v);
    if (!gep) break;
    v = gep->getPointerOperand();
  }
  if (isa<AllocaInst>(v)) return Mem::Item;
  if (auto *g = dyn_cast<GlobalVariable>(v); g && g->hasLocalLinkage()) return Mem::Item;
  return Mem::Unknown;
}

LoadInst *CodeGen::loadFrom(Type *t, Value *addr, Mem m, const Twine &name) {
  return tag(b_.CreateLoad(t, addr, name), m);
}

// An item's fields are read out of its header word, loaded whole: an item in
// registers is then one word (no field-sized pieces put back together), and a
// load is served by the 8-byte store that wrote the header.
Value *CodeGen::loadHeader(Value *item) {
  return tag(b_.CreateAlignedLoad(i64_, item, itemAlign_, "header"), itemMemory(item));
}

Value *CodeGen::itemType(Value *item) {
  return b_.CreateTrunc(b_.CreateLShr(loadHeader(item), headerShift(FType)), i32_, "type");
}

// The type of xIR value `v` (whose item is `item`): a constant when type
// inference found a single one (LLVM then folds away the tests on it and the
// paths for other types), else read from the item.
Value *CodeGen::typeOf(const xir::Value *v, Value *item) {
  xir::TypeSet t = plan_->types().of(v);
  if (t != 0 && (t & (t - 1)) == 0) return ConstantInt::get(i32_, (uint64_t)llvm::countr_zero(t));
  return itemType(item);
}

// The value word of an item holding `v`: an i64, a double, or a 4-byte member
// (a logical's i32, a pointer on 32-bit targets), which starts at the first
// byte of the union, with zeros after it.
Value *CodeGen::payloadWord(Value *v) {
  Type *t = v->getType();
  if (t->isDoubleTy()) return b_.CreateBitCast(v, i64_);
  if (t->isPointerTy()) v = b_.CreatePtrToInt(v, intPtrTy_);
  if (v->getType() == i64_) return v;
  return b_.CreateShl(b_.CreateZExt(v, i64_), valueShift(v->getType()->getIntegerBitWidth()));
}

void CodeGen::storeWords(Value *dst, Value *header, Value *payload) {
  Mem m = itemMemory(dst);
  tag(b_.CreateAlignedStore(header, dst, itemAlign_), m);
  tag(b_.CreateAlignedStore(payload, b_.CreateStructGEP(itemTy_, dst, FValue), itemAlign_), m);
}

// Between items in memory the value word moves as a vector of two 32-bit
// integers (bit for bit, on any target): code generation merges a pair of
// accesses of one type into a single 16-byte one, which then spans the two
// stores that wrote the item and cannot be forwarded from them. A Local item
// becomes registers, where the word keeps its integer type.
void CodeGen::copyItem(Value *dst, Value *src) {
  if (dst == src) return;
  bool registers = localItems_.count(dst) || localItems_.count(src);
  Type *word = registers ? static_cast<Type *>(i64_) : FixedVectorType::get(i32_, 2);
  Mem m = itemMemory(src);
  Value *header = tag(b_.CreateAlignedLoad(i64_, src, itemAlign_), m);
  Value *payload = tag(b_.CreateAlignedLoad(word, b_.CreateStructGEP(itemTy_, src, FValue), itemAlign_), m);
  storeWords(dst, header, payload);
}

void CodeGen::setNil(Value *dst) { storeWords(dst, headerWord(XB_T_NIL, 0, 0), ConstantInt::get(i64_, 0)); }

// An item of type `tag`, no width or decimals of its own, holding `v`.
void CodeGen::storeTagged(Value *dst, uint32_t tag, Value *v) {
  storeWords(dst, headerWord(tag, 0, 0), payloadWord(v));
}

void CodeGen::storeLogical(Value *dst, Value *bit) { storeTagged(dst, XB_T_LOGICAL, b_.CreateZExt(bit, i32_)); }

// One load + one well-predicted branch per fallible call; when something is
// pending, xb_pending() serves pauses (GC safepoints) and says whether to unwind.
void CodeGen::checkPending(BasicBlock *unwind) {
  Value *pending = loadFrom(i32_, b_.CreateStructGEP(ctxHeadTy_, fnCtx_, CtxPending), Mem::Ctx, "pending");
  Value *isSet = b_.CreateICmpNE(pending, ConstantInt::get(i32_, 0));
  BasicBlock *slow = newBlock("pending");
  BasicBlock *cont = newBlock("ok");
  MDBuilder md(ctx_);
  b_.CreateCondBr(isSet, slow, cont, md.createUnlikelyBranchWeights());
  b_.SetInsertPoint(slow);
  Value *leave = b_.CreateCall(rtPending_, {fnCtx_});
  b_.CreateCondBr(b_.CreateICmpNE(leave, ConstantInt::get(i32_, 0)), unwind, cont);
  b_.SetInsertPoint(cont);
}

} // namespace xb
