// Constant item and string literals, emitted as static data.
#include "codegen/CodeGen.h"

#include "xbrt.h" // constants only (type tags, flags)

#include <llvm/IR/Constants.h>

#include <cstring>

using namespace llvm;

namespace xb {

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

} // namespace xb
