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

TypeInfo::TypeInfo(const CFG &cfg) {
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

TypeSet TypeInfo::compute(const Instruction &i) const {
  switch (i.op) {
  case Op::NumDefaultDec: return kDouble;
  case Op::Binary: return binaryResult(i.binop, of(i.operand(0)), of(i.operand(1)));
  case Op::Unary: {
    if (i.unop == UnOp::Not) return kLogical;
    TypeSet a = of(i.operand(0)); // -INT64_MIN is a double
    return (a & kInt ? kNumberTypes : 0) | (a & kDouble);
  }
  case Op::FromBool: return kLogical;
  case Op::ArrayLit: return typeBit(XB_T_ARRAY);
  case Op::MakeBlock: return typeBit(XB_T_BLOCK);
  default: return kAnyType; // read from memory, or made by the runtime
  }
}

} // namespace xb::xir
