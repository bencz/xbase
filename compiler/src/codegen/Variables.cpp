// Variable addresses: frame slots, cells of captured variables, STATICs, FOR EACH references.
#include "codegen/CodeGen.h"

#include "xbrt.h" // constants only (type tags)

#include <llvm/IR/Constants.h>
#include <llvm/IR/MDBuilder.h>

using namespace llvm;

namespace xb {

Value *CodeGen::staticAddress(uint32_t idx) {
  return b_.CreateConstInBoundsGEP2_32(statics_->getValueType(), statics_, 0, idx);
}

// Variables a codeblock captured live in cells: the frame slot holds the
// cell, and a block reaches it through its own cells[].
void CodeGen::boxCaptured(const std::vector<bool> &boxed) {
  for (uint32_t i = 0; i < boxed.size(); ++i)
    if (boxed[i]) b_.CreateCall(rtCellBox_, {fnCtx_, slot(i)});
}

Value *CodeGen::cellOfSlot(uint32_t idx) {
  return b_.CreateLoad(ptrTy_, b_.CreateStructGEP(itemTy_, slot(idx), FValue), "cell");
}

Value *CodeGen::capturedCellAddress(Value *block, uint32_t index) {
  return b_.CreateInBoundsGEP(blockTy_, block, {b_.getInt32(0), b_.getInt32(2), b_.getInt32(index)});
}

bool CodeGen::isBoxed(const VarExpr &v) const {
  return v.where == VarWhere::Frame && v.slot < fnBoxed_->size() && (*fnBoxed_)[v.slot];
}

bool CodeGen::isEachVar(const VarExpr &v) const {
  for (const EachVar &e : eachVars_)
    if (e.var->where == v.where && e.var->slot == v.slot) return true;
  return false;
}

// The element an XB_T_REF item designates.
Value *CodeGen::refTarget(Value *ref) {
  Value *target = b_.CreateCall(rtRefTarget_, {fnCtx_, ref}, "elem");
  checkPending();
  return target;
}

// `item` itself, or the element it designates when it holds a reference.
Value *CodeGen::derefIfRef(Value *item) {
  Value *type = b_.CreateLoad(i32_, b_.CreateStructGEP(itemTy_, item, FType), "type");
  BasicBlock *from = b_.GetInsertBlock();
  BasicBlock *isRef = newBlock("ref");
  BasicBlock *join = newBlock("deref");
  MDBuilder md(ctx_);
  b_.CreateCondBr(b_.CreateICmpEQ(type, ConstantInt::get(i32_, XB_T_REF)), isRef, join,
                  md.createUnlikelyBranchWeights());
  b_.SetInsertPoint(isRef);
  Value *target = refTarget(item);
  BasicBlock *refEnd = b_.GetInsertBlock();
  b_.CreateBr(join);
  b_.SetInsertPoint(join);
  PHINode *phi = b_.CreatePHI(ptrTy_, 2, "var");
  phi->addIncoming(item, from);
  phi->addIncoming(target, refEnd);
  return phi;
}

// Address of the item a variable reference designates. A cell's value is at
// offset 0, so the cell pointer is the item pointer. The variable of a FOR
// EACH loop being generated holds a reference to the element; STATICs and
// captured variables may hold one when the reference was made elsewhere (a
// FOR EACH in another function, a codeblock created in a loop), so they are
// tested at run time. Other variables never hold references.
Value *CodeGen::varAddress(const Expr &e) {
  auto &v = static_cast<const VarExpr &>(e);
  if (v.where == VarWhere::Capture)
    return derefIfRef(b_.CreateLoad(ptrTy_, capturedCellAddress(fnSelf_, v.slot), "cell"));
  Value *item = v.where == VarWhere::Static ? staticAddress(v.slot) : isBoxed(v) ? cellOfSlot(v.slot) : slot(v.slot);
  if (isEachVar(v)) return refTarget(item);
  return v.where == VarWhere::Static ? derefIfRef(item) : item;
}

} // namespace xb
