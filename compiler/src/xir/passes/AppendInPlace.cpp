// `x := x + y` (and `x += y`) on a LOCAL only the function sees becomes one
// `append` (xb_op_append), whose string grows in place while nothing else
// sees it. The load may be in a block before the store's (y computed by an
// inlined body, or a branch), when it dominates the store and nothing on the
// way writes the variable. Only for a variable that holds strings: a string constant is
// stored into it or appended to it, and no other constant (a number, a
// logical, a date) is. Such a variable stays in the frame (promote-variables
// takes only loads and stores), which a numeric accumulator must not.
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"

#include <unordered_map>
#include <unordered_set>

namespace xb::xir {

namespace {

bool isConstant(const Value *v, Constant::Lit lit) {
  const Constant *c = asConstant(v);
  return c && c->lit == lit;
}

// A constant that is neither a string nor NIL: a variable given one holds numbers, logicals or dates too.
bool isOtherConstant(const Value *v) {
  const Constant *c = asConstant(v);
  return c && c->type() == Type::Item && c->lit != Constant::Lit::String && c->lit != Constant::Lit::Nil;
}

// Whether variable `var` may be written between `load` and `store` (the load's block dominates the store's): after
// the load in its block, before the store in its, or in a block on a path between them.
bool writtenBetween(const CFG &cfg, const Var *var, const Instruction *load, const Instruction *store) {
  auto writes = [&](const Instruction &i) { return i.var == var && (i.effects() & WritesVar); };
  const BasicBlock *from = load->parent, *to = store->parent;
  bool after = false;
  for (auto &i : from->insts) {
    if (i.get() == store) return false; // one block: the store ends the way
    if (after && writes(*i)) return true;
    after |= i.get() == load;
  }
  for (auto &i : to->insts) {
    if (i.get() == store) break;
    if (writes(*i)) return true;
  }
  // The blocks the load's block reaches without passing the store's, that reach the store's without passing the
  // load's.
  std::unordered_set<const BasicBlock *> fwd, bwd;
  std::vector<const BasicBlock *> work{from};
  while (!work.empty()) {
    const BasicBlock *b = work.back();
    work.pop_back();
    for (size_t k = 0; k < b->numSuccessors(); ++k) {
      const BasicBlock *n = b->successor(k);
      if (n != to && n != from && fwd.insert(n).second) work.push_back(n);
    }
  }
  work = {to};
  while (!work.empty()) {
    const BasicBlock *b = work.back();
    work.pop_back();
    for (const BasicBlock *p : cfg.preds(b))
      if (p != from && p != to && bwd.insert(p).second) work.push_back(p);
  }
  for (const BasicBlock *b : fwd)
    if (bwd.count(b))
      for (auto &i : b->insts)
        if (writes(*i)) return true;
  // The store's block again without the load's (a loop around the store alone): the store itself writes between.
  work = {to};
  std::unordered_set<const BasicBlock *> again;
  while (!work.empty()) {
    const BasicBlock *b = work.back();
    work.pop_back();
    for (size_t k = 0; k < b->numSuccessors(); ++k) {
      const BasicBlock *n = b->successor(k);
      if (n == to) return true;
      if (n != from && again.insert(n).second) work.push_back(n);
    }
  }
  return false;
}

} // namespace

bool appendInPlace(Function &f) {
  CFG cfg(f);
  std::vector<Var *> direct = directLocals(f);
  std::unordered_set<const Var *> locals(direct.begin(), direct.end());
  std::unordered_set<const Var *> holdsStrings, holdsOthers;
  std::unordered_map<const Var *, std::vector<Instruction *>> stores; // the `x := x + y` stores of each variable
  for (auto &b : f.blocks) {
    for (size_t k = 0; k < b->insts.size(); ++k) {
      Instruction &store = *b->insts[k];
      if (store.op != Op::Store || !locals.count(store.var)) continue;
      if (isConstant(store.operand(0), Constant::Lit::String)) holdsStrings.insert(store.var);
      if (isOtherConstant(store.operand(0))) holdsOthers.insert(store.var);
      const Instruction *add = asInstruction(store.operand(0), Op::Binary);
      if (!add || add->binop != BinOp::Add || !add->hasOneUse() || add->parent != b.get()) continue;
      const Instruction *load = asInstruction(add->operand(0), Op::Load);
      if (!load || load->var != store.var || !load->hasOneUse() || !cfg.reachable(load->parent) ||
          !cfg.reachable(b.get()) || !cfg.dominates(load->parent, b.get()))
        continue;
      // The append reads the variable at the store: nothing may write it from the load on.
      if (writtenBetween(cfg, store.var, load, &store)) continue;
      stores[store.var].push_back(&store);
      if (isConstant(add->operand(1), Constant::Lit::String)) holdsStrings.insert(store.var);
      if (isOtherConstant(add->operand(1))) holdsOthers.insert(store.var);
    }
  }
  std::unordered_map<const Instruction *, std::unique_ptr<Instruction>> appends; // replacing each store
  std::unordered_set<const Instruction *> gone;                                  // their adds and loads
  for (auto &[var, list] : stores) {
    if (!holdsStrings.count(var) || holdsOthers.count(var)) continue;
    for (Instruction *store : list) {
      auto *add = static_cast<Instruction *>(store->operand(0));
      gone.insert(add);
      gone.insert(asInstruction(add->operand(0), Op::Load));
      auto append = std::make_unique<Instruction>(Op::Append, Type::Void, store->parent);
      append->loc = store->loc;
      append->var = store->var;
      append->access = store->access;
      append->addOperand(add->operand(1));
      store->dropOperands();
      add->dropOperands();
      appends[store] = std::move(append);
    }
  }
  if (appends.empty()) return false;
  for (auto &b : f.blocks) {
    for (auto &i : b->insts) {
      auto it = appends.find(i.get());
      if (it != appends.end()) i = std::move(it->second);
    }
    std::erase_if(b->insts, [&](const std::unique_ptr<Instruction> &i) { return gone.count(i.get()) != 0; });
  }
  return true;
}

} // namespace xb::xir
