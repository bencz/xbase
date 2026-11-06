// Where variables are: frame slots, cells of captured variables, STATICs, FOR EACH references; write barriers.
#include "codegen/CodeGen.h"

#include "xbrt.h" // constants only (type tags, GC state bits)

#include <llvm/IR/Constants.h>
#include <llvm/IR/MDBuilder.h>

using namespace llvm;

namespace xb {

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
  return b_.CreateLoad(ptrTy_, capturedCellAddress(fnSelf_, v.index), "cell");
}

// The variable's own item: its frame slot or STATIC storage (which holds its
// cell once it is shared), or a capture's cell; nothing is followed.
Value *CodeGen::rawAddress(const xir::Var &v) {
  switch (v.kind) {
  case xir::Var::Kind::Static: return staticAddress(v.index);
  case xir::Var::Kind::Capture: return capturedCell(v);
  case xir::Var::Kind::Frame: return slot(plan_->slotOf(v));
  }
  return nullptr;
}

// The item a load or store of the variable reaches (see xir::Access).
Value *CodeGen::varAddress(const xir::Var &v, xir::Access a, BasicBlock *unwind) {
  Value *item = rawAddress(v);
  switch (a) {
  case xir::Access::Direct: return item;
  case xir::Access::ThroughRef: return refTarget(item, unwind);
  case xir::Access::MaybeRef: return derefIfRef(item, unwind);
  }
  return item;
}

// The element an XB_T_REF item designates.
Value *CodeGen::refTarget(Value *ref, BasicBlock *unwind) {
  Value *target = b_.CreateCall(rtRefTarget_, {fnCtx_, ref}, "elem");
  checkPending(unwind);
  return target;
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

// `item` itself, or the item it designates when it holds a reference. A
// cell is followed inline (its value is at offset 0): a plain branch, since
// a shared variable, or a parameter passed by reference, holds one all the
// time; only an element (an XB_T_REF, also inside a cell) needs the runtime,
// on a cold edge.
Value *CodeGen::derefIfRef(Value *item, BasicBlock *unwind) {
  BasicBlock *from = b_.GetInsertBlock();
  BasicBlock *join = newBlock("join"), *then = newBlock("ref");
  b_.CreateCondBr(holdsRef(item), then, join);
  b_.SetInsertPoint(then);
  Value *type = itemType(item);
  Value *cell = b_.CreateLoad(ptrTy_, b_.CreateStructGEP(itemTy_, item, FValue), "cell");
  Value *inner = b_.CreateSelect(b_.CreateICmpEQ(type, ConstantInt::get(i32_, XB_T_CELL)), cell, item, "inner");
  Value *innerType = itemType(inner);
  BasicBlock *innerFrom = b_.GetInsertBlock();
  BasicBlock *elemJoin = beginUnlikely(b_.CreateICmpEQ(innerType, ConstantInt::get(i32_, XB_T_REF)), "elem");
  Value *target = refTarget(inner, unwind);
  BasicBlock *elemEnd = b_.GetInsertBlock();
  endUnlikely(elemJoin);
  PHINode *followed = b_.CreatePHI(ptrTy_, 2, "followed");
  followed->addIncoming(inner, innerFrom);
  followed->addIncoming(target, elemEnd);
  BasicBlock *refEnd = b_.GetInsertBlock();
  endUnlikely(join);
  PHINode *phi = b_.CreatePHI(ptrTy_, 2, "var");
  phi->addIncoming(item, from);
  phi->addIncoming(followed, refEnd);
  return phi;
}

// After a store through `item` when it holds a reference: the write barrier
// of the object holding the target. A cell's is the inline test of
// cellWritten (a plain branch, as in derefIfRef); an element's (also inside a
// cell) is xb_ref_written, on a cold edge.
void CodeGen::refWritten(Value *item) {
  BasicBlock *then = newBlock("ref.written"), *join = newBlock("join");
  b_.CreateCondBr(holdsRef(item), then, join);
  b_.SetInsertPoint(then);
  Value *type = itemType(item);
  BasicBlock *inCell = newBlock("cell.written");
  BasicBlock *element = newBlock("elem.written");
  branchLikely(b_.CreateICmpEQ(type, ConstantInt::get(i32_, XB_T_CELL)), inCell, element);
  b_.SetInsertPoint(element);
  b_.CreateCall(rtRefWritten_, {item});
  b_.CreateBr(join);
  b_.SetInsertPoint(inCell);
  Value *cell = b_.CreateLoad(ptrTy_, b_.CreateStructGEP(itemTy_, item, FValue), "cell");
  Value *cellType = itemType(cell);
  BasicBlock *throughCell = beginUnlikely(b_.CreateICmpEQ(cellType, ConstantInt::get(i32_, XB_T_REF)), "elem");
  b_.CreateCall(rtRefWritten_, {cell});
  endUnlikely(throughCell);
  cellWritten(cell);
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
}

// After a store into a cell: the inline test of xb_gc_write (xbrt.h).
void CodeGen::cellWritten(Value *cell) {
  Value *back = ConstantInt::getSigned(i64_, -static_cast<int64_t>(XB_GC_STATE_BACK));
  Value *state = b_.CreateLoad(i8_, b_.CreateGEP(i8_, cell, back), "gc.state");
  Value *bits = b_.CreateAnd(state, ConstantInt::get(i8_, XB_GC_STATE_OLD | XB_GC_STATE_RECORDED));
  BasicBlock *join = beginUnlikely(b_.CreateICmpEQ(bits, ConstantInt::get(i8_, XB_GC_STATE_OLD)), "gc.old");
  b_.CreateCall(rtGcRemember_, {cell});
  endUnlikely(join);
}

// Write barrier after a store into a variable (see xb_gc_write in xbrt.h). A
// captured variable's cell is a heap object; a STATIC, a shared variable, a
// parameter and a FOR EACH variable may hold a cell or a reference, whose
// target lives in a heap object. Frame slots and STATIC storage are roots and
// need nothing.
void CodeGen::writeBarrier(const xir::Var &v, xir::Access a) {
  if (v.kind == xir::Var::Kind::Capture) {
    Value *cell = capturedCell(v);
    cellWritten(cell);
    refWritten(cell);
    return;
  }
  Value *item = rawAddress(v);
  if (a == xir::Access::ThroughRef) b_.CreateCall(rtRefWritten_, {item});
  else if (a == xir::Access::MaybeRef) refWritten(item);
}

// A reference to the variable, for an argument passed by reference: its cell
// (made now if it has none), or the current element of a FOR EACH variable
// (see xb_ref_of).
void CodeGen::refOf(const xir::Var &v, Value *dst) {
  if (v.kind == xir::Var::Kind::Capture) {
    storeTagged(dst, XB_T_CELL, capturedCell(v));
    return;
  }
  Value *isStatic = ConstantInt::get(i32_, v.kind == xir::Var::Kind::Static ? 1 : 0);
  b_.CreateCall(rtRefOf_, {fnCtx_, dst, rawAddress(v), isStatic});
}

} // namespace xb
