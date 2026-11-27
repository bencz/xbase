#pragma once

#include "xir/Analysis.h"
#include "xir/IR.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Changes to the shape of functions that passes share: splitting a block, copying blocks, and rebuilding SSA form
// where a value gets a second definition.
namespace xb::xir {

class Builder;

// Moves the instructions after position `k` of `b` into a new block (named like `b`, unwinding where `b` does) that `b`
// jumps to, and returns it.
BasicBlock *splitAfter(Function &f, BasicBlock *b, size_t k);
// The same with the instructions from position `k` on (`b` then holds the first `k` and the jump; its parameters stay).
BasicBlock *splitBefore(Function &f, BasicBlock *b, size_t k);

// A copy of blocks into function `f` (theirs, or another): each block's copy is named with `suffix`, takes parameters
// of the same types, and holds copies of its instructions, whose operands and targets are the copies of the originals;
// a value from outside the copied blocks (a constant, say) is what `outside` gives for it, or itself. `pairs` lists
// (original, copy) in block order: a stable order of work.
struct BlockCopy {
  std::unordered_map<const BasicBlock *, BasicBlock *> blocks; // an original -> its copy (where the copy starts)
  std::unordered_map<const BasicBlock *, BasicBlock *> ends;   // an original -> where its copy ends (unwindBody)
  std::vector<const BasicBlock *> order;                       // the originals, in the order they were copied
  std::unordered_map<const Value *, Value *> values;
  std::vector<std::pair<Value *, Value *>> pairs;
};
BlockCopy copyBlocks(Function &f, const std::vector<BasicBlock *> &originals, const std::string &suffix,
                     const std::function<Value *(Value *)> &outside = {});

// A constant of function `f` equal to `c` (each function holds its own constants).
Constant *copyConstant(Function &f, const Constant &c);

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

// Whether `body`, of a function of a carried runtime source, needs nothing of its module in another one but its
// functions (which the runtime exports: XB_LIBRARY_FUNC_PREFIX; not a typed copy, which the runtime need not have
// made alike): no STATIC of it, none of its classes, no codeblock of it made.
bool selfContained(const std::vector<BasicBlock *> &body);

// A value whose hold by an inlined body (inline.enter) nothing can see: a constant, which holds nothing, or a
// codeblock literal, which folding may leave unmade (the body then holds NIL in its place).
bool heldUnseen(const Value *v);

// Removes instruction `i` from its block, its uses undone.
void eraseInstruction(Instruction *i);

// A frame variable with no cell and no reference passed in: read and written directly.
bool plainVar(const Var *v);

// What a variable's copy in function `f` (an inlined body's, a specialized copy's) keeps of `from`: its table (its
// items made constants of `f`) and its AS types.
void copyVarFacts(Function &f, const Var &from, Var &to);

// A function's body run in another one (an inlined call, a fused generator): the callee's variables, those of the
// function its copied blocks use instead. Its captures are the variables `captures` gives; its frame variables (but
// those below `firstVar`: a resume function's enumerator) new ones of `f`. A parameter, when an argument is a
// reference (`refArgs`: @x), keeps reading and writing through what it holds, as do the callee's other variables
// marked so (the parameters of a body inlined in it before).
using VarMap = std::unordered_map<const Var *, Var *>;
VarMap bodyVars(Function &f, const Function &callee, bool refArgs, const std::vector<Var *> &captures,
                uint32_t firstVar = 0);
// The copied instructions use the function's variables (and captures); a plain one is read and written directly.
void useBodyVars(const BlockCopy &copy, const VarMap &vars);
// At the body's entry: the parameters (frame slots `firstVar` on) given the arguments, NIL past the last, and the
// LOCALs NIL; at its end, every variable NIL, as the callee's frame would be gone (promote-variables drops the stores
// of those it takes).
void enterBody(Builder &b, const Function &callee, const VarMap &vars, const std::vector<Value *> &args,
               uint32_t firstVar = 0);
void leaveBody(Builder &b, const Function &callee, const VarMap &vars);
// Where the blocks of a body run in place unwind when the code around it unwinds to `handler` (a BEGIN SEQUENCE's, a
// FOR EACH's cleanup), as its call would return with the error: a block that leaves the body's frame (inline.leave)
// and goes on to `handler` (returned: the one the block entering the body unwinds to), and for a copied block that
// ends with bodies the callee inlined open, one that leaves those first. Each block's unwind block so starts with the
// bodies it has open (openInlineBodies): a copied block where an inlined body starts or ends is split first, so that
// each inline.enter and inline.leave starts a block (BlockCopy::ends: where the copy now ends), and every operation
// that may unwind is in a block with its bodies open. Nothing when `handler` is null (the
// function's exit): null.
BasicBlock *unwindBody(Function &f, const Function &callee, BlockCopy &copy, BasicBlock *handler);

// SSA form again for values that copies of blocks define once more: each entry lists one value's definitions with
// their blocks, the original first. Its reachable uses outside the blocks in `inside` (the original and copied blocks)
// take the definition that reaches them, through block parameters where they meet (SSAUpdater), which it returns.
using Definitions = std::vector<std::pair<const BasicBlock *, Value *>>;
std::vector<BlockParam *> joinDefinitions(Function &f, const std::vector<Definitions> &defs,
                                         const std::unordered_set<const BasicBlock *> &inside);

} // namespace xb::xir
