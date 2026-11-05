// Control flow simplification: unreachable blocks, blocks that only jump on,
// blocks reached only by a jump from their single predecessor, and two
// branches to one block that differ only in a true/false argument.
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"
#include "xir/Builder.h"

#include <algorithm>
#include <iterator>
#include <unordered_map>

namespace xb::xir {

namespace {

// `cond_br c, T(..., true, ...), T(..., false, ...)`, with the arguments
// otherwise the same, is `br T(..., c, ...)`.
bool branchesToOneBlock(Function &f) {
  Builder builder(&f);
  bool changed = false;
  for (auto &bp : f.blocks) {
    Instruction *t = bp->terminator();
    if (!t || t->op != Op::CondBr || t->targets[0].block != t->targets[1].block) continue;
    const auto &a = t->targets[0].args, &b = t->targets[1].args;
    std::vector<Value *> args;
    for (size_t k = 0; k < a.size(); ++k) {
      const Constant *x = asBool(a[k]), *y = asBool(b[k]);
      if (sameValue(a[k], b[k])) args.push_back(a[k]);
      else if (x && y && x->logical && !y->logical) args.push_back(t->operand(0));
      else break;
    }
    if (args.size() != a.size()) continue;
    builder.jump(bp.get(), t->targets[0].block, args);
    changed = true;
  }
  return changed;
}

// A block that holds nothing but `br T(args)`, whose parameters are used only
// there (so no other block needs them), is skipped: its predecessors jump to T
// directly, with the arguments it would have passed.
bool threadJumps(Function &f, Predecessors &preds) {
  bool changed = false;
  for (auto &bp : f.blocks) {
    BasicBlock *e = bp.get();
    if (e == f.entry() || e->insts.size() != 1 || e->insts[0]->op != Op::Br) continue;
    Instruction &jump = *e->insts[0];
    BasicBlock *to = jump.targets[0].block;
    if (to == e) continue;
    bool localParams = true;
    for (auto &p : e->params)
      for (const Instruction *u : p->users()) localParams &= u == &jump;
    if (!localParams) continue;
    std::vector<Instruction *> &toPreds = preds[to];
    for (Instruction *pred : preds[e]) {
      for (size_t k = 0; k < pred->targets.size(); ++k) {
        if (pred->targets[k].block != e) continue;
        std::vector<Value *> args;
        for (Value *a : jump.targets[0].args) {
          const BlockParam *p = asParam(a);
          args.push_back(p && p->block == e ? pred->targets[k].args[p->index] : a);
        }
        pred->setTarget(k, to, std::move(args));
      }
      if (std::find(toPreds.begin(), toPreds.end(), pred) == toPreds.end()) toPreds.push_back(pred);
      changed = true;
    }
    preds[e].clear();
  }
  return changed;
}

// A block whose only predecessor jumps to it unconditionally joins that
// predecessor: its parameters become the jump's arguments.
bool mergeBlocks(Function &f) {
  std::unordered_map<const BasicBlock *, unsigned> edges;
  for (auto &bp : f.blocks)
    if (const Instruction *t = bp->terminator())
      for (const Target &tg : t->targets) ++edges[tg.block];
  bool changed = false;
  for (auto &bp : f.blocks) {
    BasicBlock *a = bp.get();
    while (true) {
      Instruction *t = a->terminator();
      if (!t || t->op != Op::Br) break;
      BasicBlock *next = t->targets[0].block;
      if (next == a || next == f.entry() || edges[next] != 1) break;
      std::vector<Value *> args = t->targets[0].args;
      a->takeTerminator();
      for (size_t k = 0; k < args.size(); ++k) next->params[k]->replaceAllUsesWith(args[k]);
      for (auto &i : next->insts) i->parent = a;
      std::move(next->insts.begin(), next->insts.end(), std::back_inserter(a->insts));
      next->insts.clear();
      edges[next] = 0;
      changed = true;
    }
  }
  return changed;
}

} // namespace

// The steps feed each other (threading makes two branches to one block,
// merging exposes more threading), so they repeat until nothing changes; each
// change removes an edge or a block, which bounds the repetitions.
bool simplifyCFG(Function &f) {
  bool changed = false;
  for (bool again = true; again;) {
    Predecessors preds = predecessorTerminators(f);
    again = threadJumps(f, preds);
    again |= branchesToOneBlock(f);
    again |= mergeBlocks(f);
    again |= removeUnreachableBlocks(f);
    changed |= again;
  }
  return changed;
}

} // namespace xb::xir
