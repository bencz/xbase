// Calls to a few runtime functions done in place when their arguments allow
// it, with the functions' own rules (rtl/lang.c, rtl/strings.c): Len() of a
// string or an array, Asc() of a string, Chr() of a number and SubStr( c, n, 1 )
// with n within c (both give the shared one-byte string, xb_str_bytes), and
// Asc( SubStr( c, n, 1 ) ) right after each other (the byte itself, without a
// string). Anything else calls the function, which raises the errors. Also
// shareString, which ends the in-place growth of a string an `append` made.
#include "codegen/CodeGen.h"

#include "xbrt.h" // type tags, xb_str_bytes

#include <llvm/IR/Constants.h>

#include <algorithm>

using namespace llvm;

namespace xb {

namespace {

enum class Inline { None, Len, Asc, Chr, SubStr, PCount, PValue, Int, ValType };

Inline inlineKind(const xir::Instruction &i) {
  if (i.op != xir::Op::Call || i.callee.function || !i.callee.native || i.hasRest()) return Inline::None;
  static const struct {
    const char *name;
    size_t argc;
    Inline kind;
  } table[] = {
      {"LEN", 1, Inline::Len},       {"ASC", 1, Inline::Asc},          {"CHR", 1, Inline::Chr},
      {"SUBSTR", 3, Inline::SubStr}, {"PCOUNT", 0, Inline::PCount}, {"PVALUE", 1, Inline::PValue},
      {"INT", 1, Inline::Int},       {"VALTYPE", 1, Inline::ValType},
  };
  for (const auto &e : table)
    if (i.callee.name == e.name && i.operands().size() == e.argc) return e.kind;
  return Inline::None;
}

// A load whose value goes only to Len() and Asc() calls, which read a string
// in place and keep nothing: it need not end a string's growth.
bool readOnly(const xir::Instruction &load) {
  return !load.users().empty() && std::all_of(load.users().begin(), load.users().end(), [](const xir::Instruction *u) {
    Inline k = inlineKind(*u);
    return k == Inline::Len || k == Inline::Asc;
  });
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

Value *CodeGen::isGrowing(Value *flags) {
  return b_.CreateICmpNE(b_.CreateAnd(flags, ConstantInt::get(i32_, XB_STR_APPEND)), ConstantInt::get(i32_, 0));
}

// A string an `append` made may grow in place only while its variable is
// the one holding it: when the item is such a string (XB_STR_APPEND), the
// flag goes and `hash`, which held the room, is 0 again (not computed), as
// xb_str_share does; the string is immutable from then on.
void CodeGen::shareString(Value *item) {
  BasicBlock *str = newBlock("share.str"), *clear = newBlock("share"), *done = newBlock("share.done");
  b_.CreateCondBr(b_.CreateICmpEQ(itemType(item), ConstantInt::get(i32_, XB_T_STRING)), str, done);
  b_.SetInsertPoint(str);
  StructType *ty = strType(ArrayType::get(i8_, 0));
  Value *s = itemPointer(item, "str");
  Value *flagsAt = b_.CreateStructGEP(ty, s, StrFlags);
  Value *flags = loadFrom(i32_, flagsAt, Mem::String, "flags");
  b_.CreateCondBr(isGrowing(flags), clear, done);
  b_.SetInsertPoint(clear);
  tag(b_.CreateStore(b_.CreateAnd(flags, ConstantInt::get(i32_, ~XB_STR_APPEND)), flagsAt), Mem::String);
  tag(b_.CreateStore(ConstantInt::get(i32_, 0), b_.CreateStructGEP(ty, s, StrHash)), Mem::String);
  b_.CreateBr(done);
  b_.SetInsertPoint(done);
}

// `append`: a string the variable grows (XB_STR_APPEND) takes a string's
// bytes in place when they fit its room (`hash`), as xb_op_append does;
// anything else calls it.
void CodeGen::emitAppend(const xir::Instruction &i) {
  Value *var = varAddress(*i.var, i.access, unwind_);
  Value *y = addr(i.operand(0));
  StructType *ty = strType(ArrayType::get(i8_, 0));
  BasicBlock *strings = newBlock("append.strings"), *fits = newBlock("append.fits"), *one = newBlock("append.byte");
  BasicBlock *many = newBlock("append.bytes"), *end = newBlock("append.end"), *slow = newBlock("append.call");
  BasicBlock *join = newBlock("join");
  Value *str = ConstantInt::get(i32_, XB_T_STRING);
  branchLikely(b_.CreateAnd(b_.CreateICmpEQ(itemType(var), str), b_.CreateICmpEQ(typeOf(i.operand(0), y), str)),
               strings, slow);
  b_.SetInsertPoint(strings);
  Value *s = itemPointer(var, "str"), *t = itemPointer(y, "str");
  Value *flags = loadFrom(i32_, b_.CreateStructGEP(ty, s, StrFlags), Mem::String, "flags");
  Value *room = b_.CreateZExt(loadFrom(i32_, b_.CreateStructGEP(ty, s, StrHash), Mem::String, "room"), i64_);
  Value *len = strLength(s), *more = strLength(t);
  branchLikely(b_.CreateAnd(isGrowing(flags), b_.CreateICmpULE(more, b_.CreateSub(room, len))), fits, slow);
  b_.SetInsertPoint(fits);
  Value *data = b_.CreateStructGEP(ty, s, StrData);
  Value *tail = b_.CreateInBoundsGEP(i8_, data, b_.CreateZExtOrTrunc(len, intPtrTy_));
  Value *from = b_.CreateStructGEP(ty, t, StrData);
  b_.CreateCondBr(b_.CreateICmpEQ(more, ConstantInt::get(i64_, 1)), one, many);
  b_.SetInsertPoint(one);
  tag(b_.CreateStore(loadFrom(i8_, from, Mem::String, "byte"), tail), Mem::String);
  b_.CreateBr(end);
  b_.SetInsertPoint(many);
  b_.CreateMemCpy(tail, MaybeAlign(1), from, MaybeAlign(1), b_.CreateZExtOrTrunc(more, intPtrTy_));
  b_.CreateBr(end);
  b_.SetInsertPoint(end);
  Value *newLen = b_.CreateAdd(len, more);
  tag(b_.CreateStore(b_.CreateZExtOrTrunc(newLen, intPtrTy_), b_.CreateStructGEP(ty, s, StrLen)), Mem::String);
  Value *nul = b_.CreateInBoundsGEP(i8_, data, b_.CreateZExtOrTrunc(newLen, intPtrTy_));
  tag(b_.CreateStore(ConstantInt::get(i8_, 0), nul), Mem::String);
  b_.CreateBr(join);
  b_.SetInsertPoint(slow);
  b_.CreateCall(rtAppend_, {fnCtx_, var, y});
  checkPending(unwind_);
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
}

// The loads that must end the growth: those an `append` of their variable
// reaches with no load in between (one that did has ended it already). A
// forward data flow over the blocks; a block that operations unwind to may
// be entered from the middle of another, so every variable may be growing
// there. A load whose value only Len() and Asc() read keeps the growth. A
// block reached growing along some of its edges only, from which every path
// ends the growth before any append (a loop that reads what a loop before it
// built), ends it on those edges instead (shareOnEdge_), once, rather than at
// every load: ending it earlier is safe, and costs nothing there.
void CodeGen::findSharingLoads(const xir::Function &f, const xir::CFG &cfg) {
  sharingLoads_.clear();
  shareOnEdge_.clear();
  std::unordered_map<const xir::Var *, size_t> bit;
  std::unordered_set<const xir::BasicBlock *> unwindTargets;
  for (const auto &b : f.blocks) {
    if (b->unwind) unwindTargets.insert(b->unwind);
    for (const auto &i : b->insts)
      if (i->op == xir::Op::Append && !bit.count(i->var)) bit.emplace(i->var, bit.size());
  }
  if (bit.empty()) return;
  using Growing = std::vector<bool>;
  std::unordered_map<const xir::BasicBlock *, Growing> out;
  auto transfer = [&](const xir::BasicBlock &b, Growing g, bool record) {
    for (const auto &i : b.insts) {
      auto it = i->var ? bit.find(i->var) : bit.end();
      if (it == bit.end()) continue;
      if (i->op == xir::Op::Append) {
        g[it->second] = true;
      } else if (i->op == xir::Op::Load && g[it->second] && !readOnly(*i)) {
        if (record) sharingLoads_.insert(i.get());
        g[it->second] = false;
      }
    }
    return g;
  };
  auto in = [&](const xir::BasicBlock *b) {
    Growing g(bit.size(), unwindTargets.count(b) != 0);
    for (const xir::BasicBlock *p : cfg.preds(b)) {
      auto o = out.find(p);
      if (o == out.end()) continue;
      Growing along = o->second;
      auto edge = shareOnEdge_.find({p, b});
      if (edge != shareOnEdge_.end())
        for (const xir::Var *v : edge->second) along[bit.at(v)] = false;
      for (size_t k = 0; k < g.size(); ++k) g[k] = g[k] || along[k];
    }
    return g;
  };
  auto settle = [&] {
    out.clear();
    for (bool changed = true; changed;) { // the sets only grow
      changed = false;
      for (const xir::BasicBlock *b : cfg.order()) {
        Growing g = transfer(*b, in(b), false);
        auto &o = out[b];
        if (o != g) {
          o = std::move(g);
          changed = true;
        }
      }
    }
  };
  settle();
  // Whether, from a block's start, every path ends the growth of each variable (a load that is not read-only) or
  // leaves the function before an append grows it: there, ending it earlier costs nothing. A backward data flow, from
  // "everywhere" down to the fixpoint; an unwind edge counts as a path.
  std::unordered_map<const xir::BasicBlock *, Growing> ends;
  auto endsAt = [&](const xir::BasicBlock &b) {
    Growing e(bit.size(), true);
    for (size_t k = 0; k < b.numSuccessors(); ++k) {
      auto it = ends.find(b.successor(k));
      if (it != ends.end())
        for (size_t v = 0; v < e.size(); ++v) e[v] = e[v] && it->second[v];
    }
    for (auto i = b.insts.rbegin(); i != b.insts.rend(); ++i) {
      auto it = (*i)->var ? bit.find((*i)->var) : bit.end();
      if (it == bit.end()) continue;
      if ((*i)->op == xir::Op::Append) e[it->second] = false;
      else if ((*i)->op == xir::Op::Load && !readOnly(**i)) e[it->second] = true;
    }
    return e;
  };
  for (const xir::BasicBlock *b : cfg.order()) ends[b] = Growing(bit.size(), true);
  for (bool changed = true; changed;) { // the sets only shrink
    changed = false;
    for (auto b = cfg.order().rbegin(); b != cfg.order().rend(); ++b) {
      Growing e = endsAt(**b);
      if (e != ends[*b]) {
        ends[*b] = std::move(e);
        changed = true;
      }
    }
  }
  for (const xir::BasicBlock *b : cfg.order()) {
    if (unwindTargets.count(b) || cfg.preds(b).size() < 2) continue;
    Growing g = in(b);
    for (const auto &[var, k] : bit) {
      if (!g[k] || !ends.at(b)[k]) continue;
      bool someNot = std::any_of(cfg.preds(b).begin(), cfg.preds(b).end(), [&](const xir::BasicBlock *p) {
        auto o = out.find(p);
        return o != out.end() && !o->second[k];
      });
      if (!someNot) continue;
      for (const xir::BasicBlock *p : cfg.preds(b)) {
        auto o = out.find(p);
        if (o != out.end() && o->second[k]) shareOnEdge_[{p, b}].push_back(var);
      }
    }
  }
  if (!shareOnEdge_.empty()) settle();
  for (const xir::BasicBlock *b : cfg.order()) transfer(*b, in(b), true);
}

// The growth ends on the edge `from` -> `to` (findSharingLoads).
void CodeGen::shareOnEdge(const xir::BasicBlock *from, const xir::BasicBlock *to) {
  auto edge = shareOnEdge_.find({from, to});
  if (edge == shareOnEdge_.end()) return;
  for (const xir::Var *v : edge->second) shareString(varAddress(*v, xir::Access::Direct, unwind_));
}

bool CodeGen::emitInlineCall(const xir::Instruction &i) {
  Inline kind = inlineKind(i);
  // In an inlined body, the arguments are its frame's: the runtime reads them (PValue(), whose function keeps its
  // parameters in its frame slots, is never inlined).
  if (kind == Inline::None || (!inlineOpen_.empty() && kind == Inline::PCount)) return false;
  if (kind == Inline::Asc && fusedCalls_.count(&i)) return true; // lowered with its SubStr()
  const xir::Instruction *asc = nullptr;                        // the Asc() lowered with this SubStr()
  if (kind == Inline::SubStr && i.hasOneUse() && fusedCalls_.count(i.users().front()))
    asc = static_cast<const xir::Instruction *>(i.users().front());
  Value *d = dest(i);
  Value *ascDest = asc ? dest(*asc) : nullptr; // computed here, where both paths start
  auto arg = [&](unsigned n) { return addr(i.operand(n)); }; // read in place: the argument group is for the call
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
  case Inline::ValType: { // the shared one-byte string of the tag's letter (XB_TYPE_LETTERS); a reference or a memo
    BasicBlock *fast = newBlock("valtype"); // field's value (XB_STR_MEMO): the runtime
    Value *t = argType(0);
    Value *known = b_.CreateICmpULT(t, ConstantInt::get(i32_, XB_T_COUNT_));
    Value *width = b_.CreateTrunc(b_.CreateLShr(loadHeader(arg(0)), headerShift(FWidth)), i16_, "width");
    Value *memo = b_.CreateAnd(isType(t, XB_T_STRING), b_.CreateICmpEQ(width, ConstantInt::get(i16_, XB_STR_MEMO)));
    branchLikely(b_.CreateAnd(b_.CreateAnd(known, b_.CreateNot(memo)), b_.CreateNot(holdsRef(arg(0)))), fast, slow);
    b_.SetInsertPoint(fast);
    auto *letters = ConstantDataArray::getString(ctx_, XB_TYPE_LETTERS, false);
    GlobalVariable *table = mod_.getNamedGlobal("xb.type.letters");
    if (!table)
      table = new GlobalVariable(mod_, letters->getType(), true, GlobalValue::PrivateLinkage, letters,
                                 "xb.type.letters");
    Value *letter = b_.CreateLoad(i8_, b_.CreateInBoundsGEP(letters->getType(), table,
                                                            {ConstantInt::get(i64_, 0), b_.CreateZExt(t, i64_)}));
    storeTagged(d, XB_T_STRING, byteString(b_.CreateZExt(letter, i64_)));
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
  case Inline::PCount: { // the function's own count (the slow path is never taken); a method always has Self
    storeInt(d, b_.CreateSub(b_.CreateZExt(fn_->getArg(2), i64_), ConstantInt::get(i64_, fnSkip_)));
    break;
  }
  case Inline::PValue: { // PValue( n ), n an integer: the parameter's slot or the argument as passed, NIL outside
    BasicBlock *check = newBlock("pvalue.check"), *in = newBlock("pvalue.in"), *nil = newBlock("pvalue.nil");
    BasicBlock *copy = newBlock("pvalue.copy");
    branchLikely(isType(argType(0), XB_T_INT), check, slow);
    b_.SetInsertPoint(check);
    Value *n = intValue(arg(0));
    Value *k = b_.CreateAdd(b_.CreateSub(n, ConstantInt::get(i64_, 1)), ConstantInt::get(i64_, fnSkip_));
    branchLikely(b_.CreateAnd(b_.CreateICmpSGE(n, ConstantInt::get(i64_, 1)),
                              b_.CreateICmpULT(k, b_.CreateZExt(fn_->getArg(2), i64_))),
                 in, nil);
    b_.SetInsertPoint(nil);
    setNil(d);
    b_.CreateBr(join);
    b_.SetInsertPoint(in);
    Value *param = b_.CreateICmpULT(k, ConstantInt::get(i64_, fnParams_));
    Value *src = b_.CreateSelect(param, b_.CreateInBoundsGEP(itemTy_, fnBase_, k),
                                 b_.CreateInBoundsGEP(itemTy_, fn_->getArg(3), k), "pvalue.src");
    Value *t = itemType(src);
    branchLikely(b_.CreateAnd(b_.CreateICmpNE(t, ConstantInt::get(i32_, XB_T_CELL)),
                              b_.CreateICmpNE(t, ConstantInt::get(i32_, XB_T_REF))),
                 copy, slow); // through a reference: the runtime reads it
    b_.SetInsertPoint(copy);
    copyItem(d, src);
    break;
  }
  case Inline::Int: { // an integer as it is; a double truncated, an integer when it fits 64 bits (xb_dbl_fits_i64)
    // with the double's width, its own or else the default one (xb_num_dbl_width); the rest (NaN too): the runtime
    BasicBlock *isInt = newBlock("int.int"), *other = newBlock("int.other"), *dbl = newBlock("int.double");
    BasicBlock *fits = newBlock("int.fits");
    Value *t = argType(0);
    branchLikely(isType(t, XB_T_INT), isInt, other);
    b_.SetInsertPoint(isInt);
    copyItem(d, arg(0));
    b_.CreateBr(join);
    b_.SetInsertPoint(other);
    Value *widthMask = ConstantInt::get(i64_, (uint64_t)0xFFFF << headerShift(FWidth));
    Value *own = b_.CreateAnd(loadHeader(arg(0)), widthMask, "width");
    branchLikely(isType(t, XB_T_DOUBLE), dbl, slow);
    b_.SetInsertPoint(dbl);
    Type *f64 = b_.getDoubleTy();
    Value *v = b_.CreateBitCast(intValue(arg(0)), f64);
    auto widthWord = [&](uint64_t w) { return ConstantInt::get(i64_, w << headerShift(FWidth)); };
    Value *wide = b_.CreateOr(b_.CreateFCmpOGT(v, ConstantFP::get(f64, (double)XB_NUM_FITS_MAX)),
                              b_.CreateFCmpOLT(v, ConstantFP::get(f64, (double)XB_NUM_FITS_MIN)));
    Value *width = b_.CreateSelect(b_.CreateICmpNE(own, ConstantInt::get(i64_, 0)), own,
                                   b_.CreateSelect(wide, widthWord(XB_NUM_WIDE_WIDTH), widthWord(XB_NUM_WIDTH)));
    Value *tr = b_.CreateUnaryIntrinsic(Intrinsic::trunc, v);
    Value *inRange = b_.CreateAnd(b_.CreateFCmpOGE(tr, ConstantFP::get(f64, -9223372036854775808.0)),
                                  b_.CreateFCmpOLT(tr, ConstantFP::get(f64, 9223372036854775808.0)));
    branchLikely(inRange, fits, slow); // NaN and the rest: the runtime keeps them doubles
    b_.SetInsertPoint(fits);
    storeWords(d, b_.CreateOr(headerWord(XB_T_INT, 0, 0), width), b_.CreateFPToSI(tr, i64_));
    break;
  }
  case Inline::None: break;
  }
  b_.CreateBr(join);
  b_.SetInsertPoint(slow);
  Value *argv = operandGroup(i);
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
