// Where variables are: frame slots, cells of captured variables, STATICs, FOR EACH references; write barriers.
#include "codegen/CodeGen.h"
#include "codegen/LoopSplit.h"

#include "xbrt.h" // constants only (type tags, GC state bits)
#include "xir/Types.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/MDBuilder.h>

using namespace llvm;

namespace xb {

// A new address at every call: wordsAt_ keys on it (one read of one value).
Value *CodeGen::slot(uint32_t idx) { return b_.CreateConstInBoundsGEP1_32(itemTy_, fnBase_, idx); }

Value *CodeGen::staticAddress(uint32_t idx) {
  return b_.CreateConstInBoundsGEP2_32(statics_->getValueType(), statics_, 0, idx);
}

Value *CodeGen::capturedCellAddress(Value *block, uint32_t index) {
  return b_.CreateInBoundsGEP(blockTy_, block, {b_.getInt32(0), b_.getInt32(2), b_.getInt32(index)});
}

// The cell of a codeblock's capture. A cell's value is at offset 0, so the
// cell pointer is the item pointer.
Value *CodeGen::capturedCell(const xir::Var &v) {
  return heapItem(loadFrom(ptrTy_, capturedCellAddress(fnSelf_, v.index), Mem::BlockCells, "cell"));
}

// The variable's own item: its frame slot (or, for a parameter kept out of the
// frame, an item of the function's own: FramePlan::inRegisters) or STATIC
// storage (which holds its cell once it is shared), or a capture's cell;
// nothing is followed.
Value *CodeGen::rawAddress(const xir::Var &v) {
  switch (v.kind) {
  case xir::Var::Kind::Static: return staticAddress(v.index);
  case xir::Var::Kind::Capture: return capturedCell(v);
  case xir::Var::Kind::Frame: {
    if (!plan_->inRegisters(v)) return slot(plan_->slotOf(v));
    AllocaInst *&item = paramItems_[&v];
    if (!item) {
      item = entryAlloca("param");
      localItems_.insert(item);
    }
    return item;
  }
  }
  return nullptr;
}

// The GC state byte of heap object `obj` (xbrt.h), loaded atomically with `order`.
LoadInst *CodeGen::stateByte(Value *obj, AtomicOrdering order) {
  Value *back = ConstantInt::getSigned(i64_, -static_cast<int64_t>(XB_GC_STATE_BACK));
  auto *state = cast<LoadInst>(loadFrom(i8_, b_.CreateGEP(i8_, obj, back), Mem::GcState, "gc.state"));
  state->setAtomic(order);
  return state;
}

// Whether the program's storage is still the first thread's alone (xb_storage_shared clear, xbrt.h): set while every
// other thread is stopped, so what this thread read holds until it calls something.
Value *CodeGen::storageLocal() {
  auto *flag = cast<LoadInst>(loadFrom(i32_, storageShared_, Mem::Unknown, "storage.shared"));
  flag->setAtomic(AtomicOrdering::Unordered);
  return b_.CreateICmpEQ(flag, ConstantInt::get(i32_, 0));
}

// Whether the heap object `obj` is local, not shared (XB_GC_STATE_SHARED in its header's state byte, xbrt.h). An
// unordered atomic load: other threads may set the byte's other bits, and only the thread that reaches a local object
// can share it, so what this thread read holds until it calls something that could. For an object an item holds
// (`fromItem`: `obj` is the item's pointer word), the load says where the item's header word and type are
// (kStateLoad, for versionInvariantColdExits). An array's form, in the same byte, changes only in the runtime too.
Value *CodeGen::isLocal(Value *obj, bool fromItem) { return isLocalState(objectState(obj, fromItem)); }

LoadInst *CodeGen::objectState(Value *obj, bool fromItem) {
  LoadInst *state = stateByte(obj, AtomicOrdering::Unordered);
  if (fromItem) {
    int64_t header = -static_cast<int64_t>(dl_.getStructLayout(itemTy_)->getElementOffset(FValue));
    Metadata *where[] = {ConstantAsMetadata::get(ConstantInt::getSigned(i64_, header)),
                         ConstantAsMetadata::get(ConstantInt::get(i64_, headerShift(FType)))};
    state->setMetadata(kStateLoad, MDNode::get(ctx_, where));
  }
  return state;
}

// A local array of items (XB_ELEMS_ITEMS): one test of its state byte.
Value *CodeGen::isLocalItems(Value *state) {
  return b_.CreateICmpEQ(b_.CreateAnd(state, ConstantInt::get(i8_, XB_GC_STATE_SHARED | XB_GC_STATE_ELEMS)),
                         ConstantInt::get(i8_, 0));
}

// An array's form (XB_ELEMS_ITEMS ...), from its state byte.
Value *CodeGen::elemsOf(Value *state) {
  return b_.CreateAnd(b_.CreateLShr(state, ConstantInt::get(i8_, XB_GC_STATE_ELEMS_SHIFT)),
                      ConstantInt::get(i8_, XB_GC_STATE_ELEMS_MASK));
}

Value *CodeGen::isLocalState(Value *state) {
  return b_.CreateICmpEQ(b_.CreateAnd(state, ConstantInt::get(i8_, XB_GC_STATE_SHARED)), ConstantInt::get(i8_, 0));
}

// The barrier's test (xb_gc_write in xbrt.h) on the state byte `state` of `obj`, read before its store: an old object
// not yet recorded is recorded.
void CodeGen::recordIfOld(Value *obj, Value *state) {
  BasicBlock *join = beginUnlikely(needsRecord(state), "gc.old");
  b_.CreateCall(rtGcRemember_, {obj});
  endUnlikely(join);
}

// xb_gc_needs_record (xbrt.h) on a state byte: the object is old and not recorded yet.
Value *CodeGen::needsRecord(Value *state) {
  Value *bits = b_.CreateAnd(state, ConstantInt::get(i8_, XB_GC_STATE_OLD | XB_GC_STATE_RECORDED));
  return b_.CreateICmpEQ(bits, ConstantInt::get(i8_, XB_GC_STATE_OLD));
}

// Whether `v` may refer to a heap object, as far as type inference knows: a store of it needs the write barrier.
bool CodeGen::mayReferToHeap(const xir::Value *v) {
  xir::TypeSet t = plan_->types().of(v);
  return t == 0 || (t & xir::kHeapTypes) != 0;
}

// Where a load (`load`) or store of the variable reaches its value (see xir::Access). `local` is the item in place,
// where the builder is left: the variable's own item, the value of a local cell that holds no element (a cell's value
// is at offset 0), or for a load of a FOR EACH variable its local loop reference's element in a local collection or
// own item; null when there is none (a store into a FOR EACH variable's element). The rest (a shared cell, a shared
// object, another element) goes to `shared`, a new empty block for the runtime's call (xb_ref_load, xb_ref_store),
// which takes the item's `type` (XB_T_CELL or XB_T_REF) and `ptr`.
CodeGen::VarTarget CodeGen::varTarget(const xir::Var &v, xir::Access a, bool load) {
  VarTarget t;
  Value *storageType = nullptr; // a STATIC: every runtime path is its own item's (XB_T_NIL), from wherever it starts
  if (v.kind == xir::Var::Kind::Static) {
    t.shared = newBlock("var.shared");
    t.type = storageType = ConstantInt::get(i32_, XB_T_NIL);
    t.ptr = rawAddress(v);
    BasicBlock *local = newBlock("static.local");
    branchLikely(storageLocal(), local, t.shared); // shared storage once a second thread runs (xb_storage_shared)
    b_.SetInsertPoint(local);
  }
  auto runtimeArgs = [&](Value *type, Value *ptr) {
    if (storageType) return;
    t.type = type;
    t.ptr = ptr;
  };
  if (a == xir::Access::Direct) {
    t.local = rawAddress(v);
    return t;
  }
  if (!t.shared) t.shared = newBlock("var.shared");
  if (a == xir::Access::ThroughRef) {
    Value *item = rawAddress(v);
    Value *itemTy = itemType(item), *ref = itemPointer(item, "ref");
    runtimeArgs(itemTy, ref);
    if (!load) { // a store into the element: the runtime's, which makes its write barrier
      b_.CreateBr(t.shared);
      return t;
    }
    // A read of a local loop reference's element in place: of a local collection (inlineRefTarget), or its own item
    // (a string's character, an enumerator's Current). A variable in a cell, and the rest, read through the runtime.
    BasicBlock *isRef = newBlock("each.ref"), *local = newBlock("each.local"), *inColl = newBlock("each.coll");
    BasicBlock *own = newBlock("each.own"), *join = newBlock("each.var");
    branchLikely(b_.CreateICmpEQ(itemTy, ConstantInt::get(i32_, XB_T_REF)), isRef, t.shared);
    b_.SetInsertPoint(isRef);
    branchLikely(isLocal(ref), local, t.shared);
    b_.SetInsertPoint(local);
    Value *collType = itemType(heapItem(b_.CreateStructGEP(refTy_, ref, RefColl)));
    b_.CreateCondBr(b_.CreateOr(b_.CreateICmpEQ(collType, ConstantInt::get(i32_, XB_T_ARRAY)),
                                b_.CreateICmpEQ(collType, ConstantInt::get(i32_, XB_T_HASH))),
                    inColl, own);
    b_.SetInsertPoint(inColl);
    Value *elem = inlineRefTarget(ref, t.shared);
    BasicBlock *collEnd = b_.GetInsertBlock();
    b_.CreateBr(join);
    b_.SetInsertPoint(own);
    Value *ownItem = heapItem(b_.CreateStructGEP(refTy_, ref, RefValue));
    b_.CreateBr(join);
    b_.SetInsertPoint(join);
    PHINode *phi = b_.CreatePHI(ptrTy_, 2, "each.value");
    phi->addIncoming(elem, collEnd);
    phi->addIncoming(ownItem, own);
    t.local = phi;
    return t;
  }
  if (v.kind == xir::Var::Kind::Capture) { // a codeblock's cell, which holds a value or an element
    Value *cell = capturedCell(v);
    t.type = ConstantInt::get(i32_, XB_T_CELL);
    t.ptr = cell;
    BasicBlock *notShared = newBlock("cell.local"), *local = newBlock("cell.value");
    branchLikely(isLocal(cell, false), notShared, t.shared);
    b_.SetInsertPoint(notShared);
    branchLikely(b_.CreateICmpNE(itemType(cell), ConstantInt::get(i32_, XB_T_REF)), local, t.shared);
    b_.SetInsertPoint(local);
    t.local = cell;
    return t;
  }
  // A frame variable or a STATIC: its own item, which may hold a cell (a plain branch, since a shared variable, or a
  // parameter passed by reference, holds one all the time) or an element.
  Value *item = rawAddress(v);
  BasicBlock *from = b_.GetInsertBlock(), *ref = newBlock("ref"), *inCell = newBlock("ref.cell");
  BasicBlock *value = newBlock("cell.value"), *join = newBlock("join");
  b_.CreateCondBr(holdsRef(item), ref, join);
  b_.SetInsertPoint(ref);
  Value *itemTy = itemType(item), *cell = itemPointer(item, "cell");
  runtimeArgs(itemTy, cell);
  branchLikely(b_.CreateICmpEQ(itemTy, ConstantInt::get(i32_, XB_T_CELL)), inCell, t.shared);
  b_.SetInsertPoint(inCell);
  branchLikely(isLocal(cell), value, t.shared);
  b_.SetInsertPoint(value);
  Value *inner = heapItem(cell);
  branchLikely(b_.CreateICmpNE(itemType(inner), ConstantInt::get(i32_, XB_T_REF)), join, t.shared);
  b_.SetInsertPoint(join);
  PHINode *phi = b_.CreatePHI(ptrTy_, 2, "var");
  phi->addIncoming(item, from);
  phi->addIncoming(inner, value);
  t.local = phi;
  return t;
}

// A load of the variable: the address of its value, in place, or where the runtime copied it: `shadow`, or for a Local
// `shadow` a scratch item the caller copies at once.
Value *CodeGen::loadVar(const xir::Var &v, xir::Access a, Value *shadow, BasicBlock *unwind) {
  VarTarget t = varTarget(v, a, true);
  if (!t.shared) return t.local;
  BasicBlock *localEnd = b_.GetInsertBlock(), *join = newBlock("var.loaded");
  if (t.local) b_.CreateBr(join);
  b_.SetInsertPoint(t.shared);
  // A Local home is given the scratch item the runtime wrote, which the caller copies: its own address never joins a
  // pointer (LLVM keeps it in registers).
  Value *got = localItems_.count(shadow) ? scratchItem(0) : shadow;
  b_.CreateCall(rtRefLoad_, {fnCtx_, t.type, t.ptr, got});
  checkPending(unwind);
  BasicBlock *sharedEnd = b_.GetInsertBlock();
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
  if (!t.local) return got;
  PHINode *phi = b_.CreatePHI(ptrTy_, 2, "var");
  phi->addIncoming(t.local, localEnd);
  phi->addIncoming(got, sharedEnd);
  return phi;
}

// A store of `src` into the variable: in place, or through the runtime (xb_ref_store).
void CodeGen::storeVar(const xir::Var &v, xir::Access a, Value *src, BasicBlock *unwind) {
  VarTarget t = varTarget(v, a);
  if (t.local) copyItem(t.local, src);
  if (!t.shared) return;
  BasicBlock *join = newBlock("var.stored");
  if (t.local) b_.CreateBr(join);
  b_.SetInsertPoint(t.shared);
  b_.CreateCall(rtRefStore_, {fnCtx_, t.type, t.ptr, callArg(src, 0)});
  checkPending(unwind);
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
}

// The 0-based entry `pos0` of a hash without holes (xb_hash, `used` == `len`), where position k is entry k - 1;
// branches to `miss` for one with holes or a position outside it.
Value *CodeGen::hashEntryAt(Value *hash, Value *pos0, BasicBlock *miss) {
  Value *len = hashLength(hash);
  Value *used = b_.CreateZExt(loadFrom(i32_, b_.CreateStructGEP(hashTy_, hash, HashUsed), Mem::Unknown, "hused"), i64_);
  BasicBlock *in = newBlock("hash.entry");
  b_.CreateCondBr(b_.CreateAnd(b_.CreateICmpEQ(used, len), b_.CreateICmpULT(pos0, len)), in, miss);
  b_.SetInsertPoint(in);
  Value *entries = loadFrom(ptrTy_, b_.CreateStructGEP(hashTy_, hash, HashEntries), Mem::Unknown, "entries");
  return b_.CreateInBoundsGEP(hentryTy_, entries, b_.CreateZExtOrTrunc(pos0, intPtrTy_), "entry");
}

// A local hash's live entries (xb_hash.len), as an i64.
Value *CodeGen::hashLength(Value *hash) {
  return b_.CreateZExt(loadFrom(i32_, b_.CreateStructGEP(hashTy_, hash, HashLen), Mem::Unknown, "hlen"), i64_);
}

// The element an xb_ref designates, when it is in place (see xb_ref in xbrt.h): an array's element within it (an array
// of items), or the entry at a position of a hash without holes, of a local array or hash; branches to `miss`
// otherwise (a shared collection, a compact array, anything else), where the runtime finds it or raises the bound
// error. `coll` and its `collType`, when given, are the reference's collection (a FOR EACH loop's source, whose type
// a loop versioned for it knows).
Value *CodeGen::inlineRefTarget(Value *ref, BasicBlock *miss, Value *coll, Value *collType) {
  if (!coll) coll = heapItem(b_.CreateStructGEP(refTy_, ref, RefColl));
  Value *type = collType ? collType : itemType(coll);
  Value *pos0 = b_.CreateSub(loadFrom(i64_, b_.CreateStructGEP(refTy_, ref, RefIndex), Mem::RefIndex, "index"),
                             ConstantInt::get(i64_, 1));
  BasicBlock *arr = newBlock("ref.array"), *hash = newBlock("ref.hash"), *found = newBlock("ref.found");
  SwitchInst *sw = b_.CreateSwitch(type, miss, 2);
  sw->addCase(ConstantInt::get(i32_, XB_T_ARRAY), arr);
  sw->addCase(ConstantInt::get(i32_, XB_T_HASH), hash);
  b_.SetInsertPoint(arr);
  Value *array = itemPointer(coll, "arr");
  BasicBlock *arrayLocal = newBlock("ref.array.local");
  branchLikely(isLocalItems(objectState(array)), arrayLocal, miss); // a compact form: the runtime
  b_.SetInsertPoint(arrayLocal);
  BasicBlock *inArray = newBlock("ref.array.in");
  branchLikely(b_.CreateICmpULT(pos0, arrayLength(array)), inArray, miss);
  b_.SetInsertPoint(inArray);
  Value *elem = arrayElement(array, pos0);
  b_.CreateBr(found);
  b_.SetInsertPoint(hash);
  Value *keyType = itemType(heapItem(b_.CreateStructGEP(refTy_, ref, RefKey)));
  Value *h = itemPointer(coll, "hash");
  BasicBlock *byPos = newBlock("ref.hash.pos"); // a reference that finds its entry by key goes to the runtime
  branchLikely(b_.CreateAnd(b_.CreateICmpEQ(keyType, ConstantInt::get(i32_, XB_T_NIL)), isLocal(h)), byPos, miss);
  b_.SetInsertPoint(byPos);
  Value *value = b_.CreateStructGEP(hentryTy_, hashEntryAt(h, pos0, miss), HEntryValue);
  BasicBlock *hashEnd = b_.GetInsertBlock();
  b_.CreateBr(found);
  b_.SetInsertPoint(found);
  PHINode *target = b_.CreatePHI(ptrTy_, 2, "target");
  target->addIncoming(elem, inArray);
  target->addIncoming(value, hashEnd);
  return heapItem(target);
}

// A load that reads its element in place (FramePlan::eachMoveOf): the element at the each.move's position of its
// source, as the loop's reference designates it, by the source's type (known in a loop versioned for it; in a
// resumable function, which version-each leaves alone, tested here): a string's byte (the shared one-byte string, into
// `own`), a local array's element within it (an item in place, an integer or a logical of a compact form into `own`:
// emitCompactRead), the value of the entry at the position of a local hash without holes; anything else (a shared
// collection, a NIL of a compact form) through the reference, as loadVar reads it. Null for another load, or one whose
// source's type is not known (outside a resumable function) or can be none of those.
Value *CodeGen::loadEachElement(const xir::Instruction &load, Value *own) {
  const xir::Instruction *found = plan_->eachMoveOf(&load);
  if (!found) return nullptr;
  const xir::Instruction &move = *found;
  const xir::Value *src = move.operand(2);
  xir::TypeSet t = plan_->types().of(src);
  auto may = [&](xb_type k) { return (t & xir::typeBit(k)) != 0; };
  if (!may(XB_T_STRING) && !may(XB_T_ARRAY) && !may(XB_T_HASH)) return nullptr;
  if ((t & (t - 1)) != 0 && !xir::isResumable(*fnShown_)) return nullptr; // a loop's generic copy: no test worth it
  Value *item = addr(src);
  Value *pos0 = b_.CreateSub(value(move.operand(1)), ConstantInt::get(i64_, 1));
  BasicBlock *miss = newBlock("each.miss"), *join = newBlock("each.read");
  std::vector<std::pair<Value *, BasicBlock *>> got; // the element's address on each path that read it
  auto read = [&](Value *at) {
    got.push_back({at, b_.GetInsertBlock()});
    b_.CreateBr(join);
  };
  auto within = [&](Value *len, const char *name) { // the position within `len`, or the miss
    BasicBlock *in = newBlock(name);
    branchLikely(b_.CreateICmpULT(pos0, len), in, miss);
    b_.SetInsertPoint(in);
  };
  SwitchInst *sw = b_.CreateSwitch(typeOf(src, item), miss, 3);
  if (may(XB_T_STRING)) {
    BasicBlock *str = newBlock("each.string");
    sw->addCase(ConstantInt::get(i32_, XB_T_STRING), str);
    b_.SetInsertPoint(str);
    Value *s = itemPointer(item, "str");
    within(strLength(s), "each.byte");
    storeTagged(own, XB_T_STRING, byteString(strByte(s, pos0)));
    read(own);
  }
  if (may(XB_T_ARRAY)) {
    BasicBlock *arr = newBlock("each.array"), *items = newBlock("each.items"), *compact = newBlock("each.compact");
    sw->addCase(ConstantInt::get(i32_, XB_T_ARRAY), arr);
    b_.SetInsertPoint(arr);
    Value *a = itemPointer(item, "arr");
    Value *state = objectState(a);
    branchLikely(isLocalItems(state), items, compact);
    b_.SetInsertPoint(items);
    within(arrayLength(a), "each.item");
    read(arrayElement(a, pos0));
    // A compact form is a local array's (a shared one holds items): its integers and logicals; its NILs (a value of
    // the form's own) and the other forms through the reference.
    b_.SetInsertPoint(compact);
    BasicBlock *ints = newBlock("each.ints"), *logicals = newBlock("each.logicals");
    SwitchInst *form = b_.CreateSwitch(elemsOf(state), miss, 2);
    form->addCase(ConstantInt::get(i8_, XB_ELEMS_INT), ints);
    form->addCase(ConstantInt::get(i8_, XB_ELEMS_LOGICAL), logicals);
    for (bool logical : {false, true}) {
      b_.SetInsertPoint(logical ? logicals : ints);
      BasicBlock *in = newBlock("each.compact.in");
      branchLikely(b_.CreateICmpULT(pos0, arrayLength(a)), in, miss);
      emitCompactRead(logical, own, a, pos0, in, miss, join, nullptr);
      got.push_back({own, b_.GetInsertBlock()});
    }
  }
  if (may(XB_T_HASH)) {
    BasicBlock *hash = newBlock("each.hash"), *local = newBlock("each.hash.local");
    sw->addCase(ConstantInt::get(i32_, XB_T_HASH), hash);
    b_.SetInsertPoint(hash);
    Value *h = itemPointer(item, "hash");
    branchLikely(isLocal(h), local, miss); // a shared hash: through the reference
    b_.SetInsertPoint(local);
    read(heapItem(b_.CreateStructGEP(hentryTy_, hashEntryAt(h, pos0, miss), HEntryValue)));
  }
  b_.SetInsertPoint(miss);
  Value *other = loadVar(*load.var, load.access, own, unwind_);
  got.push_back({other, b_.GetInsertBlock()});
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
  PHINode *phi = b_.CreatePHI(ptrTy_, (unsigned)got.size(), "each.elem");
  for (auto &[at, from] : got) phi->addIncoming(at, from);
  return phi;
}

// The byte that a load of loadEachElement over a string holds, read again at its position in the loop's source, for
// `use` of it in the load's own block (where FramePlan keeps the source live): null when `v` is no such load. Branches
// to `miss` for a position outside the string (where the load read through the loop's reference).
Value *CodeGen::elementByte(const xir::Instruction &use, const xir::Value *v, BasicBlock *miss) {
  const auto *load = dynamic_cast<const xir::Instruction *>(v);
  const xir::Instruction *found = load ? plan_->eachMoveOf(load) : nullptr;
  if (!found || load->parent != use.parent) return nullptr;
  const xir::Instruction &move = *found;
  if (plan_->types().of(move.operand(2)) != xir::typeBit(XB_T_STRING)) return nullptr;
  Value *s = itemPointer(addr(move.operand(2)), "str");
  Value *pos0 = b_.CreateSub(value(move.operand(1)), ConstantInt::get(i64_, 1));
  BasicBlock *in = newBlock("elem.in");
  branchLikely(b_.CreateICmpULT(pos0, strLength(s)), in, miss);
  b_.SetInsertPoint(in);
  return strByte(s, pos0);
}

// Branches to a new block when `cond` holds (marked unlikely) and continues
// there; endUnlikely joins the returned block.
BasicBlock *CodeGen::beginUnlikely(Value *cond, const char *name) {
  BasicBlock *then = newBlock(name);
  BasicBlock *join = newBlock("join");
  MDBuilder md(ctx_);
  b_.CreateCondBr(cond, then, join, md.createUnlikelyBranchWeights());
  b_.SetInsertPoint(then);
  return join;
}

void CodeGen::endUnlikely(BasicBlock *join) {
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
}

// Whether `item` designates another item (a cell or an element); the two
// tags are consecutive (xbrt.h).
Value *CodeGen::holdsRef(Value *item) {
  static_assert(XB_T_REF == XB_T_CELL + 1, "one range test covers both reference tags");
  Value *type = itemType(item);
  Value *offset = b_.CreateSub(type, ConstantInt::get(i32_, XB_T_CELL));
  return b_.CreateICmpULE(offset, ConstantInt::get(i32_, XB_T_REF - XB_T_CELL));
}

// After a store through a cell: the cell's barrier when the value went in place (a local cell that holds no element).
// A shared cell and an element took the value through xb_ref_store, which recorded it.
void CodeGen::cellStoreWritten(Value *cell, bool fromItem) {
  BasicBlock *local = newBlock("cell.local"), *inPlace = newBlock("cell.written"), *join = newBlock("join");
  branchLikely(isLocal(cell, fromItem), local, join);
  b_.SetInsertPoint(local);
  branchLikely(b_.CreateICmpNE(itemType(cell), ConstantInt::get(i32_, XB_T_REF)), inPlace, join);
  b_.SetInsertPoint(inPlace);
  cellWritten(cell);
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
}

// After a store into a cell: the inline test of xb_gc_write (xbrt.h).
void CodeGen::cellWritten(Value *cell) {
  recordIfOld(cell, stateByte(cell, AtomicOrdering::Monotonic)); // other threads set its bits (xb_gc_write)
}

// Write barrier after a store into a variable (see xb_gc_write in xbrt.h). A
// captured variable's cell is a heap object; a STATIC, a shared variable and
// a parameter may hold a cell. A store through an element (a FOR EACH
// variable's too) went through xb_ref_store. Frame slots and STATIC storage
// are roots and need nothing.
void CodeGen::writeBarrier(const xir::Var &v, xir::Access a) {
  if (v.kind == xir::Var::Kind::Capture) {
    cellStoreWritten(capturedCell(v), false);
    return;
  }
  if (a != xir::Access::MaybeRef) return;
  Value *item = rawAddress(v);
  BasicBlock *inCell = newBlock("cell"), *join = newBlock("join");
  if (v.kind == xir::Var::Kind::Static) { // shared storage: the runtime stored the value (xb_ref_store)
    BasicBlock *local = newBlock("static.local");
    branchLikely(storageLocal(), local, join);
    b_.SetInsertPoint(local);
  }
  b_.CreateCondBr(b_.CreateICmpEQ(itemType(item), ConstantInt::get(i32_, XB_T_CELL)), inCell, join);
  b_.SetInsertPoint(inCell);
  cellStoreWritten(heapItem(itemPointer(item, "cell")), true);
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
}

// Whether `v` is a STATIC, as the runtime takes it (shared storage once threads run).
Value *CodeGen::isStaticArg(const xir::Var &v) {
  return ConstantInt::get(i32_, v.kind == xir::Var::Kind::Static ? 1 : 0);
}

// What a FOR EACH operation of the runtime is told of its variable (`how`, xbrt.h): in a cell (the loop's first
// operand), a STATIC.
Value *CodeGen::eachHow(const xir::Instruction &i) {
  static_assert(XB_EACH_IN_CELL == 1, "the loop's boolean is the bit");
  Value *how = b_.CreateZExt(value(i.operand(0)), i32_);
  if (i.var->kind == xir::Var::Kind::Static) how = b_.CreateOr(how, ConstantInt::get(i32_, XB_EACH_STATIC));
  return how;
}

// A reference to the variable, for an argument passed by reference: its cell
// (made now if it has none), or the current element of a FOR EACH variable
// (see xb_ref_of).
void CodeGen::refOf(const xir::Var &v, Value *dst) {
  if (v.kind == xir::Var::Kind::Capture) {
    storeTagged(dst, XB_T_CELL, capturedCell(v));
    return;
  }
  b_.CreateCall(rtRefOf_, {fnCtx_, dst, rawAddress(v), isStaticArg(v)});
}

} // namespace xb
