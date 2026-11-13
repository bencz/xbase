#pragma once

#include "xir/Analysis.h"
#include "xir/IR.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// Changes to the shape of functions that passes share: splitting a block, copying blocks, and rebuilding SSA form
// where a value gets a second definition.
namespace xb::xir {

// Moves the instructions after position `k` of `b` into a new block (named like `b`) that `b` jumps to, and returns
// it.
BasicBlock *splitAfter(Function &f, BasicBlock *b, size_t k);

// A copy of blocks into function `f` (theirs, or another): each block's copy is named with `suffix`, takes parameters
// of the same types, and holds copies of its instructions, whose operands and targets are the copies of the originals;
// a value from outside the copied blocks (a constant, say) is what `outside` gives for it, or itself. `pairs` lists
// (original, copy) in block order: a stable order of work.
struct BlockCopy {
  std::unordered_map<const BasicBlock *, BasicBlock *> blocks;
  std::unordered_map<const Value *, Value *> values;
  std::vector<std::pair<Value *, Value *>> pairs;
};
BlockCopy copyBlocks(Function &f, const std::vector<BasicBlock *> &originals, const std::string &suffix,
                     const std::function<Value *(Value *)> &outside = {});

// A value of type `t` for a use that never runs: NIL, false, 0.
Value *undefinedValue(Function &f, Type t);

// Rebuilds SSA form for one variable of a type: definitions at the end of some blocks (addAvailable); the value at
// the start or the end of any block is the one that reaches it, through block parameters where definitions meet, made
// on demand (Braun et al., "Simple and Efficient Construction of Static Single Assignment Form"). Only the branches
// listed in `preds` count (those of reachable blocks, say); a block that none reaches gets the type's undefined value
// (no use there runs). The parameters made are added to `placed`, for removeTrivialParams once every variable is done.
class SSAUpdater {
public:
  SSAUpdater(Function &f, Type type, const Predecessors &preds, std::vector<BlockParam *> &placed)
      : f_(f), type_(type), preds_(preds), placed_(placed) {}
  void addAvailable(const BasicBlock *b, Value *v) { atEnd_[b] = v; }
  Value *atStart(BasicBlock *b);
  Value *atEnd(BasicBlock *b);
  // Replaces `from` in user `u` (operands and branch arguments) with the value reaching u's block.
  void rewriteUse(Instruction *u, Value *from);

private:
  Function &f_;
  Type type_;
  const Predecessors &preds_;
  std::vector<BlockParam *> &placed_;
  std::unordered_map<const BasicBlock *, Value *> atEnd_;
  std::unordered_map<const BasicBlock *, Value *> reaching_;
};

// A parameter among `placed` whose arguments (from the branches in `preds`) are all one other value, or itself, is
// that value; removing one may make others so. Removed ones become null in `placed`.
void removeTrivialParams(std::vector<BlockParam *> &placed, const Predecessors &preds);

} // namespace xb::xir
