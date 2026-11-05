#pragma once

#include "xir/IR.h"

#include <string>

namespace xb::xir {

// Text form of xIR (--dump-xir): for reading and tests only.
std::string print(const Module &m);
std::string print(const Function &f);

} // namespace xb::xir
