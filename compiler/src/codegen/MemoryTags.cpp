// The alias class of the memsets LLVM makes from the stores of an item (see MemoryTags.h).
#include "codegen/MemoryTags.h"

#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Module.h>

using namespace llvm;

namespace xb {

namespace {

// Whether bytes [from, to) of alloca `a` hold items: an item or an array of them, the slots of a frame (its last
// field, an array of items).
bool holdsItems(const AllocaInst &a, const DataLayout &dl, Type *item, uint64_t from, uint64_t to) {
  Type *t = a.getAllocatedType();
  std::optional<TypeSize> size = a.getAllocationSize(dl);
  if (!size || size->isScalable() || to > size->getFixedValue()) return false;
  if (t == item) return true;
  if (auto *arr = dyn_cast<ArrayType>(t)) return arr->getElementType() == item;
  auto *st = dyn_cast<StructType>(t);
  if (!st || st->getNumElements() == 0) return false;
  auto *slots = dyn_cast<ArrayType>(st->getElementType(st->getNumElements() - 1));
  return slots && slots->getElementType() == item &&
         from >= dl.getStructLayout(st)->getElementOffset(st->getNumElements() - 1);
}

} // namespace

PreservedAnalyses TagItemMemsets::run(Function &f, FunctionAnalysisManager &) {
  Module &m = *f.getParent();
  NamedMDNode *named = m.getNamedMetadata(kItemTagName);
  Type *item = StructType::getTypeByName(m.getContext(), kItemTypeName);
  if (!named || named->getNumOperands() == 0 || !item) return PreservedAnalyses::all();
  MDNode *tag = named->getOperand(0);
  const DataLayout &dl = m.getDataLayout();
  for (BasicBlock &b : f)
    for (Instruction &i : b) {
      auto *set = dyn_cast<MemSetInst>(&i);
      if (!set || set->getMetadata(LLVMContext::MD_tbaa)) continue;
      auto *len = dyn_cast<ConstantInt>(set->getLength());
      int64_t offset = 0;
      auto *a = dyn_cast<AllocaInst>(GetPointerBaseWithConstantOffset(set->getDest(), offset, dl));
      if (!len || !a || offset < 0) continue;
      uint64_t from = (uint64_t)offset;
      if (!holdsItems(*a, dl, item, from, from + len->getZExtValue())) continue;
      set->setMetadata(LLVMContext::MD_tbaa, tag);
    }
  // Metadata alone changed: every analysis still holds.
  return PreservedAnalyses::all();
}

} // namespace xb
