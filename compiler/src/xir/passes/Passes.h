#pragma once

#include "xir/IR.h"

namespace xb::xir {

// The optimization passes on xIR (docs/xir.md, Passes). Each one transforms a
// function in place and returns whether it changed anything.

// A call of a small function of the module, or of a function of the runtime's carried sources (Module::library), runs
// its body in place, shown on the call stack as the function (inline.enter, inline.leave), its variables the caller's
// own (Inline.cpp).
bool inlineFunctions(Module &m, Function &f);

// `Eval( {| p | body }, x )` of a codeblock literal of the function: the body runs in place, shown on the call stack as
// the block, its variables the function's own (Inline.cpp).
bool inlineBlocks(Function &f);

// A frame variable that nothing captures or passes by reference any more holds no cell (`shared`): it is read and
// written directly (no barrier: it is never in the heap), which lets promote-variables take it (Inline.cpp).
bool unshareVariables(Function &f);

// `x := x + y` on a LOCAL only the function sees and that holds strings
// becomes one `append`, whose string grows in place (AppendInPlace.cpp).
bool appendInPlace(Function &f);

// mem2reg: LOCALs reached only by direct loads and stores (not shared, not
// parameters, not FOR EACH variables) become SSA values, with block
// parameters where their definitions meet; they leave the frame.
bool promoteVariables(Function &f);

// Operations on constant operands become constants; a branch on a constant
// condition becomes a jump.
bool foldConstants(Function &f);

// Removes unreachable blocks, sends branches past blocks that only jump on,
// merges a block into its single predecessor when that one jumps to it, and
// turns two branches to one block that differ in a true/false argument into a
// jump passing the condition.
bool simplifyCFG(Function &f);

// Removes instructions whose result is unused and that do nothing else (see
// Effect), and block parameters nothing uses.
bool removeDeadCode(Function &f);

// Checks the result of an operation whose error may substitute it, where the
// types it gives itself hold no heap reference: a substituted value of another
// type continues in a generic copy of the rest of the function
// (TypedContinuations.cpp).
bool typedContinuations(Function &f);

// A generator's values that live across a YIELD move into frame variables, which the enumerator saves between steps
// (Resumable.cpp). Not an optimization: Gen runs it on every generator, and the other passes leave such functions'
// variables in memory.
bool makeResumable(Function &f);

} // namespace xb::xir
