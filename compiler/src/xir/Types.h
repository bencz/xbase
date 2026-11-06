#pragma once

#include "xir/Analysis.h"
#include "xir/IR.h"

#include "xbrt.h" // type tags

#include <unordered_map>

namespace xb::xir {

// The types an item value may hold: one bit per type tag (xb_type).
using TypeSet = uint32_t;

constexpr TypeSet typeBit(xb_type t) { return TypeSet(1) << t; }
constexpr TypeSet kNumberTypes = typeBit(XB_T_INT) | typeBit(XB_T_DOUBLE);
constexpr TypeSet kAnyType = (TypeSet(1) << XB_T_COUNT_) - 1;
// The tags whose value refers to a heap object (xb_type_is_heap).
constexpr TypeSet kHeapTypes = ((TypeSet(1) << (XB_T_HEAP_LAST + 1)) - 1) & ~((TypeSet(1) << XB_T_HEAP_FIRST) - 1);

// The result types of a binary operator whose operands may hold the types `a`
// and `b`, as the runtime's operators make them (core/ops.c); an operand pair
// that is an error adds nothing.
TypeSet binaryResult(BinOp op, TypeSet a, TypeSet b);
// Whether such operands can make it fail: an operand pair it does not take,
// or a divisor that may be zero.
bool binaryMayFail(BinOp op, TypeSet a, TypeSet b);

// Type inference over a function's SSA values: the set of types each item
// value may hold. An optimistic fixpoint: block parameters start empty and
// grow with the values their branches pass, operations follow their operands
// (binaryResult), and anything read from memory (a variable, an element, a
// call's result) may hold any type. The handler of an operation's error may
// substitute a value of any type for its result, so an operation that may fail
// that way may produce any type: the lowering keeps values it finds without
// heap references out of the collector's sight.
class TypeInfo {
public:
  explicit TypeInfo(const CFG &cfg); // the function the CFG was built for
  TypeSet of(const Value *v) const;
  // Never refers to a heap object: the collector need not see it.
  bool noHeap(const Value *v) const { return (of(v) & kHeapTypes) == 0; }

private:
  TypeSet compute(const Instruction &i) const;
  std::unordered_map<const Value *, TypeSet> types_;
};

} // namespace xb::xir
