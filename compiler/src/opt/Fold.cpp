#include "opt/Fold.h"

#include "common/BitFunctions.h"
#include "xbrt.h" // shared numeric rules


#include <unordered_map>

namespace xb {

std::optional<bool> stringTest(BinOp op, const std::string &x, const std::string &y) {
  xb_fold_str f;
  switch (op) {
  case BinOp::ExactEq: f = XB_FOLD_STR_EXACT; break;
  case BinOp::Eq: f = XB_FOLD_STR_EQ; break;
  case BinOp::NotEq: f = XB_FOLD_STR_NE; break;
  case BinOp::In: f = XB_FOLD_STR_IN; break;
  default: return std::nullopt;
  }
  int r;
  if (!xb_fold_strings(f, x.data(), x.size(), y.data(), y.size(), &r)) return std::nullopt;
  return r != 0;
}

std::optional<std::string> stringJoin(BinOp op, const std::string &x, const std::string &y) {
  if (op == BinOp::Add) return x + y;
  if (op != BinOp::Sub) return std::nullopt;
  std::string r(x.size() + y.size(), ' ');
  xb_str_minus(r.data(), x.data(), x.size(), y.data(), y.size());
  return r;
}

namespace {

bool isConstant(const Expr &e);

// A number as the folding rules (xbrt.h) hold it, and back.
xb_folded folded(const NumberExpr &n) {
  xb_folded f = {n.isInt ? 1 : 0, n.ival, n.dval, n.width, n.dec};
  return f;
}

ExprPtr numberOf(SourceLoc loc, const xb_folded &f) {
  return std::make_unique<NumberExpr>(loc, f.is_int != 0, f.i, f.d, f.dec, f.width);
}

// A folded integer: a constant of the program, or a computed one (a bit operation's).
ExprPtr makeInt(SourceLoc loc, int64_t v, bool computed = false) { return numberOf(loc, xb_folded_int(v, computed)); }

ExprPtr makeLogical(SourceLoc loc, bool v) { return std::make_unique<LogicalExpr>(loc, v); }

int64_t asInteger(const NumberExpr &n) { return n.isInt ? n.ival : xb_dbl_to_i64(n.dval); } // as the runtime takes it

const NumberExpr *num(const ExprPtr &e) {
  return e->kind == ExprKind::Number ? static_cast<const NumberExpr *>(e.get()) : nullptr;
}
const StringExpr *str(const ExprPtr &e) {
  return e->kind == ExprKind::String ? static_cast<const StringExpr *>(e.get()) : nullptr;
}
const LogicalExpr *logical(const ExprPtr &e) {
  return e->kind == ExprKind::Logical ? static_cast<const LogicalExpr *>(e.get()) : nullptr;
}

} // namespace

namespace {

std::optional<xb_fold_op> foldOp(BinOp op) {
  switch (op) {
  case BinOp::Add: return XB_FOLD_ADD;
  case BinOp::Sub: return XB_FOLD_SUB;
  case BinOp::Mul: return XB_FOLD_MUL;
  case BinOp::Div: return XB_FOLD_DIV;
  case BinOp::Mod: return XB_FOLD_MOD;
  case BinOp::Pow: return XB_FOLD_POW;
  default: return std::nullopt;
  }
}

} // namespace

ExprPtr foldNumbers(SourceLoc loc, BinOp op, const NumberExpr &a, const NumberExpr &b, bool runtime) {
  if (std::optional<xb_fold_op> f = foldOp(op)) {
    xb_folded x = folded(a), y = folded(b), r;
    return xb_fold_numbers(*f, &x, &y, runtime ? 1 : 0, &r) ? numberOf(loc, r) : nullptr;
  }
  if (const xb_bit_function *f = bitOperator(op)) // an integer without a display width, as the runtime makes it
    return makeInt(loc, xb_bit_apply(f->op, asInteger(a), asInteger(b)), true);
  return nullptr;
}

namespace {

// Result of a comparison operator given the operands' order c (xb_fold_compare).
bool comparisonResult(BinOp op, int c, bool &result) {
  xb_fold_cmp f;
  switch (op) {
  case BinOp::Eq: case BinOp::ExactEq: f = XB_FOLD_EQ; break;
  case BinOp::NotEq: f = XB_FOLD_NE; break;
  case BinOp::Lt: f = XB_FOLD_LT; break;
  case BinOp::Le: f = XB_FOLD_LE; break;
  case BinOp::Gt: f = XB_FOLD_GT; break;
  case BinOp::Ge: f = XB_FOLD_GE; break;
  default: return false;
  }
  result = xb_fold_compare(f, c) != 0;
  return true;
}

} // namespace

ExprPtr foldNumber(SourceLoc loc, UnOp op, const NumberExpr &n, bool runtime) {
  if (op == UnOp::BitNot) return makeInt(loc, xb_bit_apply(XB_BIT_NOT, asInteger(n), 0), true);
  if (op != UnOp::Neg) return nullptr;
  xb_folded x = folded(n);
  return numberOf(loc, xb_fold_negate(&x, runtime ? 1 : 0));
}

std::optional<bool> compareNumbers(BinOp op, const NumberExpr &a, const NumberExpr &b) {
  bool result;
  xb_folded x = folded(a), y = folded(b);
  if (comparisonResult(op, xb_folded_order(&x, &y), result)) return result;
  return std::nullopt;
}

void Folder::run(Module &m) {
  for (LocalDecl &d : m.fileStatics)
    if (d.init) fold(d.init);
  for (auto &f : m.funcs) block(f->body);
}

void Folder::block(Block &b) {
  for (auto &s : b) stmt(*s);
}

void Folder::stmt(Stmt &s) {
  switch (s.kind) {
  case StmtKind::Expr:
    fold(static_cast<ExprStmt &>(s).expr);
    break;
  case StmtKind::Local:
  case StmtKind::Static:
  case StmtKind::Private:
  case StmtKind::Public:
    for (LocalDecl &d : static_cast<LocalStmt &>(s).decls)
      if (d.init) fold(d.init);
    break;
  case StmtKind::ForEach: {
    auto &f = static_cast<ForEachStmt &>(s);
    for (auto &c : f.collections) fold(c);
    block(f.body);
    break;
  }
  case StmtKind::Switch: {
    auto &w = static_cast<SwitchStmt &>(s);
    fold(w.value);
    for (auto &c : w.cases) {
      if (c.value) fold(c.value);
      block(c.body);
    }
    break;
  }
  case StmtKind::Return: {
    auto &r = static_cast<ReturnStmt &>(s);
    if (r.value) fold(r.value);
    break;
  }
  case StmtKind::If: {
    auto &i = static_cast<IfStmt &>(s);
    for (auto &arm : i.arms) {
      fold(arm.cond);
      block(arm.body);
    }
    block(i.elseBody);
    break;
  }
  case StmtKind::While: {
    auto &w = static_cast<WhileStmt &>(s);
    fold(w.cond);
    block(w.body);
    break;
  }
  case StmtKind::For: {
    auto &f = static_cast<ForStmt &>(s);
    fold(f.start);
    fold(f.limit);
    if (f.step) fold(f.step);
    block(f.body);
    break;
  }
  case StmtKind::Yield:
    fold(static_cast<YieldStmt &>(s).value);
    break;
  case StmtKind::WithObject: {
    auto &w = static_cast<WithObjectStmt &>(s);
    fold(w.object);
    block(w.body);
    break;
  }
  case StmtKind::Sequence: {
    auto &q = static_cast<SequenceStmt &>(s);
    if (q.handler) fold(q.handler);
    block(q.body);
    block(q.recover);
    block(q.always);
    break;
  }
  case StmtKind::Exit:
  case StmtKind::Loop:
  case StmtKind::Parameters:
  case StmtKind::Memvar:
  case StmtKind::Field:
    break;
  }
}

void Folder::fold(ExprPtr &e) {
  ExprPtr r;
  switch (e->kind) {
  case ExprKind::Unary: {
    auto &u = static_cast<UnaryExpr &>(*e);
    fold(u.operand);
    r = foldUnary(u);
    break;
  }
  case ExprKind::Binary: {
    auto &b = static_cast<BinaryExpr &>(*e);
    fold(b.lhs);
    fold(b.rhs);
    r = foldBinary(b);
    break;
  }
  case ExprKind::And:
  case ExprKind::Or: {
    auto &l = static_cast<LogicExpr &>(*e);
    fold(l.lhs);
    fold(l.rhs);
    r = foldLogic(l, e->kind == ExprKind::And);
    break;
  }
  case ExprKind::Assign: {
    auto &a = static_cast<AssignExpr &>(*e);
    fold(a.target); // the indexes of an element target
    fold(a.value);
    break;
  }
  case ExprKind::Call: {
    auto &c = static_cast<CallExpr &>(*e);
    for (auto &arg : c.args) fold(arg);
    r = foldCall(c);
    break;
  }
  case ExprKind::ArrayLit:
    for (auto &el : static_cast<ArrayLitExpr &>(*e).elems) fold(el);
    break;
  case ExprKind::HashLit: {
    auto &h = static_cast<HashLitExpr &>(*e);
    for (size_t k = 0; k < h.keys.size(); ++k) {
      fold(h.keys[k]);
      fold(h.values[k]);
    }
    break;
  }
  case ExprKind::Index: {
    auto &ix = static_cast<IndexExpr &>(*e);
    fold(ix.base);
    fold(ix.index);
    break;
  }
  case ExprKind::Send: {
    auto &m = static_cast<SendExpr &>(*e);
    if (m.messageText) fold(m.messageText);
    fold(m.object);
    for (auto &a : m.args) fold(a);
    break;
  }
  case ExprKind::Block:
    for (auto &b : static_cast<BlockExpr &>(*e).body) fold(b);
    block(static_cast<BlockExpr &>(*e).statements);
    break;
  case ExprKind::IncDec:
    fold(static_cast<IncDecExpr &>(*e).target);
    break;
  case ExprKind::Ref:
    fold(static_cast<RefExpr &>(*e).target); // the indexes of an element
    break;
  case ExprKind::List: {
    // The elements before the last are evaluated for their effects only.
    auto &l = static_cast<ListExpr &>(*e);
    for (auto &x : l.exprs) fold(x);
    bool effects = false;
    for (size_t k = 0; k + 1 < l.exprs.size(); ++k) effects |= !isConstant(*l.exprs[k]);
    if (!effects) r = std::move(l.exprs.back());
    break;
  }
  case ExprKind::IIf: {
    auto &i = static_cast<IIfExpr &>(*e);
    fold(i.cond);
    fold(i.then);
    fold(i.otherwise);
    if (const LogicalExpr *c = logical(i.cond)) r = std::move(c->value ? i.then : i.otherwise);
    break;
  }
  default:
    break;
  }
  if (r) e = std::move(r);
}

ExprPtr Folder::foldUnary(UnaryExpr &u) {
  SourceLoc loc = u.loc;
  if (u.op == UnOp::Not) {
    if (const LogicalExpr *l = logical(u.operand)) return makeLogical(loc, !l->value);
    return nullptr;
  }
  const NumberExpr *n = num(u.operand);
  if (!n) return nullptr;
  if (u.op == UnOp::Plus) return std::move(u.operand);
  return foldNumber(loc, u.op, *n, false);
}

ExprPtr Folder::foldBinary(BinaryExpr &b) {
  SourceLoc loc = b.loc;
  const NumberExpr *na = num(b.lhs), *nb = num(b.rhs);
  if (na && nb) {
    if (ExprPtr r = foldNumbers(loc, b.op, *na, *nb, false)) return r;
    if (std::optional<bool> r = compareNumbers(b.op, *na, *nb)) return makeLogical(loc, *r);
    return nullptr;
  }
  const LogicalExpr *la = logical(b.lhs), *lb = logical(b.rhs);
  if (la && lb) {
    bool result;
    if (comparisonResult(b.op, (int)la->value - (int)lb->value, result)) return makeLogical(loc, result); // .F. < .T.
    return nullptr;
  }
  const StringExpr *sa = str(b.lhs), *sb = str(b.rhs);
  if (!sa || !sb) return nullptr;
  if (std::optional<std::string> r = stringJoin(b.op, sa->value, sb->value))
    return std::make_unique<StringExpr>(loc, std::move(*r));
  if (std::optional<bool> r = stringTest(b.op, sa->value, sb->value)) return makeLogical(loc, *r);
  return nullptr;
}

// A logical literal on the left decides the result or leaves the right
// operand alone. A literal on the right is not used to drop the left operand:
// that would discard its side effects.
ExprPtr Folder::foldLogic(LogicExpr &l, bool isAnd) {
  const LogicalExpr *a = logical(l.lhs);
  if (!a) return nullptr;
  if (const LogicalExpr *b = logical(l.rhs)) {
    return makeLogical(l.loc, isAnd ? a->value && b->value : a->value || b->value);
  }
  if (a->value == isAnd) return std::move(l.rhs); // .T. .AND. x, .F. .OR. x
  return makeLogical(l.loc, a->value);           // .F. .AND. x, .T. .OR. x
}

namespace {

// A value known at compile time whose evaluation does nothing else: a literal,
// or an array literal of such values.
bool isConstant(const Expr &e) {
  switch (e.kind) {
  case ExprKind::Nil: case ExprKind::Logical: case ExprKind::Number: case ExprKind::String:
    return true;
  case ExprKind::ArrayLit:
    for (auto &el : static_cast<const ArrayLitExpr &>(e).elems)
      if (!isConstant(*el)) return false;
    return true;
  default:
    return false;
  }
}

// Len( x ): the length of a string literal, or the element count of an array
// literal of constants (the array itself is not needed).
ExprPtr evalLen(const CallExpr &c) {
  if (c.args.size() != 1) return nullptr;
  if (const StringExpr *s = str(c.args[0])) return makeInt(c.loc, (int64_t)s->value.size());
  if (c.args[0]->kind == ExprKind::ArrayLit && isConstant(*c.args[0]))
    return makeInt(c.loc, (int64_t)static_cast<const ArrayLitExpr &>(*c.args[0]).elems.size());
  return nullptr;
}

// A bit function on number constants: an integer without a display width
// of its own, like the one the runtime computes.
ExprPtr evalBit(const CallExpr &c) {
  const xb_bit_function *f = findBitFunction(c.callee->name);
  if (!f || c.args.size() != f->arity) return nullptr;
  int64_t v[2] = {0, 0};
  for (size_t k = 0; k < f->arity; ++k) {
    const NumberExpr *n = num(c.args[k]);
    if (!n) return nullptr;
    v[k] = asInteger(*n);
  }
  return makeInt(c.loc, xb_bit_apply(f->op, v[0], v[1]), true);
}

// Compile-time evaluators of the functions declared FOLD; each returns null
// when its arguments are not constant enough.
using Evaluator = ExprPtr (*)(const CallExpr &);
const std::unordered_map<std::string, Evaluator> &evaluators() {
  static const std::unordered_map<std::string, Evaluator> table = [] {
    std::unordered_map<std::string, Evaluator> t = {{"LEN", evalLen}};
    for (uint32_t k = 0; k < XB_BIT_OP_COUNT; ++k)
      if (const char *name = xb_bit_function_of((xb_bit_op)k)->name) t[name] = evalBit;
    return t;
  }();
  return table;
}

} // namespace

bool Folder::canEvaluate(const std::string &name) { return evaluators().count(name) != 0; }

// A call to a FOLD function whose arguments are constant becomes its result.
ExprPtr Folder::foldCall(CallExpr &c) {
  if (!c.callee || !c.callee->fold) return nullptr;
  auto it = evaluators().find(c.callee->name);
  return it == evaluators().end() ? nullptr : it->second(c);
}

} // namespace xb
