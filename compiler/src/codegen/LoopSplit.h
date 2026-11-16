#pragma once

#include <llvm/IR/PassManager.h>

namespace xb {

// Loops without slow paths. Every inline fast path of the lowering keeps a
// call to the runtime for the other cases, behind a cold edge (a branch
// weighted unlikely), so every loop holds calls, and LLVM neither moves loads
// out of a loop that contains a call that may write memory nor keeps a type
// it has proven. Each loop with cold edges (a nest whole, when its fast copy
// would make no call; else its inner loops) is cloned twice: in the original
// every cold edge goes to the same block of a generic copy, which ends the
// iteration generically and goes back to the loop again, the second copy (an
// inner loop's iteration to the same inner loop there), whose cold edges go to
// the generic copy too. The original and the loop again, left without their
// slow paths' calls, are then optimized as fast loops. Loops too large to clone
// cheaply are left as they are. Returns whether the function changed.
bool splitColdLoopPaths(llvm::Function &f);

// Cold exits on values a loop does not change, tested once. In an innermost
// fast loop, a branch whose condition can be computed on entry (from what LLVM
// moved out of the loop: the type and bounds of an invariant array and index;
// and in a loop that makes no call, from the sharing states of the objects of
// items it does not change, read on entry) and whose cold side leaves the loop
// (into the generic copy above) is tested in the preheader instead, all of
// them at once: when any would leave, an exact copy of the loop runs, which
// leaves where the loop would have; otherwise the loop runs with those
// branches gone. The conditions are frozen first: one the loop never reaches
// may be poison on entry. Runs on the functions the default pipeline has
// optimized; returns whether the function changed.
bool versionInvariantColdExits(llvm::Function &f, llvm::FunctionAnalysisManager &fam);

// The metadata the lowering puts on a load of the state byte of an object an item holds, for
// versionInvariantColdExits: the offset of the item's header word from its pointer word, and the shift of the type in
// the header word. A loop that makes no call reads such a state on entry, once the item holds an array, a hash, a cell
// or a reference: without a call, no object this thread alone reaches becomes shared.
inline constexpr const char *kStateLoad = "xb.state";

// The function attribute of the runtime's write barriers (xb_gc_write_item, xb_gc_remember): they
// record what changed for the collector, run no code of the program and share nothing, so the loop passes above do
// not count a call to one as a call.
inline constexpr const char *kBarrierAttr = "xb-barrier";

} // namespace xb
