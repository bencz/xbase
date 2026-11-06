// mem2reg: frame variables reached only by direct loads and stores become SSA
// values, with block parameters where definitions meet (Cytron et al.: the
// iterated dominance frontiers of the blocks that store, then renaming along
// the dominator tree).
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"
#include "xir/Builder.h"

#include <unordered_map>
#include <unordered_set>

namespace xb::xir {

namespace {

class Promoter {
public:
  Promoter(Function &f, const std::vector<Var *> &vars) : f_(f), cfg_(f), vars_(vars) {
    for (size_t k = 0; k < vars.size(); ++k) slot_[vars[k]] = k;
  }

  void run() {
    placeParams();
    rename();
    removeDeadParams();
    for (auto &b : f_.blocks)
      std::erase_if(b->insts, [&](const std::unique_ptr<Instruction> &i) { return gone_.count(i.get()) != 0; });
  }

private:
  // A parameter where definitions meet, for every block in the iterated
  // dominance frontier of the blocks that store the variable (the entry
  // defines it too: NIL).
  void placeParams() {
    Frontiers df = dominanceFrontiers(cfg_);
    std::vector<std::vector<BasicBlock *>> defs(vars_.size(), std::vector<BasicBlock *>{f_.entry()});
    for (BasicBlock *b : cfg_.order())
      for (auto &i : b->insts)
        if (i->op == Op::Store && slot_.count(i->var)) defs[slot_[i->var]].push_back(b);
    for (size_t k = 0; k < vars_.size(); ++k) {
      std::unordered_set<const BasicBlock *> placed, queued(defs[k].begin(), defs[k].end());
      std::vector<BasicBlock *> work = defs[k];
      while (!work.empty()) {
        BasicBlock *b = work.back();
        work.pop_back();
        for (BasicBlock *join : df[b]) {
          if (!placed.insert(join).second) continue;
          BlockParam *p = join->addParam(Type::Item);
          paramVar_[p] = k;
          if (queued.insert(join).second) work.push_back(join);
        }
      }
    }
  }

  // Along the dominator tree, each variable's current value on a stack: loads
  // take it, stores push theirs, branches pass it to the parameters placed in
  // their targets.
  void rename() {
    std::vector<std::vector<Value *>> current(vars_.size(), std::vector<Value *>{Builder(&f_).nil()});
    struct Step {
      BasicBlock *block;
      bool leaving;
      std::vector<size_t> pushed; // variables whose stacks this block pushed, to pop when leaving
    };
    std::vector<Step> walk{{f_.entry(), false, {}}};
    while (!walk.empty()) {
      Step step = std::move(walk.back());
      walk.pop_back();
      if (step.leaving) {
        for (size_t k : step.pushed) current[k].pop_back();
        continue;
      }
      BasicBlock *b = step.block;
      std::vector<size_t> pushed;
      for (auto &p : b->params) {
        auto it = paramVar_.find(p.get());
        if (it == paramVar_.end()) continue;
        current[it->second].push_back(p.get());
        pushed.push_back(it->second);
      }
      for (auto &ip : b->insts) {
        Instruction &i = *ip;
        if ((i.op != Op::Load && i.op != Op::Store) || !slot_.count(i.var)) continue;
        size_t k = slot_[i.var];
        if (i.op == Op::Load) {
          i.replaceAllUsesWith(current[k].back());
        } else {
          current[k].push_back(i.operand(0));
          pushed.push_back(k);
          i.dropOperands();
        }
        gone_.insert(&i);
      }
      if (Instruction *t = b->terminator())
        for (size_t e = 0; e < t->targets.size(); ++e)
          for (auto &p : t->targets[e].block->params) {
            auto it = paramVar_.find(p.get());
            if (it != paramVar_.end()) t->addTargetArg(e, current[it->second].back());
          }
      walk.push_back({b, true, std::move(pushed)});
      for (BasicBlock *c : cfg_.children(b)) walk.push_back({c, false, {}});
    }
  }

  // A placed parameter is needed when something other than a branch to a
  // placed parameter uses it (an operand, or the argument of a parameter the
  // function had: the result of an IIF), or a needed parameter receives it.
  void removeDeadParams() {
    Predecessors preds = predecessorTerminators(f_);
    std::unordered_set<const BlockParam *> live;
    std::vector<const BlockParam *> work;
    auto mark = [&](const Value *v) {
      const BlockParam *p = asParam(v);
      if (p && paramVar_.count(p) && live.insert(p).second) work.push_back(p);
    };
    for (auto &[p, k] : paramVar_)
      for (const Instruction *u : p->users()) {
        for (const Value *v : u->operands())
          if (v == p) mark(p);
        for (const Target &tg : u->targets)
          for (size_t a = 0; a < tg.args.size(); ++a)
            if (tg.args[a] == p && !paramVar_.count(tg.block->params[a].get())) mark(p);
      }
    while (!work.empty()) {
      const BlockParam *p = work.back();
      work.pop_back();
      for (Instruction *t : preds[p->block])
        for (const Target &tg : t->targets)
          if (tg.block == p->block) mark(tg.args[p->index]);
    }
    auto dead = [&](const BlockParam *p) { return paramVar_.count(p) && !live.count(p); };
    for (auto &b : f_.blocks) // the dead ones only feed each other: all their arguments go first
      for (size_t k = b->params.size(); k-- > 0;)
        if (dead(b->params[k].get())) b->removeArgs(k, preds[b.get()]);
    for (auto &b : f_.blocks)
      for (size_t k = b->params.size(); k-- > 0;)
        if (dead(b->params[k].get())) {
          paramVar_.erase(b->params[k].get());
          b->eraseParam(k);
        }
  }

  Function &f_;
  CFG cfg_;
  const std::vector<Var *> &vars_;
  std::unordered_map<const Var *, size_t> slot_;            // variable -> its index in vars_
  std::unordered_map<const BlockParam *, size_t> paramVar_; // placed parameter -> its variable
  std::unordered_set<const Instruction *> gone_;            // loads and stores replaced
};

} // namespace

// A function with BEGIN SEQUENCE keeps its variables in memory: its handlers
// are reached by unwinding, from inside blocks, where no branch can pass
// values (FOR EACH cleanups read no promotable variable).
bool promoteVariables(Function &f) {
  for (auto &b : f.blocks)
    for (auto &i : b->insts)
      if (i->op == Op::SeqCatch) return false;
  std::vector<Var *> vars = directLocals(f);
  if (vars.empty()) return false;
  Promoter(f, vars).run();
  return true;
}

} // namespace xb::xir
