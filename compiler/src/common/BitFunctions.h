#pragma once

#include "parser/AST.h" // BinOp, UnOp
#include "xbrt.h"       // xb_bit_function, xb_bit_apply

#include <string>

namespace xb {

// One of xbase's bit functions, by its upper-cased name: the compiler folds
// their constant calls and inlines their number ones (xb_bit_apply's rules).
inline const xb_bit_function *findBitFunction(const std::string &name) {
  for (uint32_t k = 0; k < XB_BIT_OP_COUNT; ++k) {
    const xb_bit_function *f = xb_bit_function_of((xb_bit_op)k);
    if (f->name && name == f->name) return f;
  }
  return nullptr;
}

// The bit operation of a bit operator (& | ^^ << >>, and ~), or null.
inline const xb_bit_function *bitOperator(BinOp op) {
  switch (op) {
  case BinOp::BitAnd: return xb_bit_function_of(XB_BIT_AND);
  case BinOp::BitOr: return xb_bit_function_of(XB_BIT_OR);
  case BinOp::BitXor: return xb_bit_function_of(XB_BIT_XOR);
  case BinOp::Shl: return xb_bit_function_of(XB_BIT_SHL);
  case BinOp::Shr: return xb_bit_function_of(XB_BIT_SAR);
  default: return nullptr;
  }
}
inline const xb_bit_function *bitOperator(UnOp op) {
  return op == UnOp::BitNot ? xb_bit_function_of(XB_BIT_NOT) : nullptr;
}

} // namespace xb
