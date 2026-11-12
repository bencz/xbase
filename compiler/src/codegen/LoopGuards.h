#pragma once

#include <llvm/IR/PassManager.h>

namespace xb {

// Tests a loop makes on values it does not change, out of the loop. A loop's header that computes nothing but its PHIs
// and a branch on a condition the loop does not change, one of whose sides leaves the loop (in a fast loop that
// LoopSplit.h leaves, into the generic copy: the test of a fast path, an invariant array and index), branches in the
// preheader instead (made only for such a loop): when the test fails there, the first pass goes where it would have
// gone, with the values the header's PHIs had on entry. What the test guarded (the element's loads) then runs on every
// pass, and LLVM can move it out of the loop too. Runs on the functions the default pipeline has optimized; returns
// whether the function changed.
bool hoistLoopGuards(llvm::Function &f, llvm::FunctionAnalysisManager &fam);

} // namespace xb
