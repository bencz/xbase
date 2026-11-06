// Calls to a few runtime functions done in place when their arguments allow
// it, with the functions' own rules (rtl/lang.c, rtl/strings.c): Len() of a
// string or an array, Asc() of a string, Chr() of a number and SubStr( c, n, 1 )
// with n within c (both give the shared one-byte string, xb_str_bytes), and
// Asc( SubStr( c, n, 1 ) ) right after each other (the byte itself, without a
// string). Anything else calls the function, which raises the errors.
#include "codegen/CodeGen.h"

#include "xbrt.h" // type tags, xb_str_bytes

#include <llvm/IR/Constants.h>

using namespace llvm;

namespace xb {

namespace {

enum class Inline { None, Len, Asc, Chr, SubStr };

Inline inlineKind(const xir::Instruction &i) {
  if (i.op != xir::Op::Call || i.callee.function || !i.callee.native) return Inline::None;
  static const struct {
    const char *name;
    size_t argc;
    Inline kind;
  } table[] = {
      {"LEN", 1, Inline::Len}, {"ASC", 1, Inline::Asc}, {"CHR", 1, Inline::Chr}, {"SUBSTR", 3, Inline::SubStr},
  };
  for (const auto &e : table)
    if (i.callee.name == e.name && i.operands().size() == e.argc) return e.kind;
  return Inline::None;
}

} // namespace

// An Asc() call right after the SubStr() call that is its only argument and
// the SubStr() result's only use: the pair is lowered as one.
void CodeGen::findFusedCalls(const xir::BasicBlock &b) {
  fusedCalls_.clear();
  for (size_t k = 0; k + 1 < b.insts.size(); ++k) {
    const xir::Instruction &i = *b.insts[k], &next = *b.insts[k + 1];
    if (inlineKind(i) == Inline::SubStr && inlineKind(next) == Inline::Asc && i.hasOneUse() &&
        i.users().front() == &next)
      fusedCalls_.insert(&next);
  }
}

Value *CodeGen::strLength(Value *str) {
  StructType *ty = strType(ArrayType::get(i8_, 0));
  return b_.CreateZExtOrTrunc(loadFrom(intPtrTy_, b_.CreateStructGEP(ty, str, StrLen), Mem::String, "len"), i64_);
}

// The byte at 0-based `pos` (i64) of a string, as an i64 from 0 to 255.
Value *CodeGen::strByte(Value *str, Value *pos) {
  StructType *ty = strType(ArrayType::get(i8_, 0));
  Value *data = b_.CreateStructGEP(ty, str, StrData);
  Value *at = b_.CreateInBoundsGEP(i8_, data, b_.CreateZExtOrTrunc(pos, intPtrTy_));
  return b_.CreateZExt(loadFrom(i8_, at, Mem::String, "byte"), i64_);
}

// The shared string of the byte `c` (an i64 from 0 to 255).
Value *CodeGen::byteString(Value *c) {
  ArrayType *table = ArrayType::get(strType(ArrayType::get(i8_, 2)), 256);
  if (!strBytes_)
    strBytes_ = new GlobalVariable(mod_, table, true, GlobalValue::ExternalLinkage, nullptr, "xb_str_bytes");
  return b_.CreateInBoundsGEP(table, strBytes_, {ConstantInt::get(i64_, 0), c});
}

bool CodeGen::emitInlineCall(const xir::Instruction &i) {
  Inline kind = inlineKind(i);
  if (kind == Inline::None) return false;
  if (kind == Inline::Asc && fusedCalls_.count(&i)) return true; // lowered with its SubStr()
  const xir::Instruction *asc = nullptr;                        // the Asc() lowered with this SubStr()
  if (kind == Inline::SubStr && i.hasOneUse() && fusedCalls_.count(i.users().front()))
    asc = static_cast<const xir::Instruction *>(i.users().front());
  Value *argv = operandGroup(i);
  Value *d = dest(i);
  Value *ascDest = asc ? dest(*asc) : nullptr; // computed here, where both paths start
  uint32_t base = plan_->groupBase(&i);
  auto arg = [&](unsigned n) { return slot(base + n); };
  auto argType = [&](unsigned n) { return typeOf(i.operand(n), arg(n)); };
  auto isType = [&](Value *type, uint32_t tag) { return b_.CreateICmpEQ(type, ConstantInt::get(i32_, tag)); };
  BasicBlock *slow = newBlock("call"), *join = newBlock("join");
  switch (kind) {
  case Inline::Len: {
    BasicBlock *str = newBlock("len.str"), *other = newBlock("len.other"), *arr = newBlock("len.array");
    Value *t = argType(0);
    b_.CreateCondBr(isType(t, XB_T_STRING), str, other);
    b_.SetInsertPoint(str);
    storeInt(d, strLength(itemPointer(arg(0), "str")));
    b_.CreateBr(join);
    b_.SetInsertPoint(other);
    branchLikely(isType(t, XB_T_ARRAY), arr, slow);
    b_.SetInsertPoint(arr);
    storeInt(d, arrayLength(itemPointer(arg(0), "arr")));
    break;
  }
  case Inline::Asc: { // an empty string reads its terminating NUL: 0
    BasicBlock *fast = newBlock("asc");
    branchLikely(isType(argType(0), XB_T_STRING), fast, slow);
    b_.SetInsertPoint(fast);
    storeInt(d, strByte(itemPointer(arg(0), "str"), ConstantInt::get(i64_, 0)));
    break;
  }
  case Inline::Chr: { // the number's low byte, as xb_num_to_i64 takes it
    BasicBlock *fast = newBlock("chr");
    Value *t = argType(0);
    branchLikely(isNumberType(t), fast, slow);
    b_.SetInsertPoint(fast);
    storeTagged(d, XB_T_STRING, byteString(b_.CreateAnd(numberAsInteger(arg(0), t), ConstantInt::get(i64_, 0xFF))));
    break;
  }
  case Inline::SubStr: { // SubStr( c, n, 1 ) with 1 <= n <= Len( c )
    BasicBlock *check = newBlock("substr.check"), *fast = newBlock("substr");
    Value *ok = b_.CreateAnd(isType(argType(0), XB_T_STRING),
                             b_.CreateAnd(isType(argType(1), XB_T_INT), isType(argType(2), XB_T_INT)));
    branchLikely(ok, check, slow);
    b_.SetInsertPoint(check);
    Value *s = itemPointer(arg(0), "str");
    Value *pos = b_.CreateSub(intValue(arg(1)), ConstantInt::get(i64_, 1));
    Value *one = b_.CreateICmpEQ(intValue(arg(2)), ConstantInt::get(i64_, 1));
    branchLikely(b_.CreateAnd(one, b_.CreateICmpULT(pos, strLength(s))), fast, slow);
    b_.SetInsertPoint(fast);
    Value *byte = strByte(s, pos);
    if (asc) storeInt(ascDest, byte);
    else storeTagged(d, XB_T_STRING, byteString(byte));
    break;
  }
  case Inline::None: break;
  }
  b_.CreateBr(join);
  b_.SetInsertPoint(slow);
  setNil(d);
  b_.CreateCall(xbFuncTy_, externFunction(i.callee), {fnCtx_, d, ConstantInt::get(i32_, i.operands().size()), argv});
  checkPending(unwind_);
  if (asc) {
    Value *ascArgv = operandGroup(*asc);
    setNil(ascDest);
    b_.CreateCall(xbFuncTy_, externFunction(asc->callee), {fnCtx_, ascDest, ConstantInt::get(i32_, 1), ascArgv});
    checkPending(unwind_);
  }
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
  return true;
}

} // namespace xb
