// value-captures: the variables of a function that its codeblocks may capture by value (FuncDecl::capturedByValue).
// A codeblock shares a variable it captures with the function: the variable moves into a cell, which both reach, so
// that each sees the other's assignments. A variable that nothing assigns after its declaration (its initializer runs
// before any block can capture it: a block captures only variables declared before it), nor passes by reference, nor
// loops with (nor writes through PValue()), holds one value as long as a block can see it: a block may receive a cell of its own holding that value,
// and the function keeps the variable in its frame, read directly. Only a function's own parameters and LOCALs (not a
// codeblock's, which Eval() may pass references; not the hidden ones of WITH OBJECT, USING and AWAIT FOR EACH, which
// their statements assign).
#include "opt/AstPasses.h"

#include "parser/Walk.h"

namespace xb {

namespace {

struct Facts {
  std::vector<bool> captured, written;
};

// The function slot a variable written in the codeblocks `blocks` (outermost first) designates, or -1 for another
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

class Scan {
public:
  explicit Scan(Facts &facts) : facts_(facts) {}

  void block(Block &b) {
    for (auto &s : b) {
      stmtWrites(*s);
      forEachPart(*s, [&](ExprPtr &e) { expr(*e); }, [&](Block &x) { block(x); });
    }
  }

private:
  void written(const Expr &target) {
    if (target.kind == ExprKind::Var) mark(facts_.written, functionSlot(static_cast<const VarExpr &>(target), blocks_));
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
    default: break;
    }
    bool isBlock = e.kind == ExprKind::Block;
    if (isBlock) blocks_.push_back(static_cast<const BlockExpr *>(&e));
    forEachChildExpr(e, [&](ExprPtr &c) { expr(*c); });
    forEachChildBlock(e, [&](Block &x) { block(x); });
    if (isBlock) blocks_.pop_back();
  }

  Facts &facts_;
  std::vector<const BlockExpr *> blocks_;
};

// The slots of the function's parameters and of its LOCAL statements' variables (which come first, at the function's
// level), not the hidden ones; not the parameters of a function that may write them through PValue() (readsParams: a
// call of a PARAMS function, a macro).
std::vector<bool> declared(const FuncDecl &f) {
  std::vector<bool> out(f.numLocals);
  for (size_t k = 0; k < f.params.size() && k < out.size(); ++k) out[k] = !f.readsParams;
  for (auto &s : f.body)
    if (s->kind == StmtKind::Local)
      for (const LocalDecl &d : static_cast<const LocalStmt &>(*s).decls)
        if (d.slot < out.size()) out[d.slot] = true;
  return out;
}

} // namespace

bool markValueCaptures(Module &m) {
  bool changed = false;
  for (auto &f : m.funcs) {
    Facts facts;
    Scan(facts).block(f->body);
    std::vector<bool> own = declared(*f), byValue(f->numLocals);
    bool any = false;
    for (uint32_t s = 0; s < f->numLocals; ++s) {
      bool captured = s < facts.captured.size() && facts.captured[s];
      bool written = s < facts.written.size() && facts.written[s];
      byValue[s] = captured && !written && own[s];
      any |= byValue[s];
    }
    if (!any) byValue.clear();
    if (byValue != f->capturedByValue) {
      f->capturedByValue = std::move(byValue);
      changed = true;
    }
  }
  return changed;
}

} // namespace xb
