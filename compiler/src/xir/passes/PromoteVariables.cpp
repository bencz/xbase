// mem2reg: frame variables reached only by direct loads and stores become SSA
// values, with block parameters where definitions meet (Cytron et al.: the
// iterated dominance frontiers of the blocks that store, then renaming along
// the dominator tree). A handler (an unwind block: a sequence's seq.catch,
// ALWAYS's, a FOR EACH cleanup, an inlined body's way out) is reached by
// unwinding from inside a block, where no branch passes values: a variable live
// where one starts is loaded there, a definition, and keeps the stores that
// memory needs to hold its value when an operation unwinds there.
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"
#include "xir/Builder.h"

#include <unordered_map>
#include <unordered_set>

namespace xb::xir {

namespace {

using VarSet = std::unordered_set<const Var *>;
using LiveIn = std::unordered_map<const BasicBlock *, VarSet>;

// The variables of `vars` live where each block starts: loaded on some path from there before being stored (over the
// control flow graph, unwind edges included).
LiveIn liveness(const Function &f, const VarSet &vars) {
  std::unordered_map<const BasicBlock *, VarSet> gen, kill;
  LiveIn in;
  for (auto &b : f.blocks)
    for (auto &i : b->insts) {
      if (!i->var || !vars.count(i->var)) continue;
      if (i->op == Op::Load && !kill[b.get()].count(i->var)) gen[b.get()].insert(i->var);
      if (i->op == Op::Store) kill[b.get()].insert(i->var);
    }
  for (bool changed = true; changed;) { // the sets only grow
    changed = false;
    for (auto b = f.blocks.rbegin(); b != f.blocks.rend(); ++b) {
      const BasicBlock *bb = b->get();
      VarSet live = gen[bb];
      for (size_t k = 0; k < bb->numSuccessors(); ++k)
        for (const Var *v : in[bb->successor(k)])
          if (!kill[bb].count(v)) live.insert(v);
      if (live.size() != in[bb].size()) {
        in[bb] = std::move(live);
        changed = true;
      }
    }
  }
  return in;
}

// Whether `i` is a load of its variable at the start of handler `h` (among the loads it starts with): what this pass
// puts there.
bool loadAtStart(const BasicBlock *h, const Instruction *i) {
  for (auto &j : h->insts) {
    if (j.get() == i) return true;
    if (j->op != Op::Load) return false;
  }
  return false;
}

// The stores of the kept variables memory needs: one after which an operation may unwind to a handler where its
// variable is live (one in a block that unwinds there), or from which a block whose value memory must hold is reached
// before another store: a handler that loads it (entered by a branch too: ALWAYS's resume, an onward way), or one of
// those blocks again.
std::unordered_set<const Instruction *> neededStores(const Function &f, const VarSet &kept, const LiveIn &live,
                                                     const std::unordered_set<const BasicBlock *> &handlers) {
  std::unordered_set<const Instruction *> out;
  auto liveAt = [&](const BasicBlock *b, const Var *v) {
    auto it = live.find(b);
    return it != live.end() && it->second.count(v) != 0;
  };
  for (const Var *v : kept) {
    auto unwinds = [&](const BasicBlock *b) { return b->unwind && liveAt(b->unwind, v); };
    auto stores = [&](const BasicBlock *b) {
      for (auto &i : b->insts)
        if (i->op == Op::Store && i->var == v) return true;
      return false;
    };
    std::unordered_map<const BasicBlock *, bool> needed; // memory must hold the value when the block starts
    for (auto &b : f.blocks) needed[b.get()] = (handlers.count(b.get()) && liveAt(b.get(), v)) || unwinds(b.get());
    auto onward = [&](const BasicBlock *b) { // a branch's successor that needs it
      const Instruction *t = b->terminator();
      if (!t) return false;
      for (const Target &tg : t->targets)
        if (needed[tg.block]) return true;
      return false;
    };
    for (bool changed = true; changed;) { // the set only grows
      changed = false;
      for (auto &b : f.blocks)
        if (!needed[b.get()] && !stores(b.get()) && onward(b.get())) changed = needed[b.get()] = true;
    }
    for (auto &b : f.blocks) {
      const Instruction *last = nullptr;
      for (auto &i : b->insts)
        if (i->op == Op::Store && i->var == v) {
          if (unwinds(b.get())) out.insert(i.get());
          last = i.get();
        }
      if (last && onward(b.get())) out.insert(last);
    }
  }
  return out;
}

class Promoter {
public:
  Promoter(Function &f, const std::vector<Var *> &vars, const VarSet &kept, const LiveIn &live,
           const std::unordered_set<const BasicBlock *> &handlers)
      : f_(f), cfg_(f), vars_(vars), kept_(kept), live_(live), handlers_(handlers) {
    for (size_t k = 0; k < vars.size(); ++k) slot_[vars[k]] = k;
  }

  void run() {
    loadAtHandlers();
    placeParams();
    stores_ = neededStores(f_, kept_, live_, handlers_);
    rename();
    removeDeadParams();
    for (auto &b : f_.blocks)
      std::erase_if(b->insts, [&](const std::unique_ptr<Instruction> &i) { return gone_.count(i.get()) != 0; });
  }

private:
  // Each kept variable loaded first thing in every handler where it is live: the value the operation that unwound left
  // in memory.
  void loadAtHandlers() {
    for (auto &hp : f_.blocks) {
      BasicBlock *h = hp.get();
      if (!handlers_.count(h)) continue;
      for (Var *v : vars_) {
        auto it = live_.find(h);
        if (!kept_.count(v) || it == live_.end() || !it->second.count(v)) continue;
        auto load = std::make_unique<Instruction>(Op::Load, Type::Item, h);
        load->var = v;
        load->access = Access::Direct;
        if (!h->insts.empty()) load->loc = h->insts.front()->loc;
        handlerLoads_.insert(load.get());
        h->insts.insert(h->insts.begin(), std::move(load));
      }
    }
  }

  // A parameter where definitions meet, for every block in the iterated
  // dominance frontier of the blocks that store the variable or load it at a
  // handler's start (the entry defines it too: NIL).
  void placeParams() {
    Frontiers df = dominanceFrontiers(cfg_);
    std::vector<std::vector<BasicBlock *>> defs(vars_.size(), std::vector<BasicBlock *>{f_.entry()});
    for (BasicBlock *b : cfg_.order())
      for (auto &i : b->insts)
        if ((i->op == Op::Store || handlerLoads_.count(i.get())) && slot_.count(i->var))
          defs[slot_[i->var]].push_back(b);
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
        if (handlerLoads_.count(&i)) { // the value from memory: the variable's from here
          current[k].push_back(&i);
          pushed.push_back(k);
          continue;
        }
        if (i.op == Op::Load) {
          i.replaceAllUsesWith(current[k].back());
        } else {
          current[k].push_back(i.operand(0));
          pushed.push_back(k);
          if (stores_.count(&i)) continue; // memory holds the value for a handler
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
  const VarSet &kept_;                                      // live where a handler starts
  const LiveIn &live_;                                      // the variables live where each block starts
  const std::unordered_set<const BasicBlock *> &handlers_;  // the unwind blocks
  std::unordered_set<const Instruction *> handlerLoads_;    // kept variables loaded where handlers start: definitions
  std::unordered_set<const Instruction *> stores_;          // kept variables' stores memory needs (neededStores)
  std::unordered_map<const Var *, size_t> slot_;            // variable -> its index in vars_
  std::unordered_map<const BlockParam *, size_t> paramVar_; // placed parameter -> its variable
  std::unordered_set<const Instruction *> gone_;            // loads and stores replaced
};

} // namespace

// Resumable functions keep their variables in memory: their frames live across suspensions. A kept variable whose
// only loads are those at handlers' starts was promoted already: nothing to do for it.
bool promoteVariables(Function &f) {
  if (isResumable(f)) return false;
  std::vector<Var *> all = directLocals(f);
  if (all.empty()) return false;
  std::unordered_set<const BasicBlock *> handlers = unwindTargets(f);
  LiveIn live = liveness(f, VarSet(all.begin(), all.end()));
  VarSet kept;
  for (const BasicBlock *h : handlers)
    if (auto it = live.find(h); it != live.end()) kept.insert(it->second.begin(), it->second.end());
  std::unordered_map<const Var *, bool> done;
  for (const Var *v : kept) done[v] = true;
  for (auto &b : f.blocks)
    for (auto &i : b->insts)
      if (i->op == Op::Load && kept.count(i->var) && !(handlers.count(b.get()) && loadAtStart(b.get(), i.get())))
        done[i->var] = false;
  std::vector<Var *> vars;
  for (Var *v : all)
    if (!done[v]) vars.push_back(v);
  if (vars.empty()) return false;
  Promoter(f, vars, kept, live, handlers).run();
  return true;
}

} // namespace xb::xir
