// The facts about a function's frame variables that the AST passes share (opt/VariableFacts.h).
#include "opt/VariableFacts.h"

#include "parser/Walk.h"

namespace xb {

namespace {

// The function slot a variable used in the codeblocks `blocks` (outermost first) designates, or -1 for another
// frame's variable (a codeblock's own) or none.
int64_t functionSlot(const VarExpr &v, const std::vector<const BlockExpr *> &blocks) {
  if (v.where == VarWhere::Frame) return blocks.empty() ? (int64_t)v.slot : -1;
  if (v.where != VarWhere::Capture || blocks.empty()) return -1;
  uint32_t index = v.slot;
  for (size_t k = blocks.size(); k-- > 0;) {
    const BlockExpr::Capture &cap = blocks[k]->captures.at(index);
    if (!cap.fromParentCapture) return k == 0 ? (int64_t)cap.index : -1;
    index = cap.index;
  }
  return -1;
}

void mark(std::vector<bool> &set, int64_t slot) {
  if (slot < 0) return;
  if (set.size() <= (size_t)slot) set.resize((size_t)slot + 1);
  set[(size_t)slot] = true;
}

// The function's own variable `e` is (a VarExpr of its frame, outside its codeblocks), or null.
const VarExpr *ownVar(const Expr &e, const std::vector<const BlockExpr *> &blocks) {
  if (e.kind != ExprKind::Var || !blocks.empty()) return nullptr;
  const auto &v = static_cast<const VarExpr &>(e);
  return v.where == VarWhere::Frame ? &v : nullptr;
}

class Scan {
public:
  explicit Scan(VariableFacts &facts) : facts_(facts) {}

  void block(Block &b) {
    for (auto &s : b) {
      stmtWrites(*s);
      forEachPart(*s, [&](ExprPtr &e) { expr(*e); }, [&](Block &x) { block(x); });
    }
  }

private:
  void written(const Expr &target) {
    if (target.kind == ExprKind::Var) mark(facts_.written, functionSlot(static_cast<const VarExpr &>(target), blocks_));
    else if (target.kind == ExprKind::Index) changed(*static_cast<const IndexExpr &>(target).base);
  }

  // An element of `base` assigned or passed by reference: base (a variable's array) is no table.
  void changed(const Expr &base) {
    if (const VarExpr *v = ownVar(base, blocks_)) mark(facts_.otherUse, v->slot);
  }

  void stmtWrites(Stmt &s) {
    switch (s.kind) {
    case StmtKind::For: written(*static_cast<ForStmt &>(s).var); break;
    case StmtKind::ForEach: {
      auto &f = static_cast<ForEachStmt &>(s);
      if (f.key) written(*f.key);
      for (auto &v : f.vars) written(*v);
      break;
    }
    case StmtKind::Sequence:
      if (auto &q = static_cast<SequenceStmt &>(s); q.usingVar) written(*q.usingVar);
      break;
    default: break;
    }
  }

  void expr(Expr &e) {
    switch (e.kind) {
    case ExprKind::Assign: written(*static_cast<AssignExpr &>(e).target); break;
    case ExprKind::IncDec: written(*static_cast<IncDecExpr &>(e).target); break;
    case ExprKind::Ref: written(*static_cast<RefExpr &>(e).target); break;
    case ExprKind::Block:
      if (blocks_.empty())
        for (const BlockExpr::Capture &cap : static_cast<BlockExpr &>(e).captures)
          if (!cap.fromParentCapture) mark(facts_.captured, cap.index);
      break;
    case ExprKind::Var:
      if (const VarExpr *v = ownVar(e, blocks_)) mark(facts_.otherUse, v->slot);
      return;
    case ExprKind::Index: { // t[i]: a table's use of t (an assigned element was marked by its assignment)
      auto &x = static_cast<IndexExpr &>(e);
      if (ownVar(*x.base, blocks_)) {
        expr(*x.index);
        return;
      }
      break;
    }
    case ExprKind::Call: // Len( t ): a table's use of t
      if (auto &c = static_cast<CallExpr &>(e); runtimeLen(c) && ownVar(*c.args[0], blocks_)) return;
      break;
    default: break;
    }
    bool isBlock = e.kind == ExprKind::Block;
    if (isBlock) blocks_.push_back(static_cast<const BlockExpr *>(&e));
    forEachChildExpr(e, [&](ExprPtr &c) { expr(*c); });
    forEachChildBlock(e, [&](Block &x) { block(x); });
    if (isBlock) blocks_.pop_back();
  }

  VariableFacts &facts_;
  std::vector<const BlockExpr *> blocks_;
};

} // namespace

VariableFacts variableFacts(FuncDecl &f) {
  VariableFacts facts;
  Scan(facts).block(f.body);
  return facts;
}

} // namespace xb
