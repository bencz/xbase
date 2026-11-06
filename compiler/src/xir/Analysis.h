#pragma once

#include "xir/IR.h"

#include <unordered_map>
#include <vector>

namespace xb::xir {

// The control flow graph of a function: blocks reachable from the entry in
// reverse postorder, their predecessors, and the dominator tree (dominance
// queries in constant time from the tree's entry and exit numbers).
class CFG {
public:
  explicit CFG(const Function &f);

  const std::vector<BasicBlock *> &order() const { return rpo_; }      // reachable blocks, reverse postorder
  bool reachable(const BasicBlock *b) const { return index_.count(b) != 0; }
  const std::vector<BasicBlock *> &preds(const BasicBlock *b) const { return preds_[index_.at(b)]; }
  bool dominates(const BasicBlock *a, const BasicBlock *b) const;
  BasicBlock *idom(const BasicBlock *b) const; // null for the entry
  const std::vector<BasicBlock *> &children(const BasicBlock *b) const { return children_[index_.at(b)]; }

private:
  std::vector<BasicBlock *> rpo_;
  std::unordered_map<const BasicBlock *, uint32_t> index_;   // position in rpo_
  std::vector<std::vector<BasicBlock *>> preds_;              // by position
  std::vector<uint32_t> idom_;                                // by position; the entry is its own
  std::vector<std::vector<BasicBlock *>> children_;           // in the dominator tree, by position
  std::vector<uint32_t> enter_, leave_;                       // dominator tree numbering, by position
};

// For every block, the blocks where its dominance ends: the joins it reaches
// without dominating them (where a definition in it meets other ones).
using Frontiers = std::unordered_map<const BasicBlock *, std::vector<BasicBlock *>>;
Frontiers dominanceFrontiers(const CFG &cfg);

// The blocks of loops, with the number of loops each is in (1 for the body of
// an outermost loop): a loop is a block that dominates the source of an edge
// to it, with the blocks from which that source is reached without passing it.
using LoopDepths = std::unordered_map<const BasicBlock *, unsigned>;
LoopDepths loopDepths(const CFG &cfg);

// Removes the blocks the entry cannot reach (code after RETURN, EXIT, LOOP).
bool removeUnreachableBlocks(Function &f); // true when it removed some

// The terminators that branch to each block, once per terminator however many
// of its edges go there; a pass that redirects edges keeps it up to date.
using Predecessors = std::unordered_map<const BasicBlock *, std::vector<Instruction *>>;
Predecessors predecessorTerminators(const Function &f);

} // namespace xb::xir
