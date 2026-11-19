#pragma once

#include <llvm/IR/PassManager.h>

namespace xb {

// The alias class of the memsets LLVM makes from the stores of an item. Every access the lowering makes carries an
// alias class (CodeGen::Mem), so that a loop's loads of what it does not change (an array's header, the state byte of
// an object) move out of it; but MemCpyOpt merges the stores of a constant item (a NIL) into a memset that has none,
// which may then write anything, and the loads stay. A memset within the items of an alloca (a frame's slots, a local
// item, an array of items) gets the class of items again: those are read and written as items alone.
struct TagItemMemsets : llvm::PassInfoMixin<TagItemMemsets> {
  llvm::PreservedAnalyses run(llvm::Function &f, llvm::FunctionAnalysisManager &fam);
};

// The name of the item type in the module, and the module's named metadata whose operand is the alias tag of items
// (CodeGen::Mem::Item), which the lowering sets.
inline constexpr const char *kItemTypeName = "xb_item";
inline constexpr const char *kItemTagName = "xb.item.tbaa";

} // namespace xb
