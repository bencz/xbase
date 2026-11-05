#pragma once

#include "xir/IR.h"

#include <string>

namespace xb::xir {

// Checks every function of the module; returns an empty string, or the first
// problem found followed by the function's text. Problems are compiler bugs.
std::string verify(const Module &m);

} // namespace xb::xir
