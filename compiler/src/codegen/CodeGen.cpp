// Types, runtime declarations, the module, functions and their frames, item and basic block helpers.
#include "codegen/CodeGen.h"
#include "codegen/LoopSplit.h"
#include "codegen/LoopPolls.h"
#include "codegen/MemoryTags.h"

#include "xbrt.h" // constants only (type tags, ABI version) - never layouts

#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/MDBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdio>
#include <optional>

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

  itemTy_ = StructType::create(ctx_, {i32_, i16_, i16_, rep}, kItemTypeName);
  itemSize_ = dl_.getTypeAllocSize(itemTy_);
  itemAlign_ = dl_.getABITypeAlign(itemTy_);

  Type *voidTy = Type::getVoidTy(ctx_);
  xbFuncTy_ = FunctionType::get(voidTy, {ptrTy_, ptrTy_, i32_, ptrTy_}, false);
  blockFnTy_ = FunctionType::get(voidTy, {ptrTy_, ptrTy_, i32_, ptrTy_, ptrTy_}, false);
  initFnTy_ = FunctionType::get(voidTy, {ptrTy_}, false);
  blockTy_ = StructType::create(ctx_, {ptrTy_, i32_, ArrayType::get(ptrTy_, 0)}, "xb_block");
  arrayTy_ = StructType::create(ctx_, {intPtrTy_, intPtrTy_, ptrTy_}, "xb_array"); // size_t is pointer-sized
  refTy_ = StructType::create(ctx_, {itemTy_, i64_, itemTy_, itemTy_, i32_}, "xb_ref");
  objectTy_ = StructType::create(ctx_, {ptrTy_, i32_, ArrayType::get(itemTy_, 0)}, "xb_object");
  hashTy_ = StructType::create(ctx_, {i32_, i32_, i32_, i32_, i32_, ptrTy_, ptrTy_, ptrTy_, ptrTy_}, "xb_hash");
  hentryTy_ = StructType::create(ctx_, {itemTy_, itemTy_}, "xb_hentry");
  MDBuilder md(ctx_);
  MDNode *root = md.createTBAARoot("xbase");
  static const std::pair<Mem, const char *> kMemNames[] = {
      {Mem::Item, "item"},           {Mem::HeapItem, "heap item"}, {Mem::ArrayHeader, "array header"},
      {Mem::BlockCells, "block cells"}, {Mem::Ctx, "context"},     {Mem::FrameLink, "frame link"},
      {Mem::GcState, "gc state"},       {Mem::String, "string"},    {Mem::ObjectHeader, "object header"},
      {Mem::RefIndex, "reference index"},
  };
  static_assert(std::size(kMemNames) + 1 == (size_t)Mem::Count_, "one name per memory class but Unknown");
  for (auto [m, name] : kMemNames) {
    MDNode *type = md.createTBAAScalarTypeNode(name, root);
    tbaa_[(size_t)m] = md.createTBAAStructTagNode(type, type, 0);
  }
  mod_.getOrInsertNamedMetadata(kItemTagName)->addOperand(tbaa_[(size_t)Mem::Item]); // TagItemMemsets
  // the [ABI] fields of xb_ctx
  ctxHeadTy_ = StructType::create(ctx_, {i32_, i32_, ptrTy_, ptrTy_, ptrTy_}, "xb_ctx_head");
  nilItem_ = new GlobalVariable(mod_, itemTy_, true, GlobalValue::InternalLinkage, Constant::getNullValue(itemTy_),
                                "xb.nil");
  nilItem_->setAlignment(itemAlign_);
  rtCondError_ = rt("xb_cond_error", i32_, {ptrTy_, ptrTy_});
  rtForTest_ = rt("xb_for_test", i32_, {ptrTy_, ptrTy_, ptrTy_, ptrTy_});
  rtAbiCheck_ = rt("xb_abi_check", voidTy,
                   {i32_, i32_, i32_, i32_, i32_, i32_, i32_, i32_, i32_, i32_, i32_, i32_, i32_, i32_});
  rtStackOverflow_ = rt("xb_stack_overflow", voidTy, {ptrTy_});
  rtStackDeep_ = rt("xb_stack_deep", i32_, {ptrTy_, intPtrTy_});
  if (auto *f = dyn_cast<Function>(rtStackDeep_.getCallee())) { // it reads and writes the context alone (core/ctx.c)
    f->setMemoryEffects(MemoryEffects::argMemOnly());
    f->setWillReturn();
    f->setNoSync();
    f->setDoesNotFreeMemory();
  }
  for (auto [callee, name] : {std::pair{&rtExp_, "xb_exp"}, std::pair{&rtLog_, "xb_log"}}) {
    *callee = rt(name, b_.getDoubleTy(), {b_.getDoubleTy()});
    auto *f = cast<Function>(callee->getCallee());
    f->setDoesNotAccessMemory();
    f->setWillReturn();
    f->setNoSync();
  }
  rtNeg_ = rt("xb_op_neg", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtNot_ = rt("xb_op_not", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtBitNot_ = rt("xb_op_bitnot", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtInc_ = rt("xb_op_inc", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtDec_ = rt("xb_op_dec", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtAppend_ = rt("xb_op_append", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtStrLeft_ = rt("xb_str_left", ptrTy_, {ptrTy_, i64_});
  rtStrRight_ = rt("xb_str_right", ptrTy_, {ptrTy_, i64_});
  rtStrSub_ = rt("xb_str_sub", ptrTy_, {ptrTy_, i64_, i32_, i64_});
  rtNumDefaultDec_ = rt("xb_num_default_dec", voidTy, {ptrTy_, ptrTy_, Type::getDoubleTy(ctx_)});
  rtPending_ = rt("xb_pending", i32_, {ptrTy_});
  rtPendingGc_ = rt("xb_pending_gc", i32_, {ptrTy_});
  rtSeqCatch_ = rt("xb_seq_catch", i32_, {ptrTy_});
  rtSeqValue_ = rt("xb_seq_value", voidTy, {ptrTy_, ptrTy_});
  rtSeqWith_ = rt("xb_seq_with", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtSeqRestore_ = rt("xb_seq_restore", voidTy, {ptrTy_, ptrTy_});
  rtSeqSuspend_ = rt("xb_seq_suspend", i32_, {ptrTy_});
  rtSeqResume_ = rt("xb_seq_resume", voidTy, {ptrTy_, i32_, ptrTy_});
  rtHashNew_ = rt("xb_hash_new", voidTy, {ptrTy_, ptrTy_, i32_, ptrTy_});
  rtEachKey_ = rt("xb_each_key", voidTy, {ptrTy_, ptrTy_, ptrTy_, i64_});
  rtArrayNew_ = rt("xb_array_new", voidTy, {ptrTy_, ptrTy_, i32_, ptrTy_});
  rtArgsRest_ = rt("xb_args_rest", i32_, {ptrTy_, ptrTy_, ptrTy_, i32_, i32_, i32_});
  rtIndex_ = rt("xb_op_index", voidTy, {ptrTy_, ptrTy_, ptrTy_, ptrTy_});
  rtIndexSet_ = rt("xb_op_index_set", voidTy, {ptrTy_, ptrTy_, ptrTy_, ptrTy_});
  rtWriteItem_ = rt("xb_gc_write_item", voidTy, {ptrTy_, intPtrTy_});
  rtWriteItems_ = rt("xb_gc_write_items", voidTy, {ptrTy_, intPtrTy_, intPtrTy_});
  rtCellOf_ = rt("xb_cell_of", ptrTy_, {ptrTy_, ptrTy_, i32_});
  rtCellNew_ = rt("xb_cell_new", ptrTy_, {ptrTy_, ptrTy_});
  rtBlockNew_ = rt("xb_block_new", ptrTy_, {ptrTy_, ptrTy_, ptrTy_, i32_});
  rtRootPush_ = rt("xb_gc_root_push", voidTy, {ptrTy_, ptrTy_});
  rtRootPop_ = rt("xb_gc_root_pop", voidTy, {ptrTy_, i32_});
  rtEnumLen_ = rt("xb_enum_len", i64_, {ptrTy_, ptrTy_});
  rtEachSource_ = rt("xb_each_source", voidTy, {ptrTy_, ptrTy_, ptrTy_, i32_});
  rtEachShared_ = rt("xb_each_shared", i32_, {ptrTy_, ptrTy_, i32_});
  rtEachEnter_ = rt("xb_each_enter", voidTy, {ptrTy_, ptrTy_, ptrTy_, i32_, ptrTy_});
  rtEachMove_ = rt("xb_each_move", voidTy, {ptrTy_, ptrTy_, i32_, i64_});
  rtEachLeave_ = rt("xb_each_leave", voidTy, {ptrTy_, i32_, ptrTy_});
  rtRefLoad_ = rt("xb_ref_load", voidTy, {ptrTy_, i32_, ptrTy_, ptrTy_});
  storageShared_ = mod_.getOrInsertGlobal("xb_storage_shared", i32_);
  rtRefStore_ = rt("xb_ref_store", voidTy, {ptrTy_, i32_, ptrTy_, ptrTy_});
  rtRefOf_ = rt("xb_ref_of", voidTy, {ptrTy_, ptrTy_, ptrTy_, i32_});
  rtRefElem_ = rt("xb_ref_elem", voidTy, {ptrTy_, ptrTy_, ptrTy_, ptrTy_});
  rtGcRemember_ = rt("xb_gc_remember", voidTy, {ptrTy_});
  rtSwitchCheck_ = rt("xb_switch_check", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtSwitchMatch_ = rt("xb_switch_match", i32_, {ptrTy_, ptrTy_});
  rtModuleRegister_ =
      rt("xb_module_register", voidTy, {ptrTy_, ptrTy_, i32_, ptrTy_, i32_, i32_, ptrTy_, i32_, ptrTy_, i32_, i32_});
  rtMvMark_ = rt("xb_mv_mark", i32_, {ptrTy_});
  rtMvRelease_ = rt("xb_mv_release", voidTy, {ptrTy_, i32_});
  rtMvGet_ = rt("xb_mv_get", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtMvRef_ = rt("xb_mv_ref", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtMvSet_ = rt("xb_mv_set", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtMvPrivate_ = rt("xb_mv_private", voidTy, {ptrTy_, ptrTy_, i32_});
  rtMvPublic_ = rt("xb_mv_public", voidTy, {ptrTy_, ptrTy_});
  rtMvParam_ = rt("xb_mv_param", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtMvField_ = rt("xb_mv_field", voidTy, {ptrTy_, ptrTy_, ptrTy_, ptrTy_});
  rtFieldGet_ = rt("xb_field_get", voidTy, {ptrTy_, ptrTy_, ptrTy_, ptrTy_, ptrTy_, ptrTy_});
  rtFieldSet_ = rt("xb_field_set", voidTy, {ptrTy_, ptrTy_, ptrTy_, ptrTy_, ptrTy_, ptrTy_});
  rtMacroGet_ = rt("xb_macro_get", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtMacroRef_ = rt("xb_macro_ref", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtMacroSet_ = rt("xb_macro_set", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtMacroCall_ = rt("xb_macro_call", voidTy, {ptrTy_, ptrTy_, ptrTy_, i32_, ptrTy_});
  rtMacroSend_ = rt("xb_macro_send", voidTy, {ptrTy_, ptrTy_, ptrTy_, i32_, i32_, i32_, ptrTy_});
  rtMacroPrivate_ = rt("xb_macro_private", voidTy, {ptrTy_, ptrTy_, i32_});
  rtMacroPublic_ = rt("xb_macro_public", voidTy, {ptrTy_, ptrTy_});
  rtMacroText_ = rt("xb_macro_text", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtProgramRun_ = rt("xb_program_run", voidTy, {ptrTy_, ptrTy_, i32_, ptrTy_});
  rtGenNew_ = rt("xb_gen_new", voidTy, {ptrTy_, ptrTy_, ptrTy_, i32_, ptrTy_});
  rtGenRestore_ = rt("xb_gen_restore", i32_, {ptrTy_, ptrTy_, ptrTy_, i32_});
  rtGenYield_ = rt("xb_gen_yield", voidTy, {ptrTy_, ptrTy_, ptrTy_, i32_, i32_, ptrTy_});
  rtGenDone_ = rt("xb_gen_done", voidTy, {ptrTy_, ptrTy_});
  rtAsyncNew_ = rt("xb_async_new", voidTy, {ptrTy_, ptrTy_, ptrTy_, ptrTy_, i32_, ptrTy_});
  rtAsyncRestore_ = rt("xb_async_restore", i32_, {ptrTy_, ptrTy_, i32_});
  rtAsyncSuspend_ = rt("xb_async_suspend", voidTy, {ptrTy_, ptrTy_, i32_, i32_, ptrTy_});
  rtAgenNew_ = rt("xb_agen_new", voidTy, {ptrTy_, ptrTy_, ptrTy_, i32_, ptrTy_});
  rtAsyncYield_ = rt("xb_async_yield", voidTy, {ptrTy_, ptrTy_, i32_, i32_, ptrTy_});
  rtAwaitFuse_ = rt("xb_await_fuse", voidTy, {ptrTy_, ptrTy_});
  rtAwaitFused_ = rt("xb_await_fused", i32_, {ptrTy_});
  rtAwaitReady_ = rt("xb_await_ready", i32_, {ptrTy_});
  rtAwaitResult_ = rt("xb_await_result", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtAwaitWait_ = rt("xb_await_wait", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtSendFrom_ = rt("xb_op_send_from", voidTy, {ptrTy_, ptrTy_, ptrTy_, ptrTy_, i32_, ptrTy_, i32_, ptrTy_});
  rtObjectGet_ = rt("xb_op_object_get", voidTy, {ptrTy_, ptrTy_, i32_});
  rtObjectSet_ = rt("xb_op_object_set", voidTy, {ptrTy_, ptrTy_, i32_, ptrTy_});
  rtClassInstance_ = rt("xb_class_instance", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtClassesRegister_ = rt("xb_classes_register", voidTy, {ptrTy_, i32_, i32_});
  for (FunctionCallee c : {rtWriteItem_, rtWriteItems_, rtGcRemember_})
    cast<Function>(c.getCallee())->addFnAttr(kBarrierAttr);
  for (FunctionCallee c : {rtPending_, rtWriteItem_, rtWriteItems_, rtGcRemember_, rtCellOf_, rtCellNew_,
                           rtBlockNew_, rtRootPush_, rtRootPop_, rtArrayNew_,
                           rtEachShared_, rtEachEnter_, rtEachLeave_, rtRefOf_, rtNumDefaultDec_, rtSwitchMatch_,
                           rtSeqCatch_, rtSeqValue_, rtSeqWith_, rtSeqRestore_, rtSeqSuspend_, rtSeqResume_,
                           rtAbiCheck_, rtModuleRegister_, rtMvMark_, rtMvRelease_, rtMvPrivate_, rtMvPublic_,
                           rtMvParam_, rtGenRestore_, rtGenYield_, rtGenDone_, rtAsyncRestore_, rtAsyncSuspend_,
                           rtAsyncYield_, rtAwaitReady_, rtAwaitFuse_, rtAwaitFused_})
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
  rtAddGrow_ = rt("xb_op_add_grow", voidTy, {ptrTy_, ptrTy_, ptrTy_, ptrTy_});
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
  if (program && f.internal && runtimeLibrary_) { // reached by programs that run a body of the runtime's in place
    Function *fn = Function::Create(xbFuncTy_, GlobalValue::ExternalLinkage, librarySymbol(moduleName_, f.name), mod_);
    addAbiAttributes(fn);
    return fn;
  }
  if (program && !f.internal) {
    std::string sym = runtimeLibrary_     ? XB_RUNTIME_FUNC_PREFIX + f.name
                      : library_.empty() ? XB_PROGRAM_FUNC_PREFIX + f.name
                                          : xblibSymbol(library_, f.name);
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

// A symbol of a scope (a runtime module, a library): the prefix, the scope, '$', the name with ':' and '.' as '$'.
static std::string scopedSymbol(const char *prefix, const std::string &scope, const std::string &name) {
  std::string sym = prefix + scope + "$" + name;
  for (char &c : sym)
    if (c == ':' || c == '.') c = '$';
  return sym;
}

// The symbol of a function of a runtime module written in xBase (XB_LIBRARY_FUNC_PREFIX in xbrt.h).
std::string CodeGen::librarySymbol(const std::string &module, const std::string &name) {
  return scopedSymbol(XB_LIBRARY_FUNC_PREFIX, module, name);
}

// The LLVM function of a function of the module, or of a carried runtime source's (declared by its library symbol).
Function *CodeGen::functionOf(const xir::Function *f) {
  if (auto it = functions_.find(f); it != functions_.end()) return it->second;
  auto lib = carriedModule_.find(f);
  if (!f->carried || lib == carriedModule_.end()) {
    diag_.error({}, "internal compiler error: function " + f->name + " of no module");
    return functions_[f] = declare(*f);
  }
  Function *fn = Function::Create(xbFuncTy_, GlobalValue::ExternalLinkage, librarySymbol(lib->second, f->name), mod_);
  addAbiAttributes(fn);
  return functions_[f] = fn;
}

std::string CodeGen::xblibSymbol(const std::string &library, const std::string &name) {
  return scopedSymbol(XB_XBLIB_FUNC_PREFIX, library, name);
}

std::string CodeGen::sourceStamp(const char *prefix, const std::string &scope, uint64_t hash) {
  char hex[17];
  std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(hash));
  return scopedSymbol(prefix, scope, std::string("source$") + hex);
}

void CodeGen::setLibrary(const std::string &name, std::unordered_set<std::string> functions, uint64_t stamp,
                         uint32_t level) {
  library_ = name;
  libraryLevel_ = level;
  libraryFunctions_ = std::move(functions);
  libraryStamp_ = stamp;
}

// Stamps of sources (xir::Module::sourceHash, a library's): a module of the runtime, and a library, defines its own; a
// program given the carried modules, or compiled with libraries, references theirs, so that a runtime or a library
// built from other texts than the ones it was compiled with fails to link instead of running code that disagrees
// with it (bodies inlined, an interface changed).
void CodeGen::stampSources(const xir::Module &m) {
  auto define = [&](const std::string &symbol) {
    if (!mod_.getNamedGlobal(symbol))
      new GlobalVariable(mod_, i8_, true, GlobalValue::ExternalLinkage, ConstantInt::get(i8_, 0), symbol);
  };
  if (runtimeLibrary_) {
    if (m.sourceHash) define(sourceStamp(XB_LIBRARY_FUNC_PREFIX, m.name, m.sourceHash));
    return;
  }
  if (!library_.empty()) define(sourceStamp(XB_XBLIB_FUNC_PREFIX, library_, libraryStamp_));
  for (const xir::Module *l : m.library)
    requiredStamps_.insert(sourceStamp(XB_LIBRARY_FUNC_PREFIX, l->name, l->sourceHash));
  for (auto &[lib, stamp] : libraryStamps_) requiredStamps_.insert(sourceStamp(XB_XBLIB_FUNC_PREFIX, lib, stamp));
}

// A table of `symbols` kept in the object file, so the linker resolves each one (bringing in what defines it from a
// library) or reports it missing.
void CodeGen::keepReferences(const std::vector<Constant *> &symbols, const std::string &name) {
  if (symbols.empty()) return;
  auto *ty = ArrayType::get(ptrTy_, symbols.size());
  auto *table =
      new GlobalVariable(mod_, ty, true, GlobalValue::PrivateLinkage, ConstantArray::get(ty, symbols), name);
  appendToCompilerUsed(mod_, {table});
}

// A function of the runtime (NATIVE), of an xbase library, or of another module of the program (of the library, for
// one of its modules).
Function *CodeGen::externFunction(const xir::Callee &c) {
  std::string sym = c.native              ? XB_RUNTIME_FUNC_PREFIX + c.name
                    : !c.library.empty() ? xblibSymbol(c.library, c.name)
                    : libraryFunctions_.count(c.name) ? xblibSymbol(library_, c.name)
                                                      : XB_PROGRAM_FUNC_PREFIX + c.name;
  Function *f = mod_.getFunction(sym);
  if (!f) {
    f = Function::Create(xbFuncTy_, GlobalValue::ExternalLinkage, sym, mod_);
    addAbiAttributes(f);
  }
  return f;
}

void CodeGen::run(const xir::Module &m) {
  // The runtime's own xBase functions show no file, as its C functions do (and no line).
  file_ = ConstantPointerNull::get(ptrTy_);
  if (!runtimeLibrary_ && !m.file.empty()) file_ = b_.CreateGlobalString(m.file, ".xb.file", 0, &mod_);
  statics_ = nullptr;
  if (!m.statics.empty()) {
    auto *ty = ArrayType::get(itemTy_, m.statics.size());
    auto *gv = new GlobalVariable(mod_, ty, false, GlobalValue::InternalLinkage, ConstantAggregateZero::get(ty),
                                  "xb.statics"); // all NIL
    gv->setAlignment(itemAlign_);
    statics_ = gv;
  }
  collectMemvars(m);
  moduleName_ = m.name;
  for (const xir::Module *l : m.library)
    for (auto &f : l->functions) carriedModule_[f.get()] = l->name;
  stampSources(m);
  for (auto &f : m.functions) functions_[f.get()] = declare(*f);
  genClasses(m);
  directEntries(m);
  for (auto &f : m.functions) genFunction(*f);
  for (auto &f : m.functions)
    if (direct_.count(f.get())) genThunk(*f);
  genModuleRegistration(m); // its public functions go into the symbol table
  genRequests(m);
}

// The module's classes: an xb_class_def each (xbrt.h), in a table the runtime
// writes (its `runtime` field) and registers when the module does. A parent
// of the module points at its entry.
void CodeGen::genClasses(const xir::Module &m) {
  classTable_ = nullptr;
  if (m.classes.empty()) return;
  auto *parentTy = StructType::get(ctx_, {ptrTy_, ptrTy_});
  auto *varTy = StructType::get(ctx_, {ptrTy_, i32_});
  auto *methodTy = StructType::get(ctx_, {ptrTy_, ptrTy_, i32_});
  auto *defTy = StructType::get(ctx_, {ptrTy_, ptrTy_, i32_, ptrTy_, i32_, ptrTy_, i32_, ptrTy_, i32_, ptrTy_, i32_,
                                       ptrTy_, i32_, ptrTy_, ptrTy_, i32_, ptrTy_});
  auto *ty = ArrayType::get(defTy, m.classes.size());
  classTable_ = new GlobalVariable(mod_, ty, false, GlobalValue::InternalLinkage, nullptr, "xb.module.classes");
  auto name = [&](const std::string &s) { return b_.CreateGlobalString(s, ".xb.classname", 0, &mod_); };
  // A constant array of `ty` holding `elems`, or a null pointer when there are none.
  auto table = [&](Type *ety, const std::vector<Constant *> &elems, const char *label) -> Constant * {
    if (elems.empty()) return ConstantPointerNull::get(ptrTy_);
    auto *aty = ArrayType::get(ety, elems.size());
    return new GlobalVariable(mod_, aty, true, GlobalValue::PrivateLinkage, ConstantArray::get(aty, elems), label);
  };
  std::vector<Constant *> defs;
  for (const xir::Module::Class &c : m.classes) {
    std::vector<Constant *> parents, vars, classVars, methods, friendClasses, friendFunctions;
    for (const auto &p : c.parents) {
      Constant *def = p.local >= 0 ? classDef(p.local) : ConstantPointerNull::get(ptrTy_);
      parents.push_back(ConstantStruct::get(parentTy, {name(p.name), def}));
    }
    for (const auto &v : c.vars)
      vars.push_back(ConstantStruct::get(varTy, {name(v.name), ConstantInt::get(i32_, v.flags)}));
    for (const auto &v : c.classVars)
      classVars.push_back(ConstantStruct::get(varTy, {name(v.name), ConstantInt::get(i32_, v.flags)}));
    for (const std::string &f : c.friendClasses) friendClasses.push_back(name(f));
    for (const std::string &f : c.friendFunctions) friendFunctions.push_back(name(f));
    for (const auto &mt : c.methods)
      methods.push_back(ConstantStruct::get(
          methodTy, {name(mt.message), functions_.at(mt.fn), ConstantInt::get(i32_, mt.flags)}));
    auto fnOrNull = [&](xir::Function *f) -> Constant * {
      return f ? static_cast<Constant *>(functions_.at(f)) : ConstantPointerNull::get(ptrTy_);
    };
    defs.push_back(ConstantStruct::get(
        defTy, {name(c.name), table(parentTy, parents, "xb.class.parents"), ConstantInt::get(i32_, parents.size()),
                table(varTy, vars, "xb.class.vars"), ConstantInt::get(i32_, vars.size()),
                table(methodTy, methods, "xb.class.methods"), ConstantInt::get(i32_, methods.size()),
                table(varTy, classVars, "xb.class.classvars"), ConstantInt::get(i32_, classVars.size()),
                table(ptrTy_, friendClasses, "xb.class.friends"), ConstantInt::get(i32_, friendClasses.size()),
                table(ptrTy_, friendFunctions, "xb.class.friends"), ConstantInt::get(i32_, friendFunctions.size()),
                fnOrNull(c.init), fnOrNull(c.destructor),
                ConstantInt::get(i32_, (c.isStatic ? XB_CLASS_STATIC : 0u) | (c.scalar ? XB_CLASS_SCALAR : 0u) |
                                            (runtimeLibrary_ ? XB_CLASS_RUNTIME : 0u)),
                ConstantPointerNull::get(ptrTy_)}));
  }
  classTable_->setInitializer(ConstantArray::get(ty, defs));
}

// The xb_class_def of class `index` of the module.
Constant *CodeGen::classDef(int32_t index) {
  return ConstantExpr::getInBoundsGetElementPtr(
      classTable_->getValueType(), classTable_,
      ArrayRef<Constant *>{ConstantInt::get(i32_, 0), ConstantInt::get(i32_, (uint64_t)index)});
}

// The module's memory variables: one xb_memvar per name its functions use, in
// a table the runtime numbers when the module registers.
void CodeGen::collectMemvars(const xir::Module &m) {
  memvars_.clear();
  memvarTable_ = nullptr;
  std::vector<std::string> names;
  for (auto &f : m.functions)
    for (auto &b : f->blocks)
      for (auto &i : b->insts)
        if (xir::isMemvarOp(i->op) && !memvars_.count(i->message)) {
          memvars_[i->message] = nullptr;
          names.push_back(i->message);
        }
  if (names.empty()) return;
  auto *entryTy = StructType::get(ctx_, {ptrTy_, i32_});
  std::vector<Constant *> entries;
  for (const std::string &n : names)
    entries.push_back(ConstantStruct::get(
        entryTy, {b_.CreateGlobalString(n, ".xb.mvname", 0, &mod_), ConstantInt::get(i32_, 0)}));
  auto *ty = ArrayType::get(entryTy, entries.size());
  memvarTable_ = new GlobalVariable(mod_, ty, false, GlobalValue::InternalLinkage, ConstantArray::get(ty, entries),
                                    "xb.module.memvars"); // written: the runtime stores each number
  for (size_t k = 0; k < names.size(); ++k)
    memvars_[names[k]] = ConstantExpr::getInBoundsGetElementPtr(
        ty, memvarTable_, ArrayRef<Constant *>{ConstantInt::get(i32_, 0), ConstantInt::get(i32_, k)});
}

// EXTERNAL: the functions, which the linker brings in (from a library) or reports missing.
void CodeGen::genRequests(const xir::Module &m) {
  std::vector<Constant *> fns;
  for (const xir::Callee &c : m.requests) fns.push_back(externFunction(c));
  keepReferences(fns, "xb.module.requests");
}

bool CodeGen::finish(const xir::Function *start) {
  if (start) genEntry(*start);
  std::vector<Constant *> stamps;
  for (const std::string &s : requiredStamps_) stamps.push_back(cast<Constant>(mod_.getOrInsertGlobal(s, i8_)));
  keepReferences(stamps, "xb.source.stamps");
  std::string err;
  raw_string_ostream os(err);
  if (verifyModule(mod_, &os)) {
    diag_.error({}, "internal compiler error: invalid LLVM module:\n" + os.str());
    return false;
  }
  return diag_.errorCount() == 0;
}

// The direct entries (xir::hasDirectEntry): calls of the module pass them the
// parameters' items in registers, without argc, argv or argument slots.
void CodeGen::directEntries(const xir::Module &m) {
  for (auto &f : m.functions) {
    if (!xir::hasDirectEntry(*f)) continue;
    bool words = returnsWords(*f);
    std::vector<Type *> params{ptrTy_, ptrTy_}; // the context, then the result's item unless it returns words
    if (words) params.pop_back();
    params.push_back(ptrTy_); // the caller's frame
    for (uint32_t k = 0; k < f->numParams; ++k) params.insert(params.end(), {i64_, i64_});
    Type *result = words ? static_cast<Type *>(StructType::get(ctx_, {i64_, i64_})) : Type::getVoidTy(ctx_);
    Function *fn = Function::Create(FunctionType::get(result, params, false),
                                    GlobalValue::InternalLinkage, "xb.direct." + f->name, mod_);
    addAbiAttributes(fn);
    direct_[f.get()] = fn;
  }
}

// The dynamic entry of a function with a direct one: the parameters' items
// from argv (NIL past argc), passed in registers; a result returned as words
// written to `ret`.
void CodeGen::genThunk(const xir::Function &f) {
  Function *thunk = functions_.at(&f);
  Function *body = direct_.at(&f);
  thunk->setUWTableKind(UWTableKind::Async);
  b_.SetInsertPoint(BasicBlock::Create(ctx_, "entry", thunk));
  Value *argc = thunk->getArg(2), *argv = thunk->getArg(3);
  fnCtx_ = thunk->getArg(0);
  Value *frames = loadFrom(ptrTy_, b_.CreateStructGEP(ctxHeadTy_, fnCtx_, CtxFrames), Mem::Ctx, "frames");
  bool words = returnsWords(f);
  std::vector<Value *> args{fnCtx_};
  if (!words) args.push_back(thunk->getArg(1));
  args.push_back(frames);
  for (uint32_t k = 0; k < f.numParams; ++k) {
    Words w = loadWords(argItem(argc, argv, k));
    args.insert(args.end(), {w.header, w.payload});
  }
  takeLent(f, thunk, argc, argv);
  CallInst *result = b_.CreateCall(body, args);
  if (words) storeWords(thunk->getArg(1), b_.CreateExtractValue(result, 0), b_.CreateExtractValue(result, 1));
  else result->setTailCall();
  b_.CreateRetVoid();
}

// Arguments the caller lent (xb_ctx.lent is argv), once their words are read: the loan ends, and those the function
// has parameters for are set to NIL, so that the function alone holds what only they held (nothing can collect before
// its body stores the words into its frame).
void CodeGen::takeLent(const xir::Function &f, Function *thunk, Value *argc, Value *argv) {
  if (f.numParams == 0) return;
  Value *at = b_.CreateStructGEP(ctxHeadTy_, fnCtx_, CtxLent);
  auto block = [&](const char *name) { return BasicBlock::Create(ctx_, name, thunk); };
  BasicBlock *take = block("args.take"), *all = block("args.all"), *some = block("args.some"), *call = block("call");
  b_.CreateCondBr(b_.CreateICmpEQ(loadFrom(ptrTy_, at, Mem::Ctx, "lent"), argv), take, call);
  b_.SetInsertPoint(take);
  tag(b_.CreateStore(ConstantPointerNull::get(ptrTy_), at), Mem::Ctx);
  b_.CreateCondBr(b_.CreateICmpUGE(argc, ConstantInt::get(i32_, f.numParams)), all, some);
  b_.SetInsertPoint(all);
  for (uint32_t k = 0; k < f.numParams; ++k) setNil(b_.CreateConstInBoundsGEP1_32(itemTy_, argv, k));
  b_.CreateBr(call);
  b_.SetInsertPoint(some); // fewer arguments than parameters: those given
  for (uint32_t k = 0; k < f.numParams; ++k) {
    BasicBlock *one = block("args.one"), *next = block("args.next");
    b_.CreateCondBr(b_.CreateICmpULT(ConstantInt::get(i32_, k), argc), one, call);
    b_.SetInsertPoint(one);
    setNil(b_.CreateConstInBoundsGEP1_32(itemTy_, argv, k));
    b_.CreateBr(next);
    b_.SetInsertPoint(next);
  }
  b_.CreateBr(call);
  b_.SetInsertPoint(call);
}

// Whether a call through the dynamic entry lends its arguments: when one from operand `first` may refer to a heap
// object (a send's object alone is no reason: its method holds it, Self, until it returns, as its caller would).
bool CodeGen::lends(const xir::Instruction &i, size_t first) const {
  for (size_t k = first; k < i.operands().size(); ++k)
    if (!plan_->types().noHeap(i.operand(k))) return true;
  return false;
}

// A call through the dynamic entry whose arguments are the caller's temporaries (nothing reads them after it): the
// callee may take them (xb_ctx.lent; takeLent), until the call returns.
void CodeGen::lend(Value *argv) {
  tag(b_.CreateStore(argv, b_.CreateStructGEP(ctxHeadTy_, fnCtx_, CtxLent)), Mem::Ctx);
}

void CodeGen::endLend() {
  tag(b_.CreateStore(ConstantPointerNull::get(ptrTy_), b_.CreateStructGEP(ctxHeadTy_, fnCtx_, CtxLent)), Mem::Ctx);
}

// Prologue: the item frame (sized by the frame plan), the parameters copied
// into it; the body; the exit block, where every path ends, unlinks the frame.
void CodeGen::genFunction(const xir::Function &f) {
  auto direct = direct_.find(&f);
  fnDirect_ = direct != direct_.end();
  fn_ = fnDirect_ ? direct->second : functions_.at(&f);
  fn_->setUWTableKind(UWTableKind::Async); // backtraces / profilers
  FramePlan plan(f);
  plan_ = &plan;
  fnShown_ = &f;
  blocks_.clear();
  blockOrder_.clear();
  inlineFrames_.clear();
  callFrame_.clear();
  values_.clear();
  fusedTruth_.clear();
  findSharingLoads(f, plan.cfg());
  words_.clear();
  wordsAt_.clear();
  locals_.clear();
  paramItems_.clear();
  localItems_.clear();
  heapItems_.clear();
  scratch_.clear();
  blockTemp_ = nullptr;

  BasicBlock *entry = BasicBlock::Create(ctx_, "entry", fn_);
  b_.SetInsertPoint(entry);
  fnCtx_ = fn_->getArg(0);
  fnCtx_->setName("ctx");
  fnRet_ = nullptr;
  fnSelf_ = nullptr;
  fnParams_ = f.numParams;
  fnSkip_ = f.method ? 1 : 0;
  size_t rests = 0;
  const xir::BasicBlock *restBlock = nullptr;
  for (auto &b : f.blocks)
    for (auto &inst : b->insts)
      if (inst->op == xir::Op::ArgsRest) {
        ++rests;
        restBlock = b.get();
      }
  // Once per call: in a loop, a callee's change (PValue()) would reach the next pass.
  forwardInPlace_ = rests == 1 && !f.readsArgs && !xir::Loops(plan.cfg()).innermost(restBlock);
  Value *argc = nullptr, *argv = nullptr;
  fnWords_ = fnDirect_ && returnsWords(f);
  if (fnWords_) {
    // The result, returned as its two words: an item of its own (registers), NIL when the function unwinds, so the
    // caller's item that receives it stays valid for the collector.
    fnRet_ = entryAlloca("result");
    localItems_.insert(fnRet_);
    setNil(fnRet_);
  } else if (f.kind != xir::Function::Kind::ModuleInit) {
    fnRet_ = fn_->getArg(1);
    fnRet_->setName("ret");
  }
  // The caller's frame, which the context's chain has at the call.
  fnPrev_ = fnDirect_ ? fn_->getArg(fnWords_ ? 1 : 2) : nullptr;
  if (f.kind != xir::Function::Kind::ModuleInit && !fnDirect_) {
    argc = fn_->getArg(2);
    argv = fn_->getArg(3);
    argc->setName("argc");
    argv->setName("argv");
  }
  if (f.kind == xir::Function::Kind::Block) {
    fnSelf_ = fn_->getArg(4);
    fnSelf_->setName("self");
  }
  pushFrame(plan.frameSize(), f.numParams, argc, argv, procOf(f), f.readsArgs && argc);
  // A function without the direct entry keeps its arguments (it reads them as received, forwards them, or is the
  // other body of one that does): a loan of them ends here, so that no call it makes with them takes them.
  if (f.kind == xir::Function::Kind::Program && !fnDirect_) endLend();
  // An activation that may create PRIVATEs ends them on its way out, also when it unwinds.
  bool privates = f.callsPrivates;
  for (auto &b : f.blocks)
    for (auto &i : b->insts) privates = privates || xir::createsPrivates(i->op);
  if (f.ownsPrivates) privates = false; // its environment's, from its start: mark 0 (below), never released here
  fnMark_ = privates ? b_.CreateCall(rtMvMark_, {fnCtx_}, "mark") : nullptr;
  fnExit_ = BasicBlock::Create(ctx_, "exit", fn_);

  // Blocks in reverse postorder; machine-value block parameters become PHIs.
  for (xir::BasicBlock *xb : plan.cfg().order()) {
    BasicBlock *bb = BasicBlock::Create(ctx_, xb->name, fn_, fnExit_);
    blockOrder_[xb] = blocks_.size();
    blocks_[xb] = bb;
    b_.SetInsertPoint(bb);
    for (auto &p : xb->params)
      if (p->type() != xir::Type::Item)
        values_[p.get()] = b_.CreatePHI(p->type() == xir::Type::Bool ? b_.getInt1Ty() : (Type *)i64_, 2);
  }
  b_.SetInsertPoint(entry);
  checkStack(fnFrame_, fnExit_, nullptr); // the frame is linked: the function returns through its exit
  b_.CreateBr(blocks_.at(f.entry()));
  xir::openInlineBodies(plan.cfg(), inlineAt_); // verified: they nest
  for (xir::BasicBlock *xb : plan.cfg().order()) {
    b_.SetInsertPoint(blocks_.at(xb));
    unwind_ = xb->unwind ? blocks_.at(xb->unwind) : fnExit_;
    inlineOpen_ = inlineAt_[xb];
    for (uint32_t s : plan.clearsAtStart(xb)) setNil(slot(s));
    findFusedComparisons(*xb);
    findFusedCalls(*xb);
    findFusedSends(*xb);
    for (auto &p : xb->params) cacheWords(p.get());
    for (auto &i : xb->insts) {
      line_ = i->loc.line;
      emitting_ = i.get();
      emit(*i);
      if (!xir::isTerminator(i->op)) cacheWords(i.get());
    }
  }
  line_ = 0;
  emitting_ = nullptr;
  inlineOpen_.clear();
  b_.SetInsertPoint(fnExit_);
  if (fnMark_) b_.CreateCall(rtMvRelease_, {fnCtx_, fnMark_});
  unlinkFrame(fnFrame_, fnPrev_);
  if (fnWords_) {
    Words w = loadWords(fnRet_);
    b_.CreateAggregateRet({w.header, w.payload});
  } else {
    b_.CreateRetVoid();
  }
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
  if (auto it = procs_.find(&f); it != procs_.end()) return it->second;
  Constant *name = b_.CreateGlobalString(f.stackName, ".xb.procname", 0, &mod_);
  auto *ty = StructType::get(ctx_, {ptrTy_, i32_, i32_, ptrTy_});
  Constant *nparams = ConstantInt::get(i32_, f.numParams);
  Constant *flags = ConstantInt::get(i32_, (f.readsArgs ? XB_PROC_ARGS : 0) | (f.hidden ? XB_PROC_HIDDEN : 0) |
                                               (f.method ? XB_PROC_METHOD : 0));
  auto *proc = new GlobalVariable(mod_, ty, true, GlobalValue::PrivateLinkage,
                                  ConstantStruct::get(ty, {name, nparams, flags,
                                                           f.runtime ? ConstantPointerNull::get(ptrTy_) : file_}),
                                  ".xb.proc");
  proc->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
  procs_[&f] = proc;
  return proc;
}

// Whether the innermost frame of the code being lowered shows lines: not one of a function of the runtime, whose
// frames show line 0 (its module's, or a function of a carried runtime source inlined or copied in).
bool CodeGen::showsLines() const {
  return !(inlineOpen_.empty() ? fnShown_->runtime : inlineOpen_.back()->runtime);
}

void CodeGen::markLine(Instruction *i) {
  if (!line_ || !showsLines() || !isa<CallInst>(i)) return;
  i->setMetadata(lineKind_, MDNode::get(ctx_, ConstantAsMetadata::get(ConstantInt::get(i32_, line_))));
  if (!inlineOpen_.empty()) callFrame_[i] = innermostFrame();
}

// The innermost frame of the code being lowered: the function's, or that of the inlined body it is in.
Value *CodeGen::innermostFrame() const {
  return inlineOpen_.empty() ? fnFrame_ : inlineFrames_.at(inlineOpen_.size() - 1);
}

// The frame's line, stored right before each call that can see the call stack
// (a function of the program, a send, a runtime function or operator, which
// may raise an error): only there can it be read, so the fast paths of
// operations store nothing, and their slow paths, which are cold, do. A line
// is stored once until another one is needed, also across blocks: where every
// way into a block stored the same line into the same frame last, the block
// starts with it stored (nothing but the function's own code writes its
// frames' lines). A store marked with its line (inline.enter's, into the frame
// below, and a link's line 0) is one stored already.
void CodeGen::placeLines() {
  struct Stored {
    uint32_t line = 0;
    Value *frame = nullptr; // null: not known
    bool operator==(const Stored &) const = default;
  };
  // The line a marked instruction stores, in which frame; nothing for a call that does not see the call stack.
  auto lineOf = [&](Instruction &inst) -> std::optional<Stored> {
    MDNode *md = inst.getMetadata(lineKind_);
    if (!md) return std::nullopt;
    if (auto *call = dyn_cast<CallInst>(&inst)) {
      const Function *callee = call->getCalledFunction();
      if (callee && (callee->isIntrinsic() || quietCalls_.count(callee))) return std::nullopt;
    }
    auto in = callFrame_.find(&inst);
    return Stored{(uint32_t)mdconst::extract<ConstantInt>(md->getOperand(0))->getZExtValue(),
                  in == callFrame_.end() ? fnFrame_ : in->second};
  };
  // What each block stores last (none: what it starts with), then what each starts with: the same from every way
  // into it, else nothing known; an optimistic fixed point, the blocks not reached yet left out.
  std::unordered_map<const BasicBlock *, std::optional<Stored>> last;
  for (BasicBlock &bb : *fn_)
    for (Instruction &inst : bb)
      if (std::optional<Stored> s = lineOf(inst)) last[&bb] = s;
  std::unordered_map<const BasicBlock *, Stored> atStart, atEnd;
  for (bool changed = true; changed;) {
    changed = false;
    for (BasicBlock &bb : *fn_) {
      std::optional<Stored> in;
      bool entry = &bb == &fn_->getEntryBlock();
      if (entry) in = Stored{};
      for (BasicBlock *p : predecessors(&bb)) {
        auto e = atEnd.find(p);
        if (e == atEnd.end()) continue;
        in = !in ? e->second : *in == e->second ? *in : Stored{};
      }
      if (!in) continue;
      auto l = last.find(&bb);
      Stored out = l != last.end() ? *l->second : *in;
      auto s = atStart.find(&bb);
      if (s != atStart.end() && s->second == *in && atEnd.at(&bb) == out) continue;
      atStart[&bb] = *in;
      atEnd[&bb] = out;
      changed = true;
    }
  }
  for (BasicBlock &bb : *fn_) {
    Stored stored = atStart.count(&bb) ? atStart.at(&bb) : Stored{};
    for (Instruction &inst : bb) {
      std::optional<Stored> s = lineOf(inst);
      inst.setMetadata(lineKind_, nullptr);
      if (!s) continue;
      auto *call = dyn_cast<CallInst>(&inst);
      if (call && !(*s == stored)) {
        Value *at = GetElementPtrInst::CreateInBounds(frameType(0), s->frame,
                                                      {ConstantInt::get(i32_, 0), ConstantInt::get(i32_, FrameLine)},
                                                      "line", call->getIterator());
        tag(new StoreInst(ConstantInt::get(i32_, s->line), at, call->getIterator()), Mem::FrameLink);
      }
      stored = *s;
    }
  }
}

void CodeGen::pushFrame(uint32_t nslots, uint32_t nparams, Value *argc, Value *argv, Constant *proc, bool keepArgs) {
  StructType *ty = frameType(nslots);
  BasicBlock &entry = fn_->getEntryBlock(); // a static alloca, whatever block the frame is linked in
  AllocaInst *frame = IRBuilder<>(&entry, entry.getFirstInsertionPt()).CreateAlloca(ty, nullptr, "frame");
  fnBase_ = b_.CreateConstInBoundsGEP2_32(ty, frame, 0, FrameSlots, "slots");
  // In a direct body, the items in registers follow the context, the result's item (unless it returns words) and the
  // caller's frame.
  unsigned first = fnWords_ ? 2 : 3;
  for (uint32_t k = 0; k < nparams; ++k) {
    const xir::Var *v = plan_->param(k);
    if (!v) report_fatal_error("xbc: a parameter has no variable");
    Value *dst = rawAddress(*v); // its slot, or its item out of the frame (FramePlan::inRegisters)
    if (fnDirect_) storeWords(dst, fn_->getArg(first + 2 * k), fn_->getArg(first + 1 + 2 * k));
    else copyItem(dst, argItem(argc, argv, k));
  }
  uint32_t inFrame = plan_->paramSlots();
  if (nslots > inFrame)
    b_.CreateMemSet(slot(inFrame), b_.getInt8(0), (uint64_t)(nslots - inFrame) * itemSize_, itemAlign_);
  if (keepArgs) { // only what PCount() and PValue() read; other frames leave the two fields unset
    tag(b_.CreateStore(argv, b_.CreateStructGEP(ty, frame, FrameArgv)), Mem::FrameLink);
    tag(b_.CreateStore(argc, b_.CreateStructGEP(ty, frame, FrameArgc)), Mem::FrameLink);
  }
  linkFrame(frame, ConstantInt::get(i32_, nslots), proc, fnPrev_);
  fnFrame_ = frame;
}

// The header of a frame whose slots are valid (`nslots`, `proc`, line 0), and
// the frame linked into the context's chain.
void CodeGen::linkFrame(Value *frame, Value *nslots, Value *proc, Value *prev) {
  StructType *ty = frameType(0);
  tag(b_.CreateStore(nslots, b_.CreateStructGEP(ty, frame, FrameCount)), Mem::FrameLink);
  tag(b_.CreateStore(proc, b_.CreateStructGEP(ty, frame, FrameProc)), Mem::FrameLink);
  StoreInst *line = tag(b_.CreateStore(ConstantInt::get(i32_, 0), b_.CreateStructGEP(ty, frame, FrameLine)),
                        Mem::FrameLink);
  line->setMetadata(lineKind_, MDNode::get(ctx_, ConstantAsMetadata::get(ConstantInt::get(i32_, 0))));
  callFrame_[line] = frame; // the frame holds line 0 now (placeLines)
  Value *frames = b_.CreateStructGEP(ctxHeadTy_, fnCtx_, CtxFrames);
  if (!prev) prev = loadFrom(ptrTy_, frames, Mem::Ctx, "prev");
  tag(b_.CreateStore(prev, b_.CreateStructGEP(ty, frame, FramePrev)), Mem::FrameLink);
  tag(b_.CreateStore(frame, frames), Mem::Ctx);
}

// A frame below the context's stack limit goes on when the limit was only the mark of a deep stack (xb_stack_deep);
// else it raises the stack error (xb_stack_overflow) before anything is written into it, and the code goes on at
// `exit`, unwinding as after any error (with the stack restored to `stackSave`, when given, first). The call on the
// way back into the body is declared to touch the context alone, and passes the frame's address as a number: what the
// function knows of the rest of memory, and its frame, stay known (the function inlines as without the mark).
void CodeGen::checkStack(Value *frame, BasicBlock *exit, Value *stackSave) {
  Value *limit = loadFrom(ptrTy_, b_.CreateStructGEP(ctxHeadTy_, fnCtx_, CtxStackLimit), Mem::Ctx, "stack.limit");
  Value *at = b_.CreatePtrToInt(frame, intPtrTy_);
  Value *below = b_.CreateICmpULT(at, b_.CreatePtrToInt(limit, intPtrTy_));
  BasicBlock *join = beginUnlikely(below, "stack.below");
  Value *deep = b_.CreateCall(rtStackDeep_, {fnCtx_, at});
  BasicBlock *raise = newBlock("stack.overflow");
  b_.CreateCondBr(b_.CreateICmpNE(deep, ConstantInt::get(i32_, 0)), join, raise);
  b_.SetInsertPoint(raise);
  if (stackSave) b_.CreateStackRestore(stackSave);
  b_.CreateCall(rtStackOverflow_, {fnCtx_});
  b_.CreateBr(exit);
  b_.SetInsertPoint(join);
}

void CodeGen::unlinkFrame(Value *frame, Value *prev) {
  if (!prev) prev = loadFrom(ptrTy_, b_.CreateStructGEP(frameType(0), frame, FramePrev), Mem::FrameLink, "prev");
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
  Constant *memvars = memvarTable_ ? static_cast<Constant *>(memvarTable_) : ConstantPointerNull::get(ptrTy_);
  // The module's level (XB_LEVEL_PROGRAM in xbrt.h): the runtime's 0, a library's its own (setLibrary).
  Constant *level = ConstantInt::get(i32_, runtimeLibrary_ ? 0 : library_.empty() ? XB_LEVEL_PROGRAM : libraryLevel_);
  if (classTable_)
    b_.CreateCall(rtClassesRegister_, {classTable_, ConstantInt::get(i32_, m.classes.size()), level});
  b_.CreateCall(rtModuleRegister_, {init, statics, ConstantInt::get(i32_, m.statics.size()), table,
                                    ConstantInt::get(i32_, m.initProcs.size()),
                                    ConstantInt::get(i32_, m.exitProcs.size()), symbolTable,
                                    ConstantInt::get(i32_, symbols.size()), memvars,
                                    ConstantInt::get(i32_, memvars_.size()), level});
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
  uint64_t strHdr = dl_.getStructLayout(strType(ArrayType::get(i8_, 0)))->getElementOffset(StrBytes);
  uint64_t blockCells = dl_.getStructLayout(blockTy_)->getElementOffset(2);
  uint64_t frameSlots = dl_.getStructLayout(frameType(0))->getElementOffset(FrameSlots);
  uint64_t ctxFrames = dl_.getStructLayout(ctxHeadTy_)->getElementOffset(CtxFrames);
  uint64_t ctxDecimals = dl_.getStructLayout(ctxHeadTy_)->getElementOffset(CtxDecimals);
  uint64_t ctxStackLimit = dl_.getStructLayout(ctxHeadTy_)->getElementOffset(CtxStackLimit);
  uint64_t ctxLent = dl_.getStructLayout(ctxHeadTy_)->getElementOffset(CtxLent);
  uint64_t arrayData = dl_.getStructLayout(arrayTy_)->getElementOffset(ArrayData);
  uint64_t refValue = dl_.getStructLayout(refTy_)->getElementOffset(RefValue);
  uint64_t hashEntries = dl_.getStructLayout(hashTy_)->getElementOffset(HashEntries);
  uint64_t objectVars = dl_.getStructLayout(objectTy_)->getElementOffset(ObjectVars);
  uint64_t strBufBytes = dl_.getStructLayout(strBufType())->getElementOffset(StrBufBytes);
  b_.CreateCall(rtAbiCheck_, {ConstantInt::get(i32_, XB_ABI_VERSION), ConstantInt::get(i32_, itemSize_),
                              ConstantInt::get(i32_, strHdr), ConstantInt::get(i32_, blockCells),
                              ConstantInt::get(i32_, frameSlots), ConstantInt::get(i32_, ctxFrames),
                              ConstantInt::get(i32_, ctxDecimals), ConstantInt::get(i32_, ctxStackLimit),
                              ConstantInt::get(i32_, ctxLent),
                              ConstantInt::get(i32_, arrayData),
                              ConstantInt::get(i32_, refValue), ConstantInt::get(i32_, hashEntries),
                              ConstantInt::get(i32_, objectVars), ConstantInt::get(i32_, strBufBytes)});
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
  if (auto w = wordsAt_.find(item); w != wordsAt_.end()) return w->second.header;
  return tag(b_.CreateAlignedLoad(i64_, item, itemAlign_, "header"), itemMemory(item));
}

// Right after the definition of `v` (or at the start of the block of a
// parameter): the words of its slot, for the reads of it (Words in CodeGen.h).
// Unused ones go away in LLVM.
void CodeGen::cacheWords(const xir::Value *v) {
  if (v->type() != xir::Type::Item) return;
  const Home &h = plan_->home(v);
  if (h.kind == Home::Kind::Slot) words_[v] = loadWords(slot(h.slot));
  else if (h.kind == Home::Kind::Place && h.place == v) words_[v] = loadWords(values_.at(v)); // a load served in place
}

// The two words of an item in memory, as they are there.
CodeGen::Words CodeGen::loadWords(Value *item) {
  Mem m = itemMemory(item);
  Value *header = tag(b_.CreateAlignedLoad(i64_, item, itemAlign_, "header"), m);
  Value *payload = tag(b_.CreateAlignedLoad(i64_, b_.CreateStructGEP(itemTy_, item, FValue), itemAlign_, "int"), m);
  return {header, payload};
}

// The words of an item: those its value's definition left in registers when addr() gave this address for it
// (wordsAt_), else read from memory.
CodeGen::Words CodeGen::itemWords(Value *item) {
  if (auto w = wordsAt_.find(item); w != wordsAt_.end()) return w->second;
  return loadWords(item);
}

// Argument `k` as received: argv[k], or a NIL item past argc.
Value *CodeGen::argItem(Value *argc, Value *argv, uint32_t k) {
  Value *given = b_.CreateICmpULT(ConstantInt::get(i32_, k), argc);
  return b_.CreateSelect(given, b_.CreateConstInBoundsGEP1_32(itemTy_, argv, k), nilItem_);
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

// Between items in memory the value word is read by an unordered atomic load where 64 bits are a native integer of the
// target (a plain load there): code generation merges a pair of plain loads of consecutive words into one 16-byte load,
// which spans the two 8-byte stores that usually wrote the item and cannot be forwarded from them (qsort: 4.5 million
// blocked forwards, linked_list 11 million). On a 32-bit target the load is a plain one (two 32-bit loads): an atomic
// one would call the atomics library (32-bit PowerPC, MIPS, RISC-V; on x86, whose items are 4-byte aligned, a call that
// locks a mutex: matmul 17 times slower) or take an exclusive access (ARM). A Local item becomes registers. A literal's
// words are constants, stored as such: a choice between two literals is then one of constants, not of loads (which code
// generation would turn into branches).
void CodeGen::copyItem(Value *dst, Value *src) {
  if (dst == src) return;
  if (auto lit = literalWords_.find(src); lit != literalWords_.end()) {
    storeWords(dst, lit->second.first, lit->second.second);
    return;
  }
  bool registers = localItems_.count(dst) || localItems_.count(src);
  Mem m = itemMemory(src);
  Value *header = tag(b_.CreateAlignedLoad(i64_, src, itemAlign_), m);
  LoadInst *payload = tag(b_.CreateAlignedLoad(i64_, b_.CreateStructGEP(itemTy_, src, FValue), itemAlign_), m);
  if (!registers && dl_.isLegalInteger(64)) payload->setAtomic(AtomicOrdering::Unordered);
  storeWords(dst, header, payload);
}

void CodeGen::setNil(Value *dst) { storeWords(dst, headerWord(XB_T_NIL, 0, 0), ConstantInt::get(i64_, 0)); }

// An item of type `tag`, no width or decimals of its own, holding `v`.
void CodeGen::storeTagged(Value *dst, uint32_t tag, Value *v) {
  storeWords(dst, headerWord(tag, 0, 0), payloadWord(v));
}

void CodeGen::storeLogical(Value *dst, Value *bit) { storeTagged(dst, XB_T_LOGICAL, b_.CreateZExt(bit, i32_)); }

// The test of the context's pending requests, a load and a well-predicted branch: when something is pending,
// xb_pending_gc serves a collection another thread makes (a safepoint) and destructors, and says whether to unwind;
// after an operation whose substituted result waits in a home the collector does not see (unless `collects`: a poll
// on a back edge), xb_pending, which never collects nor parks. The load is atomic (monotonic: another thread sets a
// safepoint request there), so a loop without calls keeps it.
void CodeGen::checkPending(BasicBlock *unwind, bool collects) {
  Value *pending = loadFrom(i32_, b_.CreateStructGEP(ctxHeadTy_, fnCtx_, CtxPending), Mem::Ctx, "pending");
  cast<LoadInst>(pending)->setAtomic(AtomicOrdering::Monotonic);
  Value *isSet = b_.CreateICmpNE(pending, ConstantInt::get(i32_, 0));
  BasicBlock *slow = newBlock("pending");
  BasicBlock *cont = newBlock("ok");
  MDBuilder md(ctx_);
  b_.CreateCondBr(isSet, slow, cont, md.createUnlikelyBranchWeights());
  b_.SetInsertPoint(slow);
  // A result the error block may have substituted, in a home the collector does not see: the test must not collect
  // (xbrt.h), so destructors wait for a later one.
  bool held = !collects && emitting_ && (emitting_->effects() & xir::Substitutes) &&
              plan_->home(emitting_).kind == Home::Kind::Local;
  // What the loop polls pass (LoopPolls.h) sees: a back edge's poll, or a test that serves a safepoint.
  if (collects || !held)
    cast<LoadInst>(pending)->setMetadata(collects ? kPollMetadata : kSafepointMetadata, MDNode::get(ctx_, {}));
  Value *leave = b_.CreateCall(held ? rtPending_ : rtPendingGc_, {fnCtx_});
  b_.CreateCondBr(b_.CreateICmpNE(leave, ConstantInt::get(i32_, 0)), unwind, cont);
  b_.SetInsertPoint(cont);
}

// A loop's back edge polls the pending requests, so a loop without calls still reaches a safepoint when another
// thread collects.
void CodeGen::pollOnBackEdge(const xir::BasicBlock *from, const xir::BasicBlock *to) {
  if (blockOrder_.at(to) <= blockOrder_.at(from)) checkPending(unwind_, true);
}

const xir::Function *CodeGen::sourceOf(const llvm::Function *f) const {
  for (const auto *map : {&functions_, &direct_})
    for (auto &[xf, lf] : *map)
      if (lf == f) return xf;
  return nullptr;
}

} // namespace xb
