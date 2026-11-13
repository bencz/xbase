#pragma once

#include "xir/IR.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
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

// A natural loop: a header that dominates the source of an edge to it, with the blocks from which that source is
// reached without passing the header; the loops of one header are one. Unwind edges count: an unwind block reached
// from inside a loop that leads back to its header is in it.
class Loop {
public:
  BasicBlock *header = nullptr;
  std::vector<BasicBlock *> blocks; // in reverse postorder: the header first
  Loop *parent = nullptr;           // the innermost loop around it
  unsigned depth = 1;               // 1 for an outermost loop

  bool contains(const BasicBlock *b) const { return set_.count(b) != 0; }

private:
  friend class Loops;
  std::unordered_set<const BasicBlock *> set_;
};

// The natural loops of a function, outermost first (a loop before those inside it), and the innermost loop of each
// block.
class Loops {
public:
  explicit Loops(const CFG &cfg);
  const std::vector<std::unique_ptr<Loop>> &all() const { return loops_; }
  const Loop *innermost(const BasicBlock *b) const; // null outside every loop
  unsigned depth(const BasicBlock *b) const {       // the loops around it, 0 outside every loop
    const Loop *l = innermost(b);
    return l ? l->depth : 0;
  }

private:
  std::vector<std::unique_ptr<Loop>> loops_;
  std::unordered_map<const BasicBlock *, Loop *> innermost_;
};

// The LOCALs that are not shared, not parameters, and only loaded and stored
// directly (a FOR EACH variable is not: it holds the loop's reference): what
// only the function sees, through its own loads and stores.
std::vector<Var *> directLocals(const Function &f);

// Removes the blocks the entry cannot reach (code after RETURN, EXIT, LOOP).
bool removeUnreachableBlocks(Function &f); // true when it removed some

// The terminators that branch to each block, once per terminator however many
// of its edges go there; a pass that redirects edges keeps it up to date.
using Predecessors = std::unordered_map<const BasicBlock *, std::vector<Instruction *>>;
Predecessors predecessorTerminators(const Function &f);

// The inline.enters open where each reachable block starts, innermost last: an inlined body is entered at its start
// and left at its end. False when they do not nest so (a leave with none open, two paths into a block with different
// ones open, a return or yield with one open).
using InlineBodies = std::unordered_map<const BasicBlock *, std::vector<const Instruction *>>;
bool openInlineBodies(const CFG &cfg, InlineBodies &at);

} // namespace xb::xir
