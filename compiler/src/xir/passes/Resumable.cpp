// Resumable functions: a generator's body runs a step at each MoveNext, and
// returns between steps (gen.yield), so nothing but its frame survives from one
// step to the next: the frame goes into the enumerator and comes back at the
// next step (gen.restore), which enters the block after that YIELD
// (Generator::genGenerator builds that entry). A value computed before a YIELD
// and used after it (a FOR's limit, a FOR EACH's position, any value promoted
// from a variable) would be lost with the registers, so it moves into the
// frame: into a variable of its own, stored where it is computed and loaded
// right before each use. Such a value is exactly one with a use its definition
// does not dominate once the entries after the YIELDs exist.
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"

#include <algorithm>
#include <memory>

namespace xb::xir {

namespace {

std::unique_ptr<Instruction> make(Op op, Type t, BasicBlock *b, SourceLoc loc) {
  auto i = std::make_unique<Instruction>(op, t, b);
  i->loc = loc;
  return i;
}

size_t positionOf(const BasicBlock *b, const Instruction *i) {
  for (size_t k = 0; k < b->insts.size(); ++k)
    if (b->insts[k].get() == i) return k;
  return b->insts.size();
}

// Whether every use of `v`, defined in `def`, is reached only through its definition.
bool dominatesUses(const CFG &cfg, const Value *v, const BasicBlock *def) {
  for (const Instruction *u : v->users())
    if (cfg.reachable(u->parent) && u->parent != def && !cfg.dominates(def, u->parent)) return false;
  return true;
}

// Moves `v` (defined in `def`) into a new frame variable: stored there at position `at` of `def` (Bool and I64
// values boxed as items), loaded right before each use.
void spill(Function &f, Value *v, BasicBlock *def, size_t at, uint32_t slot) {
  Var *home = f.addVar(Var::Kind::Frame, slot, "");
  Type t = v->type();
  std::vector<Instruction *> users = v->users();
  std::sort(users.begin(), users.end());
  users.erase(std::unique(users.begin(), users.end()), users.end());
  SourceLoc loc = at < def->insts.size() ? def->insts[at]->loc : SourceLoc{};
  std::vector<std::unique_ptr<Instruction>> store;
  Value *item = v;
  if (t != Type::Item) {
    store.push_back(make(t == Type::Bool ? Op::BoxBool : Op::BoxI64, Type::Item, def, loc));
    store.back()->addOperand(v);
    item = store.back().get();
  }
  store.push_back(make(Op::Store, Type::Void, def, loc));
  store.back()->var = home;
  store.back()->addOperand(item);
  def->insts.insert(def->insts.begin() + (std::ptrdiff_t)at, std::make_move_iterator(store.begin()),
                    std::make_move_iterator(store.end()));
  for (Instruction *u : users) {
    BasicBlock *b = u->parent;
    std::vector<std::unique_ptr<Instruction>> load;
    load.push_back(make(Op::Load, Type::Item, b, u->loc));
    load.back()->var = home;
    Value *now = load.back().get();
    if (t != Type::Item) {
      load.push_back(make(t == Type::Bool ? Op::UnboxBool : Op::UnboxI64, t, b, u->loc));
      load.back()->addOperand(now);
      now = load.back().get();
    }
    size_t pos = positionOf(b, u);
    b->insts.insert(b->insts.begin() + (std::ptrdiff_t)pos, std::make_move_iterator(load.begin()),
                    std::make_move_iterator(load.end()));
    u->replaceUses(v, now);
  }
}

} // namespace

bool makeResumable(Function &f) {
  bool yields = false;
  for (auto &b : f.blocks)
    if (Instruction *t = b->terminator(); t && t->op == Op::GenYield) yields = true;
  if (!yields) return false;
  uint32_t slot = 0;
  for (auto &v : f.vars)
    if (v->kind == Var::Kind::Frame) slot = std::max(slot, v->index + 1);
  CFG cfg(f);
  struct Crossing {
    Value *v;
    BasicBlock *def;
    const Instruction *after; // null: a parameter, stored first in its block
  };
  std::vector<Crossing> crossing;
  for (BasicBlock *b : cfg.order()) {
    for (auto &p : b->params)
      if (!dominatesUses(cfg, p.get(), b)) crossing.push_back({p.get(), b, nullptr});
    for (auto &i : b->insts)
      if (!i->users().empty() && !dominatesUses(cfg, i.get(), b)) crossing.push_back({i.get(), b, i.get()});
  }
  for (const Crossing &c : crossing) {
    size_t at = c.after ? positionOf(c.def, c.after) + 1 : 0;
    spill(f, c.v, c.def, at, slot++);
  }
  return !crossing.empty();
}

} // namespace xb::xir
