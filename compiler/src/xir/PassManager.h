#pragma once

#include "xir/IR.h"

#include <string>

namespace xb::xir {

// Runs the optimization passes on every function of the module, in order,
// until none changes anything (bounded), verifying the function after each
// pass that changed it.
// Returns an empty string, or the verifier's report of the first pass that
// left invalid xIR (a compiler bug), naming the pass.
std::string optimize(Module &m);

} // namespace xb::xir
