#pragma once

namespace llvm {
class Function;
}

namespace xb {

// Loops without slow paths. Every inline fast path of the lowering keeps a
// call to the runtime for the other cases, behind a cold edge (a branch
// weighted unlikely), so every loop holds calls, and LLVM neither moves loads
// out of a loop that contains a call that may write memory nor keeps a type
// it has proven. Each innermost loop with cold edges is cloned twice: in the
// original every cold edge goes to the same block of a generic copy, which ends
// the iteration generically and goes back to the loop again, the second copy,
// whose cold edges go to the generic copy too. The original and the loop again,
// left without their slow paths' calls, are then optimized as fast loops. Loops
// too large to clone cheaply are left as they are. Returns whether the function
// changed.
bool splitColdLoopPaths(llvm::Function &f);

} // namespace xb
