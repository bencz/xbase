// Types, runtime declarations, item and basic block helpers, functions and their frames, module
// initialization, program entry.
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
    : ctx_(ctx), mod_(mod), dl_(mod.getDataLayout()), diag_(diag), b_(ctx) {
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

  xbFuncTy_ = FunctionType::get(Type::getVoidTy(ctx_), {ptrTy_, ptrTy_, i32_, ptrTy_}, false);
  blockFnTy_ = FunctionType::get(Type::getVoidTy(ctx_), {ptrTy_, ptrTy_, i32_, ptrTy_, ptrTy_}, false);
  blockTy_ = StructType::create(ctx_, {ptrTy_, i32_, ArrayType::get(ptrTy_, 0)}, "xb_block");

  Type *voidTy = Type::getVoidTy(ctx_);
  rtFramePush_ = rt("xb_frame_push", ptrTy_, {ptrTy_, i32_});
  rtFramePop_ = rt("xb_frame_pop", voidTy, {ptrTy_, ptrTy_});
  rtArgsCopy_ = rt("xb_args_copy", voidTy, {ptrTy_, i32_, i32_, ptrTy_});
  rtCondError_ = rt("xb_cond_error", i32_, {ptrTy_, ptrTy_});
  rtForTest_ = rt("xb_for_test", i32_, {ptrTy_, ptrTy_, ptrTy_, ptrTy_});
  rtAbiCheck_ = rt("xb_abi_check", voidTy, {i32_, i32_, i32_, i32_});
  rtNeg_ = rt("xb_op_neg", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtNot_ = rt("xb_op_not", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtNumDefaultDec_ = rt("xb_num_default_dec", voidTy, {ptrTy_, ptrTy_, Type::getDoubleTy(ctx_)});
  rtPending_ = rt("xb_pending", i32_, {ptrTy_});
  rtArrayNew_ = rt("xb_array_new", voidTy, {ptrTy_, ptrTy_, i32_, ptrTy_});
  rtIndex_ = rt("xb_op_index", voidTy, {ptrTy_, ptrTy_, ptrTy_, ptrTy_});
  rtIndexSet_ = rt("xb_op_index_set", voidTy, {ptrTy_, ptrTy_, ptrTy_, ptrTy_});
  rtCellBox_ = rt("xb_cell_box", voidTy, {ptrTy_, ptrTy_});
  rtBlockNew_ = rt("xb_block_new", ptrTy_, {ptrTy_, ptrTy_, ptrTy_, i32_});
  rtEnumLen_ = rt("xb_enum_len", i64_, {ptrTy_, ptrTy_});
  rtEnumRefNew_ = rt("xb_enum_ref_new", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtEnumRefMove_ = rt("xb_enum_ref_move", voidTy, {ptrTy_, ptrTy_, i64_});
  rtRefTarget_ = rt("xb_ref_target", ptrTy_, {ptrTy_, ptrTy_});
  rtRefWritten_ = rt("xb_ref_written", voidTy, {ptrTy_});
  rtGcRemember_ = rt("xb_gc_remember", voidTy, {ptrTy_});
  rtSwitchCheck_ = rt("xb_switch_check", voidTy, {ptrTy_, ptrTy_});
  rtSwitchMatch_ = rt("xb_switch_match", i32_, {ptrTy_, ptrTy_});
  rtModuleRegister_ = rt("xb_module_register", voidTy, {ptrTy_, ptrTy_, i32_});
  rtModulesInit_ = rt("xb_modules_init", voidTy, {ptrTy_});
  static const std::pair<BinOp, const char *> bins[] = {
      {BinOp::Add, "xb_op_add"}, {BinOp::Sub, "xb_op_sub"},       {BinOp::Mul, "xb_op_mul"},
      {BinOp::Div, "xb_op_div"}, {BinOp::Mod, "xb_op_mod"},       {BinOp::Pow, "xb_op_pow"},
      {BinOp::Eq, "xb_op_eq"},   {BinOp::ExactEq, "xb_op_exacteq"}, {BinOp::NotEq, "xb_op_ne"},
      {BinOp::Lt, "xb_op_lt"},   {BinOp::Le, "xb_op_le"},         {BinOp::Gt, "xb_op_gt"},
      {BinOp::Ge, "xb_op_ge"},   {BinOp::In, "xb_op_in"},
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

// A function of the program, or of the runtime (an EXTERN FUNCTION): see the
// symbol prefixes in xbrt.h.
Function *CodeGen::declareXbFunc(const std::string &xbName, bool internal, bool runtime) {
  std::string sym = (runtime ? XB_RUNTIME_FUNC_PREFIX : XB_PROGRAM_FUNC_PREFIX) + xbName;
  Function *f = mod_.getFunction(sym);
  if (!f) {
    f = Function::Create(xbFuncTy_, internal ? GlobalValue::InternalLinkage : GlobalValue::ExternalLinkage, sym, mod_);
    addAbiAttributes(f);
  }
  return f;
}

BasicBlock *CodeGen::newBlock(const char *name) { return BasicBlock::Create(ctx_, name, fn_); }

Value *CodeGen::slot(uint32_t idx) { return b_.CreateConstInBoundsGEP1_32(itemTy_, fnBase_, idx); }

uint32_t CodeGen::allocTemps(uint32_t n) {
  uint32_t first = numLocals_ + temps_;
  temps_ += n;
  maxSlots_ = std::max(maxSlots_, numLocals_ + temps_);
  return first;
}

void CodeGen::copyItem(Value *dst, Value *src) {
  if (dst == src) return;
  b_.CreateMemCpy(dst, itemAlign_, src, itemAlign_, itemSize_);
}

void CodeGen::setNil(Value *dst) { b_.CreateMemSet(dst, b_.getInt8(0), itemSize_, itemAlign_); }

void CodeGen::storeLogical(Value *dst, Value *bit) {
  setNil(dst);
  b_.CreateStore(ConstantInt::get(i32_, XB_T_LOGICAL), b_.CreateStructGEP(itemTy_, dst, FType));
  b_.CreateStore(b_.CreateZExt(bit, i32_), b_.CreateStructGEP(itemTy_, dst, FValue));
}

// One load + one well-predicted branch per fallible call; when something is
// pending, xb_pending() serves pauses (GC safepoints) and says whether to unwind.
void CodeGen::checkPending() {
  Value *pending = b_.CreateLoad(i32_, fnCtx_, "pending");
  Value *isSet = b_.CreateICmpNE(pending, ConstantInt::get(i32_, 0));
  BasicBlock *slow = newBlock("pending");
  BasicBlock *cont = newBlock("ok");
  MDBuilder md(ctx_);
  b_.CreateCondBr(isSet, slow, cont, md.createUnlikelyBranchWeights());
  b_.SetInsertPoint(slow);
  Value *unwind = b_.CreateCall(rtPending_, {fnCtx_});
  b_.CreateCondBr(b_.CreateICmpNE(unwind, ConstantInt::get(i32_, 0)), fnExit_, cont);
  b_.SetInsertPoint(cont);
}

void CodeGen::startDeadBlock() { b_.SetInsertPoint(newBlock("dead")); }

bool CodeGen::run(const Module &m, bool emitEntry) {
  if (!m.statics.empty()) {
    auto *ty = ArrayType::get(itemTy_, m.statics.size());
    auto *gv = new GlobalVariable(mod_, ty, false, GlobalValue::InternalLinkage, ConstantAggregateZero::get(ty),
                                  "xb.statics"); // all NIL
    gv->setAlignment(itemAlign_);
    statics_ = gv;
  }
  for (auto &f : m.funcs) funcs_[f->name] = declareXbFunc(f->name, f->isStatic, false);
  for (auto &f : m.funcs) genFunc(*f);
  if (!m.statics.empty()) genModuleInit(m);
  if (emitEntry && !m.funcs.empty()) genEntry(*m.funcs.front());

  std::string err;
  raw_string_ostream os(err);
  if (verifyModule(mod_, &os)) {
    diag_.error({}, "internal compiler error: invalid LLVM module:\n" + os.str());
    return false;
  }
  return diag_.errorCount() == 0;
}

void CodeGen::genFunc(const FuncDecl &f) {
  fn_ = funcs_[f.name];
  fn_->addFnAttr(Attribute::NoUnwind);
  fn_->setUWTableKind(UWTableKind::Async); // backtraces / profilers

  auto *entry = BasicBlock::Create(ctx_, "entry", fn_);
  b_.SetInsertPoint(entry);
  auto args = fn_->arg_begin();
  fnCtx_ = &*args++;
  fnRet_ = &*args++;
  Value *argc = &*args++;
  Value *argv = &*args++;
  fnCtx_->setName("ctx");
  fnRet_->setName("ret");
  argc->setName("argc");
  argv->setName("argv");

  fnSelf_ = nullptr;
  beginFrame(argc, argv, (uint32_t)f.params.size(), f.numLocals, f.boxed);
  genBlock(f.body);
  endFrame();
}

// Function body prologue: an item frame (its size is patched by endFrame),
// the parameters copied into it, captured variables moved into cells.
void CodeGen::beginFrame(Value *argc, Value *argv, uint32_t nparams, uint32_t nlocals, const std::vector<bool> &boxed) {
  numLocals_ = nlocals;
  temps_ = 0;
  maxSlots_ = numLocals_;
  loops_.clear();
  fnBoxed_ = &boxed;
  fnPush_ = b_.CreateCall(rtFramePush_, {fnCtx_, ConstantInt::get(i32_, 0)}, "frame");
  fnBase_ = fnPush_;
  if (nparams) b_.CreateCall(rtArgsCopy_, {fnBase_, ConstantInt::get(i32_, nparams), argc, argv});
  boxCaptured(boxed);
  fnExit_ = BasicBlock::Create(ctx_, "exit", fn_);
}

// Epilogue: every path ends in the exit block, which pops the frame.
void CodeGen::endFrame() {
  if (!b_.GetInsertBlock()->getTerminator()) b_.CreateBr(fnExit_);
  fnExit_->moveAfter(&fn_->back());
  b_.SetInsertPoint(fnExit_);
  b_.CreateCall(rtFramePop_, {fnCtx_, fnBase_});
  b_.CreateRetVoid();
  fnPush_->setArgOperand(1, ConstantInt::get(i32_, maxSlots_));
}

// void xb_app_main(xb_ctx *ctx) - called by the runtime's main().
void CodeGen::genEntry(const FuncDecl &start) {
  auto *ty = FunctionType::get(Type::getVoidTy(ctx_), {ptrTy_}, false);
  auto *fn = Function::Create(ty, GlobalValue::ExternalLinkage, "xb_app_main", mod_);
  fn->addFnAttr(Attribute::NoUnwind);
  b_.SetInsertPoint(BasicBlock::Create(ctx_, "entry", fn));
  Value *ctx = fn->getArg(0);
  uint64_t strHdr = dl_.getStructLayout(strType(ArrayType::get(i8_, 0)))->getElementOffset(3);
  uint64_t blockCells = dl_.getStructLayout(blockTy_)->getElementOffset(2);
  b_.CreateCall(rtAbiCheck_, {ConstantInt::get(i32_, XB_ABI_VERSION), ConstantInt::get(i32_, itemSize_),
                              ConstantInt::get(i32_, strHdr), ConstantInt::get(i32_, blockCells)});
  fn_ = fn;
  fnCtx_ = ctx;
  fnExit_ = BasicBlock::Create(ctx_, "done", fn);
  b_.CreateCall(rtModulesInit_, {ctx});
  checkPending(); // an error in a module initialization ends the program
  Value *ret = b_.CreateCall(rtFramePush_, {ctx, ConstantInt::get(i32_, 1)});
  b_.CreateCall(xbFuncTy_, funcs_[start.name], {ctx, ret, ConstantInt::get(i32_, 0), ConstantPointerNull::get(ptrTy_)});
  b_.CreateCall(rtFramePop_, {ctx, ret});
  b_.CreateBr(fnExit_);
  fnExit_->moveAfter(b_.GetInsertBlock());
  b_.SetInsertPoint(fnExit_);
  b_.CreateRetVoid();
}

// The module's STATICs get their initial values in an initialization
// function; a global constructor registers it, and the STATICs as GC roots,
// before main() runs.
void CodeGen::genModuleInit(const Module &m) {
  auto *initTy = FunctionType::get(Type::getVoidTy(ctx_), {ptrTy_}, false);
  Function *init = Function::Create(initTy, GlobalValue::InternalLinkage, "xb.module.init", mod_);
  init->addFnAttr(Attribute::NoUnwind);
  fn_ = init;
  b_.SetInsertPoint(BasicBlock::Create(ctx_, "entry", init));
  fnCtx_ = init->getArg(0);
  fnRet_ = nullptr;
  fnSelf_ = nullptr;
  static const std::vector<bool> noBoxed;
  beginFrame(nullptr, nullptr, 0, 0, noBoxed);
  for (const LocalDecl *d : m.statics) {
    if (!d->init) continue;
    uint32_t mark = temps_;
    genExpr(*d->init, staticAddress(d->slot));
    temps_ = mark;
  }
  endFrame();

  auto *regTy = FunctionType::get(Type::getVoidTy(ctx_), {}, false);
  Function *reg = Function::Create(regTy, GlobalValue::InternalLinkage, "xb.module.register", mod_);
  reg->addFnAttr(Attribute::NoUnwind);
  b_.SetInsertPoint(BasicBlock::Create(ctx_, "entry", reg));
  b_.CreateCall(rtModuleRegister_, {init, statics_, ConstantInt::get(i32_, m.statics.size())});
  b_.CreateRetVoid();
  appendToGlobalCtors(mod_, reg, 65535);
}

} // namespace xb
