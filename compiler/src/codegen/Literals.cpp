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
  if (Constant *word = constantPayloadWord(payload)) literalWords_[gv] = {headerWord(type, width, dec), word};
  return gv;
}

// The value word of an item holding the constant `payload` (as payloadWord makes it), or null where it is no constant:
// a pointer narrower than the word, which only an instruction widens.
Constant *CodeGen::constantPayloadWord(Constant *payload) {
  if (auto *fp = dyn_cast<ConstantFP>(payload))
    return ConstantInt::get(i64_, fp->getValueAPF().bitcastToAPInt().getZExtValue());
  if (auto *ci = dyn_cast<ConstantInt>(payload)) {
    unsigned bits = ci->getBitWidth();
    return ConstantInt::get(i64_, ci->getValue().zext(64).getZExtValue() << valueShift(bits));
  }
  if (payload->getType()->isPointerTy() && intPtrTy_ == i64_) return ConstantExpr::getPtrToInt(payload, i64_);
  return nullptr;
}

// The static item of an xIR item constant; equal constants share one.
Constant *CodeGen::literal(const xir::Constant &c) {
  switch (c.lit) {
  case xir::Constant::Lit::Nil:
    if (!nilLit_) nilLit_ = literalItem(XB_T_NIL, 0, 0, ConstantInt::get(i64_, 0));
    return nilLit_;
  case xir::Constant::Lit::Logical: {
    Constant *&lit = c.logical ? trueLit_ : falseLit_;
    if (!lit) lit = literalItem(XB_T_LOGICAL, 0, 0, ConstantInt::get(i32_, c.logical ? 1 : 0));
    return lit;
  }
  case xir::Constant::Lit::Integer: {
    std::string key = "i" + std::to_string(c.ival) + ":" + std::to_string(c.width);
    Constant *&lit = numLits_[key];
    if (!lit) lit = literalItem(XB_T_INT, c.width, 0, ConstantInt::get(i64_, (uint64_t)c.ival, true));
    return lit;
  }
  case xir::Constant::Lit::Double: {
    uint64_t bits;
    std::memcpy(&bits, &c.dval, sizeof bits);
    std::string key = "d" + std::to_string(bits) + ":" + std::to_string(c.dec) + ":" + std::to_string(c.width);
    Constant *&lit = numLits_[key];
    if (!lit) lit = literalItem(XB_T_DOUBLE, c.width, c.dec, ConstantFP::get(Type::getDoubleTy(ctx_), c.dval));
    return lit;
  }
  case xir::Constant::Lit::String: return stringLiteral(c.str);
  case xir::Constant::Lit::Bool:
  case xir::Constant::Lit::I64: break; // machine constants are not items
  }
  report_fatal_error("xbc: not an item constant");
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
  // The hash is computed here, as the runtime does (it cannot store it in a constant).
  auto *init = ConstantStruct::get(st, {ConstantInt::get(i32_, XB_STR_STATIC),
                                        ConstantInt::get(i32_, xb_hash_bytes(s.data(), s.size())),
                                        ConstantInt::get(intPtrTy_, s.size()), data});
  auto *str = new GlobalVariable(mod_, st, true, GlobalValue::PrivateLinkage, init, ".xb.str");
  str->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
  str->setAlignment(dl_.getABITypeAlign(st));
  c = literalItem(XB_T_STRING, 0, 0, str);
  return c;
}

// The constant item of a function reference (@name(), XB_T_SYMBOL): one per function, pointing at its xb_funcref
// { name, entry, native, byref }.
Constant *CodeGen::funcRef(const xir::Callee &c) {
  Function *fn = c.function ? functions_.at(c.function) : externFunction(c);
  Constant *&item = funcRefs_[fn];
  if (item) return item;
  const std::string &name = c.function ? c.function->name : c.name;
  auto *refTy = StructType::get(ctx_, {ptrTy_, ptrTy_, i32_, i32_});
  auto *init = ConstantStruct::get(refTy, {messageName(name), fn, ConstantInt::get(i32_, c.native ? 1 : 0),
                                           ConstantInt::get(i32_, c.byref)});
  auto *ref = new GlobalVariable(mod_, refTy, true, GlobalValue::PrivateLinkage, init, ".xb.funcref");
  ref->setAlignment(dl_.getABITypeAlign(refTy));
  item = literalItem(XB_T_SYMBOL, 0, 0, ref);
  return item;
}

// A message name (or a field's) as the C string xb_op_send (or xb_field_get) receives.
Constant *CodeGen::messageName(const std::string &message) {
  Constant *&c = messages_[message];
  if (c) return c;
  auto *data = ConstantDataArray::getString(ctx_, message, /*AddNull=*/true);
  auto *name = new GlobalVariable(mod_, data->getType(), true, GlobalValue::PrivateLinkage, data, ".xb.msg");
  name->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
  name->setAlignment(Align(1));
  c = name;
  return c;
}

// A work area's alias as xb_field_get receives it: NULL for the current work area.
Constant *CodeGen::aliasName(const std::string &alias) {
  return alias.empty() ? static_cast<Constant *>(ConstantPointerNull::get(ptrTy_)) : messageName(alias);
}

// A field reference's own cache (xb_field_cache: one 64-bit word the runtime fills, read and written atomically, so
// aligned to 8 bytes on every target).
Constant *CodeGen::fieldCache() {
  auto *cache = new GlobalVariable(mod_, i64_, false, GlobalValue::PrivateLinkage, ConstantInt::get(i64_, 0),
                                   "xb.field.cache");
  cache->setAlignment(Align(alignof(xb_field_cache)));
  return cache;
}

} // namespace xb
