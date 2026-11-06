// Type inference over xIR values (see Types.h).
#include "xir/Types.h"

namespace xb::xir {

namespace {

constexpr TypeSet kInt = typeBit(XB_T_INT);
constexpr TypeSet kString = typeBit(XB_T_STRING);
constexpr TypeSet kDate = typeBit(XB_T_DATE);
constexpr TypeSet kDouble = typeBit(XB_T_DOUBLE);
constexpr TypeSet kLogical = typeBit(XB_T_LOGICAL);

// The cases of each arithmetic operator that give a result, from core/ops.c:
// operands of the types `a` and `b` give a value of the types `result`.
struct BinaryRule {
  BinOp op;
  TypeSet a, b, result;
};

constexpr BinaryRule kBinaryRules[] = {
    {BinOp::Add, kInt, kInt, kNumberTypes}, // an integer overflow gives a double
    {BinOp::Add, kDouble, kNumberTypes, kDouble},
    {BinOp::Add, kNumberTypes, kDouble, kDouble},
    {BinOp::Add, kString, kString, kString},
    {BinOp::Add, kDate, kNumberTypes | kDate, kDate},
    {BinOp::Add, kNumberTypes, kDate, kDate},
    {BinOp::Sub, kInt, kInt, kNumberTypes},
    {BinOp::Sub, kDouble, kNumberTypes, kDouble},
    {BinOp::Sub, kNumberTypes, kDouble, kDouble},
    {BinOp::Sub, kString, kString, kString},
    {BinOp::Sub, kDate, kDate, kNumberTypes},
    {BinOp::Sub, kDate, kNumberTypes, kDate},
    {BinOp::Mul, kInt, kInt, kNumberTypes},
    {BinOp::Mul, kDouble, kNumberTypes, kDouble},
    {BinOp::Mul, kNumberTypes, kDouble, kDouble},
    {BinOp::Div, kNumberTypes, kNumberTypes, kDouble},
    {BinOp::Mod, kNumberTypes, kNumberTypes, kDouble},
    {BinOp::Pow, kNumberTypes, kNumberTypes, kDouble},
    {BinOp::BitAnd, kNumberTypes, kNumberTypes, kInt}, // numbers taken as integers
    {BinOp::BitOr, kNumberTypes, kNumberTypes, kInt},
    {BinOp::BitXor, kNumberTypes, kNumberTypes, kInt},
    {BinOp::Shl, kNumberTypes, kNumberTypes, kInt},
    {BinOp::Shr, kNumberTypes, kNumberTypes, kInt},
};

TypeSet literalType(const Constant &c) {
  switch (c.lit) {
  case Constant::Lit::Nil: return typeBit(XB_T_NIL);
  case Constant::Lit::Logical: return kLogical;
  case Constant::Lit::Integer: return typeBit(XB_T_INT);
  case Constant::Lit::Double: return kDouble;
  case Constant::Lit::String: return kString;
  case Constant::Lit::Bool: case Constant::Lit::I64: break; // machine values
  }
  return 0;
}

} // namespace

namespace {

// Whether one operand type pair of `op` gives a result instead of an error
// (core/ops.c): the comparisons take two numbers, strings, logicals or dates
// (xb_compare), = <> and == also NIL against anything, == also the same array,
// block, object or hash type; $ takes strings, or a key and a hash; the arithmetic follows kBinaryRules.
bool pairWorks(BinOp op, TypeSet a, TypeSet b) {
  const TypeSet comparable[] = {kNumberTypes, kString, kLogical, kDate};
  auto compares = [&] {
    for (TypeSet kind : comparable)
      if ((a & kind) && (b & kind)) return true;
    return false;
  };
  constexpr TypeSet kNil = typeBit(XB_T_NIL);
  constexpr TypeSet kHash = typeBit(XB_T_HASH);
  constexpr TypeSet kIdentity = typeBit(XB_T_ARRAY) | typeBit(XB_T_BLOCK) | typeBit(XB_T_OBJECT) | kHash;
  switch (op) {
  case BinOp::Eq: case BinOp::NotEq: return ((a | b) & kNil) || compares();
  case BinOp::ExactEq: return ((a | b) & kNil) || (a == b && (a & kIdentity)) || compares();
  case BinOp::Lt: case BinOp::Le: case BinOp::Gt: case BinOp::Ge: return compares();
  case BinOp::In: return (a == kString && b == kString) || (b == kHash && (a & (kString | kNumberTypes | kDate)));
  default:
    for (const BinaryRule &rule : kBinaryRules)
      if (rule.op == op && (a & rule.a) && (b & rule.b)) return true;
    return false;
  }
}

} // namespace

bool binaryMayFail(BinOp op, TypeSet a, TypeSet b, bool divisorMayBeZero) {
  if ((op == BinOp::Div || op == BinOp::Mod) && (b & kNumberTypes) && divisorMayBeZero) return true;
  for (unsigned ta = 0; ta < XB_T_COUNT_; ++ta)
    for (unsigned tb = 0; tb < XB_T_COUNT_; ++tb)
      if ((a & typeBit(xb_type(ta))) && (b & typeBit(xb_type(tb))) &&
          !pairWorks(op, typeBit(xb_type(ta)), typeBit(xb_type(tb))))
        return true;
  return false;
}

TypeSet binaryResult(BinOp op, TypeSet a, TypeSet b) {
  switch (op) {
  case BinOp::Eq: case BinOp::ExactEq: case BinOp::NotEq: case BinOp::Lt: case BinOp::Le: case BinOp::Gt:
  case BinOp::Ge: case BinOp::In:
    return kLogical;
  default:
    break;
  }
  TypeSet r = 0;
  for (const BinaryRule &rule : kBinaryRules)
    if (rule.op == op && (a & rule.a) && (b & rule.b)) r |= rule.result;
  return r;
}

TypeInfo::TypeInfo(const CFG &cfg, Substitution substitution) : substitution_(substitution) {
  // Reverse postorder sees definitions before their uses except along back
  // edges, which the repetition settles; sets only grow, so it ends.
  for (bool changed = true; changed;) {
    changed = false;
    for (BasicBlock *b : cfg.order())
      for (auto &ip : b->insts) {
        const Instruction &i = *ip;
        if (i.type() == Type::Item) {
          TypeSet t = compute(i);
          TypeSet &cur = types_[&i];
          if ((cur | t) != cur) {
            cur |= t;
            changed = true;
          }
        }
        for (const Target &tg : i.targets)
          for (size_t k = 0; k < tg.args.size(); ++k) {
            const BlockParam *p = tg.block->params[k].get();
            if (p->type() != Type::Item) continue;
            TypeSet t = of(tg.args[k]);
            TypeSet &cur = types_[p];
            if ((cur | t) != cur) {
              cur |= t;
              changed = true;
            }
          }
      }
  }
}

TypeSet TypeInfo::of(const Value *v) const {
  if (const Constant *c = asConstant(v)) return literalType(*c);
  auto it = types_.find(v);
  return it == types_.end() ? 0 : it->second; // not reached yet (or never: unreachable code)
}

namespace {

// The types an operation gives itself, and whether it may fail with an error
// whose block may substitute its result.
struct OwnResult {
  TypeSet types;
  bool substitutable;
};

OwnResult ownResult(const Instruction &i, const TypeInfo &t) {
  constexpr TypeSet kSwitchTypes = kInt | kString;
  switch (i.op) {
  case Op::NumDefaultDec: return {kDouble, false};
  case Op::Binary: {
    TypeSet a = t.of(i.operand(0)), b = t.of(i.operand(1));
    const Constant *c = asConstant(i.operand(1));
    bool nonZero = c && ((c->lit == Constant::Lit::Integer && c->ival != 0) ||
                         (c->lit == Constant::Lit::Double && c->dval != 0.0));
    return {binaryResult(i.binop, a, b), binaryMayFail(i.binop, a, b, !nonZero)};
  }
  case Op::Unary: {
    TypeSet a = t.of(i.operand(0));
    if (i.unop == UnOp::Not) return {kLogical, (a & ~kLogical) != 0};
    if (i.unop == UnOp::BitNot) return {a & kNumberTypes ? kInt : 0, (a & ~kNumberTypes) != 0};
    return {(a & kInt ? kNumberTypes : 0) | (a & kDouble), (a & ~kNumberTypes) != 0}; // -INT64_MIN is a double
  }
  case Op::SwitchCheck: {
    TypeSet a = t.of(i.operand(0));
    return {a & kSwitchTypes, (a & ~kSwitchTypes) != 0};
  }
  case Op::FromBool: return {kLogical, false};
  case Op::ArrayLit: return {typeBit(XB_T_ARRAY), false};
  case Op::HashLit: return {typeBit(XB_T_HASH), false};
  case Op::MakeBlock: return {typeBit(XB_T_BLOCK), false};
  default: return {kAnyType, false}; // read from memory, or made by the runtime
  }
}

// Checked by subst.check: a substituted value of another type leaves the
// function's typed code there (passes/TypedContinuations.cpp).
bool checked(const Instruction &i) {
  for (const Instruction *u : i.users())
    if (u->op == Op::SubstCheck) return true;
  return false;
}

} // namespace

bool TypeInfo::maySubstitute(const Instruction &i) const { return ownResult(i, *this).substitutable; }

TypeSet TypeInfo::compute(const Instruction &i) const {
  OwnResult r = ownResult(i, *this);
  bool any = r.substitutable && substitution_ == Substitution::Any && !checked(i);
  return r.types | (any ? kAnyType : 0);
}

} // namespace xb::xir
