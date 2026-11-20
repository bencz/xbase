// Branch instructions across LLVM versions: LLVM 23 splits BranchInst into CondBrInst and UncondBrInst (BranchInst
// deprecated); LLVM 22 has BranchInst alone, and still builds xbc. The passes use these names.
#pragma once

#include <llvm/Config/llvm-config.h>
#include <llvm/IR/Instructions.h>

namespace xb {

#if LLVM_VERSION_MAJOR >= 23
using CondBranch = llvm::CondBrInst;

// The conditional branch `i` is, or null.
inline CondBranch *condBranch(llvm::Instruction *i) { return llvm::dyn_cast_or_null<llvm::CondBrInst>(i); }
// Whether `i` is a branch, conditional or not.
inline bool isBranch(const llvm::Instruction *i) { return llvm::isa<llvm::UncondBrInst, llvm::CondBrInst>(i); }
inline bool isUncondBranch(const llvm::Instruction *i) { return llvm::isa<llvm::UncondBrInst>(i); }
inline llvm::Instruction *createBranch(llvm::BasicBlock *to, llvm::InsertPosition at) {
  return llvm::UncondBrInst::Create(to, at);
}
inline CondBranch *createCondBranch(llvm::Value *cond, llvm::BasicBlock *ifTrue, llvm::BasicBlock *ifFalse,
                                    llvm::InsertPosition at) {
  return llvm::CondBrInst::Create(cond, ifTrue, ifFalse, at);
}
#else
using CondBranch = llvm::BranchInst;

inline CondBranch *condBranch(llvm::Instruction *i) {
  auto *b = llvm::dyn_cast_or_null<llvm::BranchInst>(i);
  return b && b->isConditional() ? b : nullptr;
}
inline bool isBranch(const llvm::Instruction *i) { return llvm::isa<llvm::BranchInst>(i); }
inline bool isUncondBranch(const llvm::Instruction *i) {
  auto *b = llvm::dyn_cast<llvm::BranchInst>(i);
  return b && b->isUnconditional();
}
inline llvm::Instruction *createBranch(llvm::BasicBlock *to, llvm::InsertPosition at) {
  return llvm::BranchInst::Create(to, at);
}
inline CondBranch *createCondBranch(llvm::Value *cond, llvm::BasicBlock *ifTrue, llvm::BasicBlock *ifFalse,
                                    llvm::InsertPosition at) {
  return llvm::BranchInst::Create(ifTrue, ifFalse, cond, at);
}
#endif

} // namespace xb
