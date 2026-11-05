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

private:
  std::vector<BasicBlock *> rpo_;
  std::unordered_map<const BasicBlock *, uint32_t> index_;   // position in rpo_
  std::vector<std::vector<BasicBlock *>> preds_;              // by position
  std::vector<uint32_t> idom_;                                // by position; the entry is its own
  std::vector<uint32_t> enter_, leave_;                       // dominator tree numbering, by position
};

// Removes the blocks the entry cannot reach (code after RETURN, EXIT, LOOP).
bool removeUnreachableBlocks(Function &f); // true when it removed some

// The terminators that branch to each block, once per terminator however many
// of its edges go there; a pass that redirects edges keeps it up to date.
using Predecessors = std::unordered_map<const BasicBlock *, std::vector<Instruction *>>;
Predecessors predecessorTerminators(const Function &f);

} // namespace xb::xir
