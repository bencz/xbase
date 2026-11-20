#pragma once

#include <llvm/IR/PassManager.h>

namespace xb {

// A function's frame set up where it is needed, not on entry. The entry writes the frame's slots (the parameters, NIL
// in the others) and header, links it into the context's chain and tests the stack's limit; nothing needs that before
// the first instruction that touches the frame or calls a function (whatever may collect, raise an error or walk the
// chain), so a path that reaches neither, a leaf's early return (`fib`'s `n < 2`), does none of it, and code
// generation can leave the saving of registers to the other paths too (shrink wrapping). The setup moves to the first
// such instruction of every path: a flag, made false on entry and true by a setup, is tested there (the
// simplification turns it into PHIs and folds the tests a setup dominates). The setup writes the slots with the values
// they had on entry, which is right since nothing touched the frame before it; the unlink at the exit stores the
// caller's frame, which the chain still holds when nothing was linked. When some way returns without a setup, the exit
// unlinks only a frame linked, reading the caller's frame back from its link, so that way keeps nothing for the way
// out; a function whose part before any setup can return is then split into a small entry (inlined into its callers)
// and a copy of the whole function. Only a function whose entry has the frame's setup as lowered (a direct entry, whose
// caller's frame is an argument) is changed. Returns whether it changed.
bool sinkFrameSetup(llvm::Function &f, llvm::FunctionAnalysisManager &fam);

} // namespace xb
