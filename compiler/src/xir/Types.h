#pragma once

#include "xir/Analysis.h"
#include "xir/IR.h"

#include "xbrt.h" // type tags

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace xb::xir {

// The types an item value may hold: one bit per type tag (xb_type).
using TypeSet = uint32_t;

constexpr TypeSet typeBit(xb_type t) { return TypeSet(1) << t; }
constexpr TypeSet kIntType = typeBit(XB_T_INT);
constexpr TypeSet kNumberTypes = kIntType | typeBit(XB_T_DOUBLE);
constexpr TypeSet kAnyType = (TypeSet(1) << XB_T_COUNT_) - 1;
// The tags whose value refers to a heap object (xb_type_is_heap).
constexpr TypeSet kHeapTypes = ((TypeSet(1) << (XB_T_HEAP_LAST + 1)) - 1) & ~((TypeSet(1) << XB_T_HEAP_FIRST) - 1);

// The result types of a binary operator whose operands may hold the types `a`
// and `b`, as the runtime's operators make them (core/ops.c); an operand pair
// that is an error adds nothing.
TypeSet binaryResult(BinOp op, TypeSet a, TypeSet b);
// Whether such operands can make it fail: an operand pair it does not take,
// or a divisor that may be zero (not when it is a non-zero constant).
bool binaryMayFail(BinOp op, TypeSet a, TypeSet b, bool divisorMayBeZero);

// Type inference over a function's SSA values: the set of types each item
// value may hold. An optimistic fixpoint: block parameters start empty and
// grow with the values their branches pass, operations follow their operands
// (binaryResult; an operator on an object may run its class's method: any
// type), a call of a function of the module gives its return types
// (Function::returnTypes), and anything else read from memory (a variable, an
// element, another call's result) may hold any type. The handler of an operation's error may
// substitute a value of any type for its result, so an operation that may fail
// that way may produce any type, unless subst.check follows it; the lowering
// keeps values it finds without heap references out of the collector's sight.
enum class Substitution : uint8_t {
  Any,  // as the program runs
  None, // as if no error block substituted anything: what checking every operation would give (the analysis of
        // passes/TypedContinuations.cpp)
};

// The types the result of a call of `f` may hold: those of the values its RETURNs give (NIL for none), as type
// inference finds them with the return types of the functions it calls (Function::returnTypes); every type for a
// generator's resume function, which returns between steps.
TypeSet returnTypes(const Function &f);

// Int( x / k ) of the runtime's Int(), k an integer constant other than -1, 0 and 1: x (else null). Of an integer x,
// the quotient is within 2^62 even as a double rounds it, so Int() gives an integer.
const Value *intQuotientDividend(const Instruction &call);

// + - * (a binary add, sub or mul): of two integers, an integer unless it overflows int64.
bool arithmetic(const Instruction &i);

// The integers an item value may hold when it holds one (its XB_T_INT case): a closed interval, empty while nothing
// is known (the optimistic start of the inference).
struct IntRange {
  int64_t lo = INT64_MAX, hi = INT64_MIN;
  bool empty() const { return lo > hi; }
  static IntRange full() { return {INT64_MIN, INT64_MAX}; }
  static IntRange of(int64_t v) { return {v, v}; }
  bool operator==(const IntRange &) const = default;
};

// The types and, for the values that may be integers, the ranges of those integers: an interval per value, from the
// integer constants through + - * of integers, unary -, & with a constant, Int() of an integer divided by a constant, a
// runtime function's RESULT INTEGER RANGE, a CHOOSES call (the hull of its arguments'), table.index (its table's
// integers, for Substitution::None), and block parameters (widened once one grows twice: to the next of the function's
// integer constants, their neighbours, or an end of int64). Tests bound them (`Bound`): a FOR's counter is no greater
// than its limit (no less with a negative step) where its true branch leads, when the limit and the step are integers
// and the step's sign is known; either operand of < <= > >= of two numbers is bounded by the other, an integer, on
// either branch. A bound holds in the blocks its branch dominates when the branch is the only way into its block from
// outside it, and for the arguments the branch passes. + - * of two integers whose ranges cannot overflow give an
// integer only (`fits`), and so does Int() of an integer divided by a constant other than -1, 0 and 1. One optimistic
// fixpoint for both, then the ranges narrowed and the types inferred again with them (docs/xir.md, Types).
class TypeInfo {
public:
  // The function the CFG was built for; `assumedChecks` (with Substitution::Any): operations taken as if subst.check
  // followed each (the checks passes/TypedContinuations.cpp has chosen so far).
  explicit TypeInfo(const CFG &cfg, Substitution s = Substitution::Any,
                    const std::unordered_set<const Instruction *> *assumedChecks = nullptr);
  TypeSet of(const Value *v) const;
  // The operation may fail with an error whose block may substitute its result.
  bool maySubstitute(const Instruction &i) const;
  // Never refers to a heap object: the collector need not see it.
  bool noHeap(const Value *v) const { return (of(v) & kHeapTypes) == 0; }
  // A value a guard (subst.check with a mask) checks that may refer to a heap object until its check, across a
  // safepoint (a call's pending test may collect): one its operation may give another type to (an error substituted,
  // a value read from memory), which keeps a home the collector sees; + - * of two integers, guarded against an
  // overflow's double, never does.
  bool guardNeedsHome(const Instruction &i) const;
  // + - * whose integer operands' ranges keep the result within int64: of two integers, an integer (the lowering still
  // tests the overflow, a form LLVM simplifies best).
  bool fits(const Instruction &i) const { return fits_.count(&i) != 0; }

private:
  // A test whose branch `edge` (0 true, 1 false) to `body` bounds two numbers: `x op y` holds on it. A FOR test's
  // true branch (`step` its step, whose sign makes op <= or >=), or a comparison's either branch (`step` null).
  struct Bound {
    const Value *x, *y, *step;
    BinOp op;
    const BasicBlock *test, *body;
    size_t edge;
  };
  void findBounds();
  void findThresholds();
  void infer(bool withRanges);
  void narrow();
  TypeSet compute(const Instruction &i) const;
  IntRange computeRange(const Instruction &i) const;
  IntRange rangeOf(const Value *v, const BasicBlock *at) const;
  IntRange bounded(IntRange r, const Bound &b, const Value *v) const; // r of v, where b holds
  IntRange argRange(const Value *arg, const BasicBlock *from, size_t edge) const; // passed on from's branch edge
  bool noOverflow(const Instruction &i) const; // + - * of two integers: the result is one
  bool intQuotient(const Instruction &call, IntRange *r) const; // Int( i / k ): an integer, in the range *r
  const CFG *cfg_; // while the constructor runs, then null
  Substitution substitution_;
  const std::unordered_set<const Instruction *> *assumedChecks_;
  std::unordered_map<const Value *, TypeSet> types_;
  std::unordered_map<const Value *, IntRange> ranges_;
  // The bounds of each value (as `x` or `y`) whose branch is the only way into its block from outside it: they hold
  // where it dominates; at most kMaxBoundsPerValue of each value, the first in reverse postorder (a value compared in
  // thousands of places would make each look-up as long).
  std::unordered_map<const Value *, std::vector<Bound>> bounds_;
  std::unordered_map<const BasicBlock *, std::vector<Bound>> edgeBounds_; // every one, by its test's block
  std::vector<int64_t> thresholds_; // where a widened range stops, sorted
  std::unordered_set<const Instruction *> fits_;
};

} // namespace xb::xir
