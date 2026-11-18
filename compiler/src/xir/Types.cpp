// Type inference over xIR values (see Types.h).
#include "xir/Types.h"

#include <llvm/Support/CheckedArithmetic.h>

#include <algorithm>

namespace xb::xir {

namespace {

constexpr int kWidenAfter = 2;  // a block parameter's range grows this many times before it is widened
constexpr int kNarrowRounds = 2; // descending rounds after the fixpoint
constexpr size_t kMaxBoundsPerValue = 16;
constexpr TypeSet kString = typeBit(XB_T_STRING);
constexpr TypeSet kDate = typeBit(XB_T_DATE);
constexpr TypeSet kDouble = typeBit(XB_T_DOUBLE);
constexpr TypeSet kLogical = typeBit(XB_T_LOGICAL);
constexpr TypeSet kObject = typeBit(XB_T_OBJECT);

// The cases of each arithmetic operator that give a result, from core/ops.c:
// operands of the types `a` and `b` give a value of the types `result`.
struct BinaryRule {
  BinOp op;
  TypeSet a, b, result;
};

constexpr BinaryRule kBinaryRules[] = {
    {BinOp::Add, kIntType, kIntType, kNumberTypes}, // an integer overflow gives a double
    {BinOp::Add, kDouble, kNumberTypes, kDouble},
    {BinOp::Add, kNumberTypes, kDouble, kDouble},
    {BinOp::Add, kString, kString, kString},
    {BinOp::Add, kDate, kNumberTypes | kDate, kDate},
    {BinOp::Add, kNumberTypes, kDate, kDate},
    {BinOp::Sub, kIntType, kIntType, kNumberTypes},
    {BinOp::Sub, kDouble, kNumberTypes, kDouble},
    {BinOp::Sub, kNumberTypes, kDouble, kDouble},
    {BinOp::Sub, kString, kString, kString},
    {BinOp::Sub, kDate, kDate, kNumberTypes},
    {BinOp::Sub, kDate, kNumberTypes, kDate},
    {BinOp::Mul, kIntType, kIntType, kNumberTypes},
    {BinOp::Mul, kDouble, kNumberTypes, kDouble},
    {BinOp::Mul, kNumberTypes, kDouble, kDouble},
    {BinOp::Div, kNumberTypes, kNumberTypes, kDouble},
    {BinOp::Mod, kNumberTypes, kNumberTypes, kDouble},
    {BinOp::Pow, kNumberTypes, kNumberTypes, kDouble},
    {BinOp::BitAnd, kNumberTypes, kNumberTypes, kIntType}, // numbers taken as integers
    {BinOp::BitOr, kNumberTypes, kNumberTypes, kIntType},
    {BinOp::BitXor, kNumberTypes, kNumberTypes, kIntType},
    {BinOp::Shl, kNumberTypes, kNumberTypes, kIntType},
    {BinOp::Shr, kNumberTypes, kNumberTypes, kIntType},
};

TypeSet literalType(const Constant &c) {
  switch (c.lit) {
  case Constant::Lit::Nil: return typeBit(XB_T_NIL);
  case Constant::Lit::Logical: return kLogical;
  case Constant::Lit::Integer: return kIntType;
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
// block, object, hash or function reference type, and an array with an object; $ takes strings, or a key and a hash
// (a hash with a hash may fail: one of more than one entry); the arithmetic follows kBinaryRules.
bool pairWorks(BinOp op, TypeSet a, TypeSet b) {
  const TypeSet comparable[] = {kNumberTypes, kString, kLogical, kDate};
  auto compares = [&] {
    for (TypeSet kind : comparable)
      if ((a & kind) && (b & kind)) return true;
    return false;
  };
  constexpr TypeSet kNil = typeBit(XB_T_NIL);
  constexpr TypeSet kHash = typeBit(XB_T_HASH);
  constexpr TypeSet kArrayLike = typeBit(XB_T_ARRAY) | kObject; // one kind for == (core/ops.c)
  constexpr TypeSet kIdentity = kArrayLike | typeBit(XB_T_BLOCK) | kHash | typeBit(XB_T_SYMBOL);
  switch (op) {
  case BinOp::Eq: case BinOp::NotEq: return ((a | b) & kNil) || compares();
  case BinOp::ExactEq:
    return ((a | b) & kNil) || (a == b && (a & kIdentity)) || ((a | b) & ~kArrayLike) == 0 || compares();
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

bool arithmetic(const Instruction &i) {
  return i.op == Op::Binary && (i.binop == BinOp::Add || i.binop == BinOp::Sub || i.binop == BinOp::Mul);
}

namespace {

// Int( x ) of one argument (the runtime's), not spread from an array.
bool intCall(const Instruction &i) {
  return i.op == Op::Call && !i.callee.function && i.callee.native && i.callee.name == "INT" &&
         i.operands().size() == 1 && !i.hasRest();
}

IntRange hull(IntRange a, IntRange b) {
  if (a.empty()) return b;
  if (b.empty()) return a;
  return {std::min(a.lo, b.lo), std::max(a.hi, b.hi)};
}

// The results of `op` on two integers of the ranges `a` and `b` (non-empty): the hull of its corners, the extremes of
// + - and * over intervals, and whether every one fits int64; when one does not, the integers the operation can give
// are any (the others are doubles).
IntRange arithmeticRange(BinOp op, IntRange a, IntRange b, bool *fits) {
  const int64_t c[4] = {a.lo, a.lo, a.hi, a.hi}, d[4] = {b.lo, b.hi, b.lo, b.hi};
  IntRange r;
  for (int k = 0; k < 4; ++k) {
    std::optional<int64_t> v = op == BinOp::Add   ? llvm::checkedAdd(c[k], d[k])
                               : op == BinOp::Sub ? llvm::checkedSub(c[k], d[k])
                                                  : llvm::checkedMul(c[k], d[k]);
    if (!v) {
      *fits = false;
      return IntRange::full();
    }
    r = hull(r, IntRange::of(*v));
  }
  *fits = true;
  return r;
}

constexpr int64_t kExactDoubles = int64_t(1) << XB_DBL_EXACT_BITS; // the integers a double holds exactly

} // namespace

TypeInfo::TypeInfo(const CFG &cfg, Substitution substitution) : cfg_(&cfg), substitution_(substitution) {
  findBounds();
  findThresholds();
  // Types and ranges together, from nothing (optimistic): a value found an integer of a range gives one to what is
  // computed from it (Int( a / 16 ) of a counter is an integer, and so the counter's next value), and each only grows;
  // a block parameter's range that grows again after twice goes on that side to the next threshold (widening).
  infer(true);
  // The ranges narrowed (each again from the others, twice: what widening gave a counter its test bounds), then the
  // types again with those ranges, which only shrink.
  for (int round = 0; round < kNarrowRounds; ++round) narrow();
  types_.clear();
  infer(false);
  for (BasicBlock *b : cfg.order())
    for (auto &i : b->insts)
      if (arithmetic(*i) && noOverflow(*i)) fits_.insert(i.get());
  ranges_.clear();
  bounds_.clear();
  edgeBounds_.clear();
  thresholds_.clear();
  cfg_ = nullptr;
}

void TypeInfo::infer(bool withRanges) {
  // Reverse postorder sees definitions before their uses except along back edges, which the repetition settles;
  // sets and ranges only grow (ranges widened), so it ends.
  std::unordered_map<const Value *, int> grown;
  for (bool changed = true; changed;) {
    changed = false;
    for (BasicBlock *b : cfg_->order())
      for (auto &ip : b->insts) {
        const Instruction &i = *ip;
        if (i.type() == Type::Item) {
          TypeSet t = compute(i);
          TypeSet &cur = types_[&i];
          if ((cur | t) != cur) {
            cur |= t;
            changed = true;
          }
          if (withRanges) {
            IntRange &r = ranges_[&i];
            IntRange h = hull(r, computeRange(i));
            if (!(h == r)) {
              r = h;
              changed = true;
            }
          }
        }
        for (size_t e = 0; e < i.targets.size(); ++e) {
          const Target &tg = i.targets[e];
          for (size_t k = 0; k < tg.args.size(); ++k) {
            const BlockParam *p = tg.block->params[k].get();
            if (p->type() != Type::Item) continue;
            TypeSet t = of(tg.args[k]);
            TypeSet &cur = types_[p];
            if ((cur | t) != cur) {
              cur |= t;
              changed = true;
            }
            if (!withRanges || !(cur & kIntType)) continue;
            IntRange &r = ranges_[p];
            IntRange h = hull(r, argRange(tg.args[k], b, e));
            if (h == r) continue;
            if (!r.empty() && ++grown[p] > kWidenAfter) {
              if (h.lo < r.lo) h.lo = *std::prev(std::upper_bound(thresholds_.begin(), thresholds_.end(), h.lo));
              if (h.hi > r.hi) h.hi = *std::lower_bound(thresholds_.begin(), thresholds_.end(), h.hi);
            }
            r = h;
            changed = true;
          }
        }
      }
  }
}

void TypeInfo::narrow() {
  for (BasicBlock *b : cfg_->order())
    for (auto &ip : b->insts)
      if (ip->type() == Type::Item) ranges_[ip.get()] = computeRange(*ip);
  std::unordered_map<const Value *, IntRange> params;
  for (BasicBlock *b : cfg_->order())
    for (auto &ip : b->insts)
      for (size_t e = 0; e < ip->targets.size(); ++e) {
        const Target &tg = ip->targets[e];
        for (size_t k = 0; k < tg.args.size(); ++k) {
          const BlockParam *p = tg.block->params[k].get();
          if (p->type() == Type::Item && (of(p) & kIntType)) params[p] = hull(params[p], argRange(tg.args[k], b, e));
        }
      }
  for (auto &[p, r] : params) ranges_[p] = r;
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
  constexpr TypeSet kSwitchTypes = kIntType | kString;
  switch (i.op) {
  case Op::NumDefaultDec: return {kDouble, false};
  case Op::Binary: {
    TypeSet a = t.of(i.operand(0)), b = t.of(i.operand(1));
    const Constant *c = asConstant(i.operand(1));
    bool nonZero = c && ((c->lit == Constant::Lit::Integer && c->ival != 0) ||
                         (c->lit == Constant::Lit::Double && c->dval != 0.0));
    // An object's class may define the operator (core/ops.c), whose method gives the result: where no error can
    // come of the operands (an equality with NIL, == of two objects), that result is of any type; elsewhere the
    // operator without a method fails, which already makes it so.
    bool equality = i.binop == BinOp::Eq || i.binop == BinOp::NotEq || i.binop == BinOp::ExactEq;
    TypeSet r = equality && (a & kObject) ? kAnyType : binaryResult(i.binop, a, b);
    return {r, binaryMayFail(i.binop, a, b, !nonZero)};
  }
  case Op::Unary: {
    TypeSet a = t.of(i.operand(0));
    if (i.unop == UnOp::Not) return {kLogical, (a & ~kLogical) != 0};
    if (i.unop == UnOp::BitNot) return {a & kNumberTypes ? kIntType : 0, (a & ~kNumberTypes) != 0};
    return {(a & kIntType ? kNumberTypes : 0) | (a & kDouble), (a & ~kNumberTypes) != 0}; // -INT64_MIN is a double
  }
  case Op::SwitchCheck: {
    TypeSet a = t.of(i.operand(0));
    return {a & kSwitchTypes, (a & ~kSwitchTypes) != 0};
  }
  case Op::FromBool: return {kLogical, false};
  case Op::ObjectSet: return {t.of(i.operand(1)), false}; // the value assigned
  case Op::ClassNew: return {typeBit(XB_T_OBJECT), false};
  case Op::ArrayLit: return {typeBit(XB_T_ARRAY), false};
  case Op::HashLit: return {typeBit(XB_T_HASH), false};
  case Op::MakeBlock: return {typeBit(XB_T_BLOCK), false};
  case Op::FuncRef: return {typeBit(XB_T_SYMBOL), false};
  case Op::Call:
  case Op::CallForward: // what the module's function returns (its errors are its own: none substitutes the call)
    if (!i.callee.function && i.callee.resultTypes) // a runtime function's (RESULT), unless an error substitutes it
      return {i.callee.resultTypes & kAnyType, true};
    return {i.callee.function ? i.callee.function->returnTypes & kAnyType : kAnyType, false};
  case Op::Load: { // a parameter of a typed copy, which no store changes: the types its calls pass
    const Function *f = i.parent->parent;
    const Var *v = i.var;
    if (isParameter(v) && v->index < f->paramTypes.size() && !v->shared && i.access == Access::Direct)
      return {f->paramTypes[v->index] & kAnyType, false};
    return {kAnyType, false};
  }
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

// The types a guard (subst.check with a mask) lets on: every other use is past it.
TypeSet guardMask(const Instruction &i) {
  for (const Instruction *u : i.users())
    if (u->op == Op::SubstCheck && u->imm != 0) return (TypeSet)u->imm;
  return kAnyType;
}

} // namespace


TypeSet returnTypes(const Function &f) {
  CFG cfg(f);
  TypeInfo types(cfg);
  TypeSet r = 0;
  for (BasicBlock *b : cfg.order())
    for (auto &i : b->insts) {
      if (isSuspension(i->op)) return kAnyType;
      if (i->op == Op::Ret) r |= i->operands().empty() ? typeBit(XB_T_NIL) : types.of(i->operand(0));
    }
  return r;
}

bool TypeInfo::maySubstitute(const Instruction &i) const { return ownResult(i, *this).substitutable; }

bool TypeInfo::guardNeedsHome(const Instruction &i) const {
  if (guardMask(i) == kAnyType) return false;
  OwnResult r = ownResult(i, *this);
  return r.substitutable || (r.types & kHeapTypes) != 0;
}

TypeSet TypeInfo::compute(const Instruction &i) const {
  OwnResult r = ownResult(i, *this);
  if (arithmetic(i) && noOverflow(i)) { // the integers' case gives an integer only
    TypeSet a = of(i.operand(0)), b = of(i.operand(1));
    r.types = binaryResult(i.binop, a, b & ~kIntType) | binaryResult(i.binop, a & ~kIntType, b & kIntType) |
              ((a & kIntType) && (b & kIntType) ? kIntType : 0);
  }
  if (intCall(i) && intQuotient(i, nullptr)) return kIntType & guardMask(i); // a number: no error
  bool any = r.substitutable && substitution_ == Substitution::Any && !checked(i);
  return (r.types | (any ? kAnyType : 0)) & guardMask(i);
}

IntRange TypeInfo::bounded(IntRange r, const Bound &b, const Value *v) const {
  if ((b.x != v && b.y != v) || r.empty()) return r;
  BinOp op = b.op;
  if (b.step) { // a FOR test that cannot fail: its limit and step integers, the step's sign known
    if (of(b.x) != kIntType || of(b.y) != kIntType || of(b.step) != kIntType) return r;
    IntRange step = rangeOf(b.step, b.test);
    if (step.empty() || (step.lo < 0 && step.hi >= 0)) return r;
    op = step.hi < 0 ? BinOp::Ge : BinOp::Le;
  } else if ((of(b.x) & ~kNumberTypes) || (of(b.y) & ~kNumberTypes)) { // a comparison that may fail
    return r;
  }
  const Value *other = b.x == v ? b.y : b.x;
  if (b.y == v) // y op x: the same with the operands swapped
    op = op == BinOp::Lt ? BinOp::Gt : op == BinOp::Le ? BinOp::Ge : op == BinOp::Gt ? BinOp::Lt : BinOp::Le;
  if (of(other) != kIntType) return r; // the integers it holds bound v's (a double would bound them less simply)
  IntRange o = rangeOf(other, b.test);
  if (o.empty()) return r;
  switch (op) { // v op other
  case BinOp::Lt: if (o.hi > INT64_MIN) r.hi = std::min(r.hi, o.hi - 1); break;
  case BinOp::Le: r.hi = std::min(r.hi, o.hi); break;
  case BinOp::Gt: if (o.lo < INT64_MAX) r.lo = std::max(r.lo, o.lo + 1); break;
  default: r.lo = std::max(r.lo, o.lo); break;
  }
  return r;
}

IntRange TypeInfo::argRange(const Value *arg, const BasicBlock *from, size_t edge) const {
  IntRange r = rangeOf(arg, from);
  auto it = edgeBounds_.find(from);
  if (it != edgeBounds_.end())
    for (const Bound &b : it->second)
      if (b.edge == edge) r = bounded(r, b, arg);
  return r;
}

IntRange TypeInfo::rangeOf(const Value *v, const BasicBlock *at) const {
  if (const Constant *c = asConstant(v)) return c->lit == Constant::Lit::Integer ? IntRange::of(c->ival) : IntRange{};
  auto it = ranges_.find(v);
  IntRange r = it == ranges_.end() ? IntRange{} : it->second;
  auto mine = bounds_.find(v);
  if (mine != bounds_.end())
    for (const Bound &b : mine->second)
      if (cfg_->dominates(b.body, at)) r = bounded(r, b, v);
  return r;
}

bool TypeInfo::noOverflow(const Instruction &i) const {
  IntRange a = rangeOf(i.operand(0), i.parent), b = rangeOf(i.operand(1), i.parent);
  if (a.empty() || b.empty()) return true; // no integer pair reaches it
  bool fits;
  arithmeticRange(i.binop, a, b, &fits);
  return fits;
}

const Value *intQuotientDividend(const Instruction &call) {
  if (!intCall(call)) return nullptr;
  const auto *div = dynamic_cast<const Instruction *>(call.operand(0));
  if (!div || div->op != Op::Binary || div->binop != BinOp::Div) return nullptr;
  const Constant *k = asConstant(div->operand(1));
  if (!k || k->lit != Constant::Lit::Integer || (k->ival > -2 && k->ival < 2)) return nullptr;
  return div->operand(0);
}

bool TypeInfo::intQuotient(const Instruction &call, IntRange *r) const {
  const Value *dividend = intQuotientDividend(call);
  if (!dividend || of(dividend) != kIntType) return false;
  if (!r) return true;
  const auto *div = static_cast<const Instruction *>(call.operand(0));
  const int64_t k = asConstant(div->operand(1))->ival;
  IntRange x = rangeOf(dividend, div->parent);
  *r = IntRange::full();
  if (x.empty()) {
    *r = {};
  } else if (x.lo > -kExactDoubles && x.hi < kExactDoubles) { // the quotient a double gives exactly
    int64_t a = x.lo / k, b = x.hi / k; // truncated, as Int() truncates it
    *r = {std::min(a, b), std::max(a, b)};
  }
  return true;
}

IntRange TypeInfo::computeRange(const Instruction &i) const {
  TypeSet own = of(&i);
  if (!(own & kIntType)) return {};
  OwnResult r = ownResult(i, *this);
  // a handler may substitute any integer (a check lets one through: it is of the operation's own types)
  if (r.substitutable && substitution_ == Substitution::Any && !(intCall(i) && intQuotient(i, nullptr)))
    return IntRange::full();
  if (arithmetic(i)) {
    TypeSet a = of(i.operand(0)), b = of(i.operand(1));
    if (((a | b) & ~kNumberTypes) != 0) return IntRange::full(); // dates: their differences
    IntRange x = rangeOf(i.operand(0), i.parent), y = rangeOf(i.operand(1), i.parent);
    if (x.empty() || y.empty()) return {};
    bool fits;
    return arithmeticRange(i.binop, x, y, &fits);
  }
  if (i.op == Op::Binary && i.binop == BinOp::BitAnd) { // with a constant of no sign bit: within it
    for (size_t k = 0; k < 2; ++k)
      if (const Constant *c = asConstant(i.operand(k)); c && c->lit == Constant::Lit::Integer && c->ival >= 0)
        return {0, c->ival};
    return IntRange::full();
  }
  if (i.op == Op::Unary && i.unop == UnOp::Neg) { // -INT64_MIN is a double
    IntRange x = rangeOf(i.operand(0), i.parent);
    if (x.empty()) return {};
    return {x.hi == INT64_MIN ? INT64_MAX : -x.hi, x.lo == INT64_MIN ? INT64_MAX : -x.lo};
  }
  if (intCall(i)) {
    IntRange q;
    if (intQuotient(i, &q)) return q;
    if (of(i.operand(0)) == kIntType) return rangeOf(i.operand(0), i.parent); // Int() of an integer: itself
  }
  return IntRange::full();
}

void TypeInfo::findThresholds() {
  // The integer constants of the function, each with its neighbours (a FOR's last value plus its step: the counter's
  // value after the loop), and the ends of int64: the points widening stops at.
  thresholds_ = {INT64_MIN, INT64_MAX};
  auto add = [&](const Value *v) {
    const Constant *c = asConstant(v);
    if (!c || c->lit != Constant::Lit::Integer) return;
    thresholds_.push_back(c->ival);
    if (c->ival > INT64_MIN) thresholds_.push_back(c->ival - 1);
    if (c->ival < INT64_MAX) thresholds_.push_back(c->ival + 1);
  };
  for (BasicBlock *b : cfg_->order())
    for (auto &ip : b->insts) {
      for (const Value *v : ip->operands()) add(v);
      for (const Target &tg : ip->targets)
        for (const Value *v : tg.args) add(v);
    }
  std::sort(thresholds_.begin(), thresholds_.end());
  thresholds_.erase(std::unique(thresholds_.begin(), thresholds_.end()), thresholds_.end());
}

void TypeInfo::findBounds() {
  // The tests that may bound numbers on a branch (which rangeOf checks as the types grow): a FOR test's true branch
  // (counter <= limit, or >= with a negative step) and either branch of a condition that is a comparison < <= > >=
  // (the false branch its negation), when the branch's edge is the only way into its block from outside it (its other
  // predecessors are blocks it dominates: a loop's back edges), so that every block that block dominates is reached
  // through that edge.
  auto onlyEntry = [&](const BasicBlock *b, const BasicBlock *body) {
    for (const BasicBlock *p : cfg_->preds(body))
      if (p != b && !cfg_->dominates(body, p)) return false;
    return true;
  };
  for (BasicBlock *b : cfg_->order()) {
    if (b->insts.empty()) continue;
    const Instruction &br = *b->insts.back();
    if (br.op != Op::CondBr || br.targets.size() != 2 || br.targets[0].block == br.targets[1].block) continue;
    const auto *t = dynamic_cast<const Instruction *>(br.operand(0));
    if (t && t->op == Op::Truth) t = dynamic_cast<const Instruction *>(t->operand(0)); // the test of a logical
    if (!t || t->parent != b) continue;
    const BasicBlock *yes = br.targets[0].block, *no = br.targets[1].block;
    auto add = [&](const Bound &bound) {
      edgeBounds_[b].push_back(bound);
      if (!onlyEntry(b, bound.body)) return;
      for (const Value *v : {bound.x, bound.y}) {
        std::vector<Bound> &mine = bounds_[v];
        if (mine.size() < kMaxBoundsPerValue) mine.push_back(bound);
      }
    };
    if (t->op == Op::ForTest) {
      if (t->operand(1) != t->operand(0) && t->operand(2) != t->operand(0))
        add({t->operand(0), t->operand(1), t->operand(2), BinOp::Le, b, yes, 0});
      continue;
    }
    if (t->op != Op::Binary || t->operand(0) == t->operand(1)) continue;
    BinOp negated;
    switch (t->binop) {
    case BinOp::Lt: negated = BinOp::Ge; break;
    case BinOp::Le: negated = BinOp::Gt; break;
    case BinOp::Gt: negated = BinOp::Le; break;
    case BinOp::Ge: negated = BinOp::Lt; break;
    default: continue;
    }
    add({t->operand(0), t->operand(1), nullptr, t->binop, b, yes, 0});
    add({t->operand(0), t->operand(1), nullptr, negated, b, no, 1});
  }
}

} // namespace xb::xir
