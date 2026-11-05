// The catalog of xIR operations: names and effects.
#include "xir/Ops.h"

#include <cstddef>
#include <iterator>

namespace xb::xir {

namespace {

// One row per operation, in the order of Op (checked below).
constexpr OpInfo kOps[] = {
    {Op::NumDefaultDec, "num.default_dec", ReadsState},
    {Op::Load, "load", ReadsVar | FailsByRef | ReadsHeap},
    {Op::Store, "store", WritesVar | FailsByRef | WritesHeap},
    {Op::Barrier, "barrier", WritesHeap},
    {Op::Unary, "unary", Fails},
    {Op::Binary, "binary", Fails | ReadsState | Allocates},
    {Op::Truth, "truth", Fails},
    {Op::FromBool, "from_bool", 0},
    {Op::Call, "call", Fails | RunsCode | Allocates | ReadsHeap | WritesHeap | ReadsState},
    {Op::ArrayLit, "array", Allocates},
    {Op::Index, "index", Fails | ReadsHeap},
    {Op::IndexSet, "index.set", Fails | WritesHeap},
    {Op::MakeBlock, "block", Allocates | WritesVar},
    {Op::ForTest, "for.test", Fails},
    {Op::EnumLen, "enum.len", Fails | ReadsHeap},
    {Op::EachInCell, "each.in_cell", ReadsVar | WritesVar | Allocates},
    {Op::EachEnter, "each.enter", ReadsVar | WritesVar | WritesHeap | Allocates},
    {Op::EachMove, "each.move", Fails | WritesVar | WritesHeap | Allocates},
    {Op::EachLeave, "each.leave", WritesVar | WritesHeap},
    {Op::RefVar, "ref", ReadsVar | WritesVar | Allocates},
    {Op::RefIndex, "ref.index", Fails | ReadsHeap | Allocates},
    {Op::SwitchCheck, "switch.check", Fails},
    {Op::SwitchMatch, "switch.match", 0},
    {Op::ParamsRef, "params.ref", ReadsVar},
    {Op::CallForward, "call.forward", Fails | RunsCode | Allocates | ReadsHeap | WritesHeap | ReadsState},
    {Op::IntAdd, "i.add", Commutative},
    {Op::IntSub, "i.sub", 0},
    {Op::IntCmp, "i.cmp", 0},
    {Op::BoolAnd, "and", Commutative},
    {Op::BoolNot, "not", 0},
    {Op::Br, "br", Terminator},
    {Op::CondBr, "cond_br", Terminator},
    {Op::Ret, "ret", Terminator},
};

constexpr bool rowsInOrder() {
  for (size_t k = 0; k < std::size(kOps); ++k)
    if (kOps[k].op != static_cast<Op>(k)) return false;
  return std::size(kOps) == static_cast<size_t>(Op::Ret) + 1;
}
static_assert(rowsInOrder(), "kOps has one row per Op, in order");

} // namespace

const OpInfo &opInfo(Op op) { return kOps[static_cast<size_t>(op)]; }

} // namespace xb::xir
