// Control flow analyses of xIR functions.
#include "xir/Analysis.h"

#include <algorithm>
#include <unordered_set>

namespace xb::xir {

CFG::CFG(const Function &f) {
  // Depth-first postorder with an explicit stack (functions can be large).
  // Successors are visited last first, so that in reverse postorder a block's
  // first successor (the loop body, the THEN branch) comes right after it and
  // the exits come later: the order is also the layout of the code.
  std::vector<BasicBlock *> post;
  std::unordered_set<const BasicBlock *> seen;
  std::vector<std::pair<BasicBlock *, size_t>> stack;
  BasicBlock *entry = f.entry();
  stack.push_back({entry, 0});
  seen.insert(entry);
  while (!stack.empty()) {
    auto &[b, next] = stack.back();
    size_t count = b->numSuccessors();
    if (next < count) {
      BasicBlock *s = b->successor(count - 1 - next++);
      if (seen.insert(s).second) stack.push_back({s, 0});
      continue;
    }
    post.push_back(b);
    stack.pop_back();
  }
  rpo_.assign(post.rbegin(), post.rend());
  const uint32_t n = (uint32_t)rpo_.size();
  for (uint32_t i = 0; i < n; ++i) index_[rpo_[i]] = i;
  preds_.resize(n);
  for (BasicBlock *b : rpo_)
    for (size_t k = 0; k < b->numSuccessors(); ++k) {
      auto &p = preds_[index_[b->successor(k)]];
      if (std::find(p.begin(), p.end(), b) == p.end()) p.push_back(b);
    }

  // Dominators: the iterative algorithm of Cooper, Harvey and Kennedy over
  // reverse postorder positions.
  constexpr uint32_t none = UINT32_MAX;
  idom_.assign(n, none);
  idom_[0] = 0;
  auto intersect = [&](uint32_t a, uint32_t b) {
    while (a != b) {
      while (a > b) a = idom_[a];
      while (b > a) b = idom_[b];
    }
    return a;
  };
  for (bool changed = true; changed;) {
    changed = false;
    for (uint32_t i = 1; i < n; ++i) {
      uint32_t nd = none;
      for (BasicBlock *p : preds_[i]) {
        uint32_t pi = index_[p];
        if (idom_[pi] == none) continue;
        nd = nd == none ? pi : intersect(pi, nd);
      }
      if (nd != idom_[i]) {
        idom_[i] = nd;
        changed = true;
      }
    }
  }
  // Number the dominator tree depth first: a dominates b exactly when b's
  // numbers fall within a's.
  std::vector<std::vector<uint32_t>> children(n);
  for (uint32_t i = 1; i < n; ++i) children[idom_[i]].push_back(i);
  enter_.assign(n, 0);
  leave_.assign(n, 0);
  uint32_t clock = 0;
  std::vector<std::pair<uint32_t, size_t>> walk{{0, 0}};
  enter_[0] = clock++;
  while (!walk.empty()) {
    auto &[node, next] = walk.back();
    if (next < children[node].size()) {
      uint32_t c = children[node][next++];
      enter_[c] = clock++;
      walk.push_back({c, 0});
      continue;
    }
    leave_[node] = clock++;
    walk.pop_back();
  }
}

bool CFG::dominates(const BasicBlock *a, const BasicBlock *b) const {
  uint32_t ia = index_.at(a), ib = index_.at(b);
  return enter_[ia] <= enter_[ib] && leave_[ib] <= leave_[ia];
}

void removeUnreachableBlocks(Function &f) {
  CFG cfg(f);
  // Uses of values from dead blocks only occur in dead blocks (dominance), so
  // dropping the dead instructions' operands first leaves no dangling user.
  for (auto &b : f.blocks)
    if (!cfg.reachable(b.get()))
      for (auto &i : b->insts) i->dropOperands();
  f.blocks.erase(std::remove_if(f.blocks.begin(), f.blocks.end(),
                                [&](const std::unique_ptr<BasicBlock> &b) { return !cfg.reachable(b.get()); }),
                 f.blocks.end());
}

} // namespace xb::xir
