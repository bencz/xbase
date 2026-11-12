#pragma once

#include <llvm/IR/PassManager.h>


namespace xb {

// Fewer polls on loops' back edges (codegen: pollOnBackEdge, the loads marked !xb.poll), on the functions the loop
// splitting changed or not (LoopSplit.h runs first: its fast loops are the ones these passes run in). A loop needs no
// poll when a test of the pending requests that serves a safepoint (a load marked !xb.safepoint, after a call) runs
// on every pass: its polls go. A counted loop, whose header's exit test compares an induction variable of constant
// step with a value the loop does not change, polls in the branch that test already makes (strip mining): the test
// compares with a stop, the exit's value or the variable's value kPassesPerPoll passes ahead, and only on reaching it
// does the loop tell the exit from a poll, whose slow path is the one the back edge's poll had. Another loop counts its
// passes down in a register and loads the pending requests when the count runs out. Returns whether the function
// changed.
bool placeLoopPolls(llvm::Function &f, llvm::FunctionAnalysisManager &fam);

// The metadata the lowering puts on the loads of the pending requests: a back edge's poll, and a test that serves a
// safepoint.
inline constexpr const char *kPollMetadata = "xb.poll";
inline constexpr const char *kSafepointMetadata = "xb.safepoint";

} // namespace xb
