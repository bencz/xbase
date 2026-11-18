#pragma once

#include "xir/IR.h"

#include <string>

namespace xb::xir {

// Text form of xIR (--dump-xir): for reading and tests only. With `types` (--dump-xir-types), each item value is
// followed by the types the type inference finds it may hold (xir::TypeInfo), as `{int,double}`.
std::string print(const Module &m, bool types = false);
std::string print(const Function &f, bool types = false);

} // namespace xb::xir
