#pragma once

#include "xir/IR.h"

namespace xb::xir {

// The optimization passes on xIR (docs/xir.md, Passes). Each one transforms a
// function in place and returns whether it changed anything.

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

} // namespace xb::xir
