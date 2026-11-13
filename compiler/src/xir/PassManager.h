#pragma once

#include "xir/IR.h"

#include <string>

namespace xb::xir {

// Runs the optimization passes on every function of the module, in order,
// until none changes anything (bounded), then (with `final`) the final ones,
// verifying the function after each pass that changed it. Without `final`,
// the module's bodies are ready to be inlined elsewhere (a carried source of
// the runtime), the final passes running where they are copied.
// Returns an empty string, or the verifier's report of the first pass that
// left invalid xIR (a compiler bug), naming the pass.
std::string optimize(Module &m, bool final = true);

} // namespace xb::xir
