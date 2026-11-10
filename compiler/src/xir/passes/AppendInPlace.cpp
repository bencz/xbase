// `x := x + y` (and `x += y`) on a LOCAL only the function sees becomes one
// `append` (xb_op_append), whose string grows in place while nothing else
// sees it. Only for a variable that holds strings: a string constant is
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

} // namespace

bool appendInPlace(Function &f) {
  std::vector<Var *> direct = directLocals(f);
  std::unordered_set<const Var *> locals(direct.begin(), direct.end());
  std::unordered_set<const Var *> holdsStrings, holdsOthers;
  std::unordered_map<const Var *, std::vector<Instruction *>> stores; // the `x := x + y` stores of each variable
  for (auto &b : f.blocks) {
    std::unordered_map<const Instruction *, size_t> at;
    for (size_t k = 0; k < b->insts.size(); ++k) at[b->insts[k].get()] = k;
    for (size_t k = 0; k < b->insts.size(); ++k) {
      Instruction &store = *b->insts[k];
      if (store.op != Op::Store || !locals.count(store.var)) continue;
      if (isConstant(store.operand(0), Constant::Lit::String)) holdsStrings.insert(store.var);
      if (isOtherConstant(store.operand(0))) holdsOthers.insert(store.var);
      const Instruction *add = asInstruction(store.operand(0), Op::Binary);
      if (!add || add->binop != BinOp::Add || !add->hasOneUse() || add->parent != b.get()) continue;
      const Instruction *load = asInstruction(add->operand(0), Op::Load);
      if (!load || load->var != store.var || !load->hasOneUse() || load->parent != b.get()) continue;
      bool written = false; // between the load and the store, as the append reads the variable at the store
      for (size_t j = at.at(load) + 1; j < k && !written; ++j)
        written = b->insts[j]->var == store.var && (b->insts[j]->effects() & WritesVar);
      if (written) continue;
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
