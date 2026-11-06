#pragma once

namespace llvm {
class Function;
}

namespace xb {

// Loops without slow paths. Every inline fast path of the lowering keeps a
// call to the runtime for the other cases, behind a cold edge (a branch
// weighted unlikely), so every loop holds calls, and LLVM neither moves loads
// out of a loop that contains a call that may write memory nor keeps a type
// it has proven. Each innermost loop with cold edges is cloned: in the
// original every cold edge goes to the same block of the copy, which keeps
// running the loop generically and never comes back. The original, left
// without its slow paths' calls, is then optimized as a fast loop. Loops too
// large to clone cheaply are left as they are. Returns whether the function
// changed.
bool splitColdLoopPaths(llvm::Function &f);

} // namespace xb
