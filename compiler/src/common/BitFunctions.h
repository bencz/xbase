#pragma once

#include "xbrt.h" // xb_bit_function, xb_bit_apply

#include <string>

namespace xb {

// One of xbase's bit functions, by its upper-cased name: the compiler folds
// their constant calls and inlines their integer ones (xb_bit_apply's rules).
inline const xb_bit_function *findBitFunction(const std::string &name) {
  for (uint32_t k = 0; k < XB_BIT_FUNCTION_COUNT; ++k)
    if (name == xb_bit_function_of((xb_bit_op)k)->name) return xb_bit_function_of((xb_bit_op)k);
  return nullptr;
}

} // namespace xb
