#include "CodeGen.h"

#include "xbrt.h" // constants only (type tags, ABI version) - never layouts

#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/MDBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <cstring>

using namespace llvm;

namespace xb {

namespace {
enum ItemField : unsigned { FType = 0, FWidth = 1, FDec = 2, FValue = 3 };

bool hasSideEffects(const Expr &e) {
  switch (e.kind) {
  case ExprKind::Call: case ExprKind::Assign: case ExprKind::IncDec:
    return true;
  case ExprKind::Unary:
    return hasSideEffects(*static_cast<const UnaryExpr &>(e).operand);
  case ExprKind::Binary: {
    auto &b = static_cast<const BinaryExpr &>(e);
    return hasSideEffects(*b.lhs) || hasSideEffects(*b.rhs);
  }
  case ExprKind::And: case ExprKind::Or: {
    auto &b = static_cast<const LogicExpr &>(e);
    return hasSideEffects(*b.lhs) || hasSideEffects(*b.rhs);
  }
  case ExprKind::IIf: {
    auto &i = static_cast<const IIfExpr &>(e);
    return hasSideEffects(*i.cond) || hasSideEffects(*i.then) || hasSideEffects(*i.otherwise);
  }
  default:
    return false;
  }
}
} // namespace

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

  Type *voidTy = Type::getVoidTy(ctx_);
  rtFramePush_ = rt("xb_frame_push", ptrTy_, {ptrTy_, i32_});
  rtFramePop_ = rt("xb_frame_pop", voidTy, {ptrTy_, ptrTy_});
  rtArgsCopy_ = rt("xb_args_copy", voidTy, {ptrTy_, i32_, i32_, ptrTy_});
  rtCondError_ = rt("xb_cond_error", i32_, {ptrTy_, ptrTy_});
  rtForTest_ = rt("xb_for_test", i32_, {ptrTy_, ptrTy_, ptrTy_, ptrTy_});
  rtAbiCheck_ = rt("xb_abi_check", voidTy, {i32_, i32_, i32_});
  rtNeg_ = rt("xb_op_neg", voidTy, {ptrTy_, ptrTy_, ptrTy_});
  rtNot_ = rt("xb_op_not", voidTy, {ptrTy_, ptrTy_, ptrTy_});
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

Function *CodeGen::declareXbFunc(const std::string &xbName, bool internal) {
  std::string sym = mangle(xbName);
  Function *f = mod_.getFunction(sym);
  if (!f) {
    f = Function::Create(xbFuncTy_, internal ? GlobalValue::InternalLinkage : GlobalValue::ExternalLinkage, sym, mod_);
    addAbiAttributes(f);
  }
  return f;
}

Constant *CodeGen::literalItem(uint32_t type, uint16_t width, uint16_t dec, Constant *payload) {
  Type *pt = payload->getType();
  std::vector<Type *> elems{i32_, i16_, i16_, pt};
  std::vector<Constant *> vals{ConstantInt::get(i32_, type), ConstantInt::get(i16_, width),
                               ConstantInt::get(i16_, dec), payload};
  uint64_t psz = dl_.getTypeAllocSize(pt);
  if (psz < unionSize_) {
    auto *padTy = ArrayType::get(i8_, unionSize_ - psz);
    elems.push_back(padTy);
    vals.push_back(ConstantAggregateZero::get(padTy));
  }
  auto *st = StructType::get(ctx_, elems);
  if (dl_.getTypeAllocSize(st) != itemSize_ ||
      dl_.getStructLayout(st)->getElementOffset(FValue) != dl_.getStructLayout(itemTy_)->getElementOffset(FValue))
    report_fatal_error("xbc: literal item layout does not match xb_item");
  auto *gv = new GlobalVariable(mod_, st, true, GlobalValue::PrivateLinkage, ConstantStruct::get(st, vals), ".xb.lit");
  gv->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
  gv->setAlignment(itemAlign_);
  return gv;
}

Constant *CodeGen::nilLiteral() {
  if (!nilLit_) nilLit_ = literalItem(XB_T_NIL, 0, 0, ConstantInt::get(i64_, 0));
  return nilLit_;
}

Constant *CodeGen::logicalLiteral(bool v) {
  Constant *&slotRef = v ? trueLit_ : falseLit_;
  if (!slotRef) slotRef = literalItem(XB_T_LOGICAL, 0, 0, ConstantInt::get(i32_, v ? 1 : 0));
  return slotRef;
}

Constant *CodeGen::intLiteral(int64_t v) {
  std::string key = "i" + std::to_string(v);
  Constant *&c = numLits_[key];
  if (!c) c = literalItem(XB_T_INT, 0, 0, ConstantInt::get(i64_, (uint64_t)v, true));
  return c;
}

Constant *CodeGen::numberLiteral(const NumberExpr &n) {
  if (n.isInt) {
    std::string key = "i" + std::to_string(n.ival) + ":" + std::to_string(n.width);
    Constant *&c = numLits_[key];
    if (!c) c = literalItem(XB_T_INT, n.width, 0, ConstantInt::get(i64_, (uint64_t)n.ival, true));
    return c;
  }
  uint64_t bits;
  std::memcpy(&bits, &n.dval, sizeof bits);
  std::string key = "d" + std::to_string(bits) + ":" + std::to_string(n.dec) + ":" + std::to_string(n.width);
  Constant *&c = numLits_[key];
  if (!c) c = literalItem(XB_T_DOUBLE, n.width, n.dec, ConstantFP::get(Type::getDoubleTy(ctx_), n.dval));
  return c;
}

// Strings are emitted as static xb_str objects (header + bytes + NUL), so a
// literal costs nothing at run time and is never touched by the GC.
// xb_str: { flags, hash, len, data[] } with the bytes typed as `data`.
StructType *CodeGen::strType(Type *data) { return StructType::get(ctx_, {i32_, i32_, intPtrTy_, data}); }

Constant *CodeGen::stringLiteral(const std::string &s) {
  Constant *&c = strLits_[s];
  if (c) return c;
  auto *data = ConstantDataArray::getString(ctx_, s, /*AddNull=*/true);
  StructType *st = strType(data->getType());
  auto *init = ConstantStruct::get(st, {ConstantInt::get(i32_, XB_STR_STATIC), ConstantInt::get(i32_, 0),
                                        ConstantInt::get(intPtrTy_, s.size()), data});
  auto *str = new GlobalVariable(mod_, st, true, GlobalValue::PrivateLinkage, init, ".xb.str");
  str->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
  str->setAlignment(dl_.getABITypeAlign(st));
  c = literalItem(XB_T_STRING, 0, 0, str);
  return c;
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

// One load + one well-predicted branch per fallible call.
void CodeGen::checkPending() {
  Value *pending = b_.CreateLoad(i32_, fnCtx_, "pending");
  Value *isSet = b_.CreateICmpNE(pending, ConstantInt::get(i32_, 0));
  BasicBlock *cont = newBlock("ok");
  MDBuilder md(ctx_);
  b_.CreateCondBr(isSet, fnExit_, cont, md.createUnlikelyBranchWeights());
  b_.SetInsertPoint(cont);
}

void CodeGen::startDeadBlock() { b_.SetInsertPoint(newBlock("dead")); }

bool CodeGen::run(const Module &m, bool emitEntry) {
  for (auto &f : m.funcs) funcs_[f->name] = declareXbFunc(f->name, f->isStatic);
  for (auto &f : m.funcs) genFunc(*f);
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

  numLocals_ = f.numLocals;
  temps_ = 0;
  maxSlots_ = numLocals_;
  loops_.clear();

  fnPush_ = b_.CreateCall(rtFramePush_, {fnCtx_, ConstantInt::get(i32_, 0)}, "frame");
  fnBase_ = fnPush_;
  if (!f.params.empty())
    b_.CreateCall(rtArgsCopy_, {fnBase_, ConstantInt::get(i32_, f.params.size()), argc, argv});

  fnExit_ = BasicBlock::Create(ctx_, "exit", fn_);
  genBlock(f.body);
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
  b_.CreateCall(rtAbiCheck_, {ConstantInt::get(i32_, XB_ABI_VERSION), ConstantInt::get(i32_, itemSize_),
                              ConstantInt::get(i32_, strHdr)});
  Value *ret = b_.CreateCall(rtFramePush_, {ctx, ConstantInt::get(i32_, 1)});
  b_.CreateCall(xbFuncTy_, funcs_[start.name], {ctx, ret, ConstantInt::get(i32_, 0), ConstantPointerNull::get(ptrTy_)});
  b_.CreateCall(rtFramePop_, {ctx, ret});
  b_.CreateRetVoid();
}

void CodeGen::genBlock(const Block &blk) {
  for (auto &s : blk) {
    uint32_t mark = temps_;
    genStmt(*s);
    temps_ = mark;
  }
}

void CodeGen::genStmt(const Stmt &s) {
  switch (s.kind) {
  case StmtKind::Expr:
    genDiscard(*static_cast<const ExprStmt &>(s).expr);
    break;

  case StmtKind::Local:
    for (const LocalDecl &d : static_cast<const LocalStmt &>(s).decls)
      if (d.init) genExpr(*d.init, slot(d.slot));
    break;

  case StmtKind::Return: {
    auto &r = static_cast<const ReturnStmt &>(s);
    if (r.value) genExpr(*r.value, fnRet_);
    b_.CreateBr(fnExit_);
    startDeadBlock();
    break;
  }

  case StmtKind::If: {
    auto &st = static_cast<const IfStmt &>(s);
    BasicBlock *end = newBlock("endif");
    for (auto &arm : st.arms) {
      Value *c = genCond(*arm.cond);
      BasicBlock *then = newBlock("then");
      BasicBlock *next = newBlock("else");
      b_.CreateCondBr(c, then, next);
      b_.SetInsertPoint(then);
      genBlock(arm.body);
      if (!b_.GetInsertBlock()->getTerminator()) b_.CreateBr(end);
      b_.SetInsertPoint(next);
    }
    genBlock(st.elseBody);
    if (!b_.GetInsertBlock()->getTerminator()) b_.CreateBr(end);
    end->moveAfter(b_.GetInsertBlock());
    b_.SetInsertPoint(end);
    break;
  }

  case StmtKind::While: {
    auto &st = static_cast<const WhileStmt &>(s);
    BasicBlock *cond = newBlock("while.cond");
    BasicBlock *body = newBlock("while.body");
    BasicBlock *end = newBlock("while.end");
    b_.CreateBr(cond);
    b_.SetInsertPoint(cond);
    b_.CreateCondBr(genCond(*st.cond), body, end);
    b_.SetInsertPoint(body);
    loops_.push_back({end, cond});
    genBlock(st.body);
    loops_.pop_back();
    if (!b_.GetInsertBlock()->getTerminator()) b_.CreateBr(cond);
    end->moveAfter(b_.GetInsertBlock());
    b_.SetInsertPoint(end);
    break;
  }

  case StmtKind::For: {
    // The TO and STEP expressions are re-evaluated on every iteration, as in
    // Clipper.
    auto &st = static_cast<const ForStmt &>(s);
    Value *var = varSlot(*st.var);
    genExpr(*st.start, var);
    BasicBlock *test = newBlock("for.test");
    BasicBlock *body = newBlock("for.body");
    BasicBlock *incr = newBlock("for.incr");
    BasicBlock *end = newBlock("for.end");
    b_.CreateBr(test);

    b_.SetInsertPoint(test);
    uint32_t mark = temps_;
    Value *limit = genOperand(*st.limit);
    if (st.step && hasSideEffects(*st.step)) {
      Value *t = tempSlot(); // evaluating STEP must not change the limit being tested
      copyItem(t, limit);
      limit = t;
    }
    Value *step = st.step ? genOperand(*st.step) : intLiteral(1);
    Value *go = b_.CreateCall(rtForTest_, {fnCtx_, var, limit, step});
    checkPending();
    b_.CreateCondBr(b_.CreateICmpNE(go, ConstantInt::get(i32_, 0)), body, end);
    temps_ = mark;

    b_.SetInsertPoint(body);
    loops_.push_back({end, incr});
    genBlock(st.body);
    loops_.pop_back();
    if (!b_.GetInsertBlock()->getTerminator()) b_.CreateBr(incr);

    incr->moveAfter(b_.GetInsertBlock());
    b_.SetInsertPoint(incr);
    step = st.step ? genOperand(*st.step) : intLiteral(1);
    genBinary(BinOp::Add, var, var, step);
    temps_ = mark;
    b_.CreateBr(test);

    end->moveAfter(b_.GetInsertBlock());
    b_.SetInsertPoint(end);
    break;
  }

  case StmtKind::Exit:
  case StmtKind::Loop:
    b_.CreateBr(s.kind == StmtKind::Exit ? loops_.back().exitTo : loops_.back().loopTo);
    startDeadBlock();
    break;
  }
}

Value *CodeGen::varSlot(const Expr &e) {
  auto &v = static_cast<const VarExpr &>(e);
  return slot(v.slot);
}

// Returns a pointer to an item holding the value of `e`, without copying
// when the value already lives somewhere (a variable or a literal).
Value *CodeGen::genOperand(const Expr &e) {
  switch (e.kind) {
  case ExprKind::Nil: return nilLiteral();
  case ExprKind::Logical: return logicalLiteral(static_cast<const LogicalExpr &>(e).value);
  case ExprKind::Number: return numberLiteral(static_cast<const NumberExpr &>(e));
  case ExprKind::String: return stringLiteral(static_cast<const StringExpr &>(e).value);
  case ExprKind::Var: return varSlot(e);
  default: {
    Value *t = tempSlot();
    genExpr(e, t);
    return t;
  }
  }
}

// Left-to-right evaluation: if evaluating the right side could change a
// variable used on the left (`x + (x := 5)`), snapshot the left first.
std::pair<Value *, Value *> CodeGen::genOperands(const Expr &lhs, const Expr &rhs) {
  Value *a = genOperand(lhs);
  if (lhs.kind == ExprKind::Var && hasSideEffects(rhs)) {
    Value *t = tempSlot();
    copyItem(t, a);
    a = t;
  }
  Value *b = genOperand(rhs);
  return {a, b};
}

// Runtime operators accept a result pointer that aliases an operand.
void CodeGen::genBinary(BinOp op, Value *dst, Value *a, Value *b) {
  b_.CreateCall(rtBin_[op], {fnCtx_, dst, a, b});
  checkPending();
}

Value *CodeGen::testTruth(Value *item) {
  Value *type = b_.CreateLoad(i32_, b_.CreateStructGEP(itemTy_, item, FType), "type");
  BasicBlock *ok = newBlock("cond.ok");
  BasicBlock *bad = newBlock("cond.bad");
  MDBuilder md(ctx_);
  b_.CreateCondBr(b_.CreateICmpEQ(type, ConstantInt::get(i32_, XB_T_LOGICAL)), ok, bad, md.createLikelyBranchWeights());
  b_.SetInsertPoint(bad);
  b_.CreateCall(rtCondError_, {fnCtx_, item});
  b_.CreateBr(fnExit_);
  b_.SetInsertPoint(ok);
  Value *v = b_.CreateLoad(i32_, b_.CreateStructGEP(itemTy_, item, FValue), "lval");
  return b_.CreateICmpNE(v, ConstantInt::get(i32_, 0));
}

Value *CodeGen::genCond(const Expr &e) {
  switch (e.kind) {
  case ExprKind::Logical:
    return b_.getInt1(static_cast<const LogicalExpr &>(e).value);
  case ExprKind::And:
  case ExprKind::Or: {
    auto &l = static_cast<const LogicExpr &>(e);
    bool isAnd = e.kind == ExprKind::And;
    Value *lv = genCond(*l.lhs);
    BasicBlock *lhsEnd = b_.GetInsertBlock();
    BasicBlock *rhsBB = newBlock(isAnd ? "and.rhs" : "or.rhs");
    BasicBlock *join = newBlock(isAnd ? "and.end" : "or.end");
    if (isAnd) b_.CreateCondBr(lv, rhsBB, join);
    else b_.CreateCondBr(lv, join, rhsBB);
    b_.SetInsertPoint(rhsBB);
    Value *rv = genCond(*l.rhs);
    BasicBlock *rhsEnd = b_.GetInsertBlock();
    b_.CreateBr(join);
    b_.SetInsertPoint(join);
    PHINode *phi = b_.CreatePHI(b_.getInt1Ty(), 2);
    phi->addIncoming(b_.getInt1(!isAnd), lhsEnd);
    phi->addIncoming(rv, rhsEnd);
    return phi;
  }
  case ExprKind::Unary:
    if (static_cast<const UnaryExpr &>(e).op == UnOp::Not)
      return b_.CreateNot(genCond(*static_cast<const UnaryExpr &>(e).operand));
    [[fallthrough]];
  default:
    return testTruth(genOperand(e));
  }
}

void CodeGen::genDiscard(const Expr &e) {
  switch (e.kind) {
  case ExprKind::Assign:
    genAssign(static_cast<const AssignExpr &>(e), nullptr);
    break;
  case ExprKind::IncDec:
    genIncDec(static_cast<const IncDecExpr &>(e), nullptr);
    break;
  default:
    genExpr(e, tempSlot());
    break;
  }
}

void CodeGen::genExpr(const Expr &e, Value *dst) {
  switch (e.kind) {
  case ExprKind::Nil:
    setNil(dst);
    break;
  case ExprKind::Logical:
  case ExprKind::Number:
  case ExprKind::String:
  case ExprKind::Var:
    copyItem(dst, genOperand(e));
    break;
  case ExprKind::Call:
    genCall(static_cast<const CallExpr &>(e), dst);
    break;
  case ExprKind::Unary: {
    auto &u = static_cast<const UnaryExpr &>(e);
    Value *a = genOperand(*u.operand);
    switch (u.op) {
    case UnOp::Neg:
      b_.CreateCall(rtNeg_, {fnCtx_, dst, a});
      checkPending();
      break;
    case UnOp::Not:
      b_.CreateCall(rtNot_, {fnCtx_, dst, a});
      checkPending();
      break;
    case UnOp::Plus:
      copyItem(dst, a);
      break;
    }
    break;
  }
  case ExprKind::Binary: {
    auto &bin = static_cast<const BinaryExpr &>(e);
    auto [a, b] = genOperands(*bin.lhs, *bin.rhs);
    genBinary(bin.op, dst, a, b);
    break;
  }
  case ExprKind::And:
  case ExprKind::Or:
    storeLogical(dst, genCond(e));
    break;
  case ExprKind::Assign:
    genAssign(static_cast<const AssignExpr &>(e), dst);
    break;
  case ExprKind::IncDec:
    genIncDec(static_cast<const IncDecExpr &>(e), dst);
    break;
  case ExprKind::IIf:
    genIIf(static_cast<const IIfExpr &>(e), dst);
    break;
  }
}

void CodeGen::genCall(const CallExpr &c, Value *dst) {
  // Arguments go into consecutive slots of the caller's frame; the callee
  // receives (argc, argv) pointing at them.
  uint32_t n = (uint32_t)c.args.size();
  uint32_t first = n ? allocTemps(n) : 0;
  for (uint32_t i = 0; i < n; ++i) genExpr(*c.args[i], slot(first + i));
  Value *argv = n ? slot(first) : ConstantPointerNull::get(ptrTy_);

  auto it = funcs_.find(c.name);
  Function *callee = it != funcs_.end() ? it->second : declareXbFunc(c.name, false);
  setNil(dst); // the caller clears the result, so callees that return nothing need not
  b_.CreateCall(xbFuncTy_, callee, {fnCtx_, dst, ConstantInt::get(i32_, n), argv});
  checkPending();
}

void CodeGen::genAssign(const AssignExpr &a, Value *dst) {
  Value *target = varSlot(*a.target);
  if (a.compound) {
    auto [l, r] = genOperands(*a.target, *a.value);
    genBinary(a.op, target, l, r);
  } else if (hasSideEffects(*a.value)) {
    // The value may change the target while it is computed (`x := x++`): the
    // assignment stores the value as computed, after those side effects.
    Value *t = tempSlot();
    genExpr(*a.value, t);
    copyItem(target, t);
  } else {
    genExpr(*a.value, target);
  }
  if (dst) copyItem(dst, target);
}

void CodeGen::genIncDec(const IncDecExpr &e, Value *dst) {
  Value *target = varSlot(*e.target);
  if (dst && !e.prefix) copyItem(dst, target);
  genBinary(e.increment ? BinOp::Add : BinOp::Sub, target, target, intLiteral(1));
  if (dst && e.prefix) copyItem(dst, target);
}

void CodeGen::genIIf(const IIfExpr &e, Value *dst) {
  Value *c = genCond(*e.cond);
  BasicBlock *thenBB = newBlock("iif.then");
  BasicBlock *elseBB = newBlock("iif.else");
  BasicBlock *end = newBlock("iif.end");
  b_.CreateCondBr(c, thenBB, elseBB);
  b_.SetInsertPoint(thenBB);
  genExpr(*e.then, dst);
  b_.CreateBr(end);
  b_.SetInsertPoint(elseBB);
  genExpr(*e.otherwise, dst);
  b_.CreateBr(end);
  b_.SetInsertPoint(end);
}

} // namespace xb
