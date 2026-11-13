// Constant folding on xIR: machine operations on constants, the truth test of
// a constant or of a logical just made, arithmetic (also - and ~ of one) and
// comparisons of number constants (as the run-time operators compute them), comparisons of string
// constants for equality, ValType() of a value whose type is known, and
// branches on known conditions.
#include "xir/passes/Passes.h"

#include "opt/Fold.h"
#include "xir/Builder.h"
#include "xir/Types.h"

#include <optional>
#include <unordered_set>

namespace xb::xir {

namespace {

const Instruction *producedBy(const Value *v, Op op) {
  if (v->kind() != Value::Kind::Instruction) return nullptr;
  auto *i = static_cast<const Instruction *>(v);
  return i->op == op ? i : nullptr;
}

bool compare(IntPred p, int64_t a, int64_t b) {
  switch (p) {
  case IntPred::Eq: return a == b;
  case IntPred::Ne: return a != b;
  case IntPred::Lt: return a < b;
  case IntPred::Le: return a <= b;
  case IntPred::Gt: return a > b;
  case IntPred::Ge: return a >= b;
  }
  return false;
}

const Constant *asString(const Value *v) {
  const Constant *c = asConstant(v);
  return c && c->lit == Constant::Lit::String ? c : nullptr;
}

// A number constant as the literal it was made from (for the folding rules of opt/Fold).
std::optional<NumberExpr> asNumber(const Value *v) {
  const Constant *c = asConstant(v);
  if (!c || (c->lit != Constant::Lit::Integer && c->lit != Constant::Lit::Double)) return std::nullopt;
  return NumberExpr({}, c->lit == Constant::Lit::Integer, c->ival, c->dval, c->dec, c->width);
}

// ValType() of a value whose types all have one letter (XB_TYPE_LETTERS); not a string, which may be a memo field's
// value ("M"), nor a cell or reference.
std::optional<char> typeLetter(TypeSet t) {
  constexpr TypeSet kNot = typeBit(XB_T_STRING) | typeBit(XB_T_CELL) | typeBit(XB_T_REF);
  if (!t || (t & kNot)) return std::nullopt;
  std::optional<char> letter;
  for (int tag = 0; tag < XB_T_COUNT_; ++tag) {
    if (!(t & typeBit((xb_type)tag))) continue;
    if (letter && *letter != XB_TYPE_LETTERS[tag]) return std::nullopt;
    letter = XB_TYPE_LETTERS[tag];
  }
  return letter;
}

// The value `i` always gives, or null. Counters wrap like the machine
// arithmetic they lower to.
Value *foldedValue(Instruction &i, Builder &b, const TypeInfo &types) {
  const auto &ops = i.operands();
  switch (i.op) {
  case Op::Unary:
    if (std::optional<NumberExpr> x = asNumber(ops[0]))
      if (ExprPtr r = foldNumber({}, i.unop, *x, true)) return b.number(static_cast<const NumberExpr &>(*r));
    return nullptr;
  case Op::Binary: {
    if (std::optional<NumberExpr> x = asNumber(ops[0]), y = asNumber(ops[1]); x && y) {
      // Two values the program computes at run time: the run-time operator's result.
      if (ExprPtr r = foldNumbers({}, i.binop, *x, *y, true)) return b.number(static_cast<const NumberExpr &>(*r));
      std::optional<bool> r = compareNumbers(i.binop, *x, *y);
      return r ? b.logical(*r) : nullptr;
    }
    const Constant *x = asString(ops[0]), *y = asString(ops[1]);
    if (!x || !y) return nullptr;
    std::optional<bool> r = stringEquality(i.binop, x->str, y->str);
    return r ? b.logical(*r) : nullptr;
  }
  case Op::Call:
    if (i.callee.function || !i.callee.native || i.callee.name != "VALTYPE" || ops.size() != 1 || i.hasRest())
      return nullptr;
    if (std::optional<char> letter = typeLetter(types.of(ops[0]))) return b.string(std::string(1, *letter));
    return nullptr;
  case Op::IntAdd: case Op::IntSub: {
    const Constant *x = asConstant(ops[0]), *y = asConstant(ops[1]);
    if (!x || !y) return nullptr;
    uint64_t r = i.op == Op::IntAdd ? (uint64_t)x->ival + (uint64_t)y->ival : (uint64_t)x->ival - (uint64_t)y->ival;
    return b.i64((int64_t)r);
  }
  case Op::IntCmp: {
    const Constant *x = asConstant(ops[0]), *y = asConstant(ops[1]);
    return x && y ? b.boolean(compare(i.pred, x->ival, y->ival)) : nullptr;
  }
  case Op::BoolAnd:
    for (size_t k = 0; k < 2; ++k)
      if (const Constant *c = asBool(ops[k])) return c->logical ? ops[1 - k] : b.boolean(false);
    return nullptr;
  case Op::BoolNot:
    if (const Constant *c = asBool(ops[0])) return b.boolean(!c->logical);
    if (const Instruction *inner = producedBy(ops[0], Op::BoolNot)) return inner->operand(0);
    return nullptr;
  case Op::FromBool:
    if (const Constant *c = asBool(ops[0])) return b.logical(c->logical);
    return nullptr;
  case Op::Truth: // a logical cannot fail the test
    if (const Constant *c = asConstant(ops[0]); c && c->lit == Constant::Lit::Logical) return b.boolean(c->logical);
    if (const Instruction *made = producedBy(ops[0], Op::FromBool)) return made->operand(0);
    return nullptr;
  default:
    return nullptr;
  }
}

} // namespace

// A folded instruction is removed here: it may be one that can fail, like
// `truth`, proven not to with these operands.
bool foldConstants(Function &f) {
  Builder b(&f);
  CFG cfg(f);
  TypeInfo types(cfg);
  bool changed = false;
  for (auto &bb : f.blocks) {
    std::unordered_set<const Instruction *> folded;
    for (auto &ip : bb->insts) {
      Instruction &i = *ip;
      if (i.users().empty()) continue; // dead code goes in its own pass
      if (Value *v = foldedValue(i, b, types)) {
        i.replaceAllUsesWith(v);
        i.dropOperands();
        folded.insert(&i);
      }
    }
    if (!folded.empty()) {
      std::erase_if(bb->insts, [&](const std::unique_ptr<Instruction> &i) { return folded.count(i.get()) != 0; });
      changed = true;
    }
    Instruction *t = bb->terminator();
    if (t && t->op == Op::CondBr) {
      if (const Constant *c = asBool(t->operand(0))) {
        Target taken = t->targets[c->logical ? 0 : 1];
        b.jump(bb.get(), taken.block, taken.args);
        changed = true;
      }
    }
  }
  return changed;
}

} // namespace xb::xir
