// Peeling for types: a value a loop carries that starts with one type and gets another from the first pass on (a
// sum that starts at the integer 0 and adds doubles: `nSum := 0 ... nSum += x / y`) makes the loop test its type at
// every pass, and keeps it where both kinds of number fit (an integer register, moved to the floating-point unit and
// back at every pass). The first pass runs once as a copy in front of the loop: the loop proper is then entered with
// the values the passes give, of one type, which code generation keeps typed (a double stays a double).
//
// A loop is peeled when it is innermost and small, holds no suspension point or inlined body's frame (each of those is
// one of a kind), and a parameter of its header gets one number type from the loop's own passes and another number
// type from outside. The copy (with the block each check of the loop leads to, which only its check may reach) has its
// edges back to its header go to the loop's header, and the edges that entered the loop enter the copy; values of the
// loop used after it are joined with the copy's (SSA reconstruction, as in TypedContinuations.cpp).
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"
#include "xir/Transform.h"
#include "xir/Types.h"

#include <unordered_set>

namespace xb::xir {

namespace {

constexpr size_t kMaxFunction = 600; // instructions: the copies stay bounded
constexpr size_t kMaxLoop = 80;      // instructions of a loop copied
constexpr unsigned kMaxPeeled = 4;   // loops peeled in a function

// The types parameter `k` of the loop's header gets from outside the loop (`entry`) and from its passes (`passes`).
void headerArgTypes(const Function &f, const Loop &l, size_t k, const TypeInfo &types, TypeSet &entry,
                    TypeSet &passes) {
  entry = passes = 0;
  for (auto &b : f.blocks)
    if (Instruction *t = b->terminator())
      for (const Target &tg : t->targets)
        if (tg.block == l.header) (l.contains(b.get()) ? passes : entry) |= types.of(tg.args[k]);
}

bool worthPeeling(const Function &f, const Loop &l, const Loops &loops, const TypeInfo &types) {
  if (loops.innermost(l.header) != &l) return false;
  size_t size = 0;
  for (const BasicBlock *b : l.blocks) {
    if (b->unwind) return false;
    size += b->insts.size();
    for (auto &i : b->insts)
      if (isSuspension(i->op) || i->op == Op::InlineEnter || i->op == Op::InlineLeave) return false;
  }
  if (size > kMaxLoop) return false;
  for (size_t k = 0; k < l.header->params.size(); ++k) {
    if (l.header->params[k]->type() != Type::Item) continue;
    TypeSet entry, passes;
    headerArgTypes(f, l, k, types, entry, passes);
    bool single = passes != 0 && (passes & (passes - 1)) == 0;
    if (single && entry != 0 && (passes & ~kNumberTypes) == 0 && (entry & ~kNumberTypes) == 0 &&
        (entry & ~passes) != 0)
      return true;
  }
  return false;
}

void peel(Function &f, const Loop &l) {
  std::vector<BasicBlock *> blocks(l.blocks.begin(), l.blocks.end());
  for (BasicBlock *b : l.blocks) // a check's own block, outside the loop
    if (Instruction *t = b->terminator(); t && t->op == Op::CondBr && asInstruction(t->operand(0), Op::SubstCheck) &&
                                          !l.contains(t->targets[0].block))
      blocks.push_back(t->targets[0].block);
  std::vector<BasicBlock *> outsiders; // the blocks before the copy: those that may enter the loop
  for (auto &b : f.blocks)
    if (!l.contains(b.get())) outsiders.push_back(b.get());
  BlockCopy copy = copyBlocks(f, blocks, ".peel");
  BasicBlock *first = copy.blocks.at(l.header);
  for (BasicBlock *b : blocks) // the copy's edges back to its header lead into the loop
    if (Instruction *t = copy.blocks.at(b)->terminator())
      for (Target &tg : t->targets)
        if (tg.block == first) tg.block = l.header;
  for (BasicBlock *b : outsiders) // the loop is entered through the copy
    if (Instruction *t = b->terminator())
      for (Target &tg : t->targets)
        if (tg.block == l.header) tg.block = first;

  // Values of the loop used after it: the loop's, or the copy's, whichever pass left it.
  CFG now(f);
  Predecessors preds;
  for (auto &[b, terms] : predecessorTerminators(f))
    for (Instruction *t : terms)
      if (now.reachable(t->parent)) preds[b].push_back(t);
  std::unordered_set<const BasicBlock *> inside(blocks.begin(), blocks.end());
  for (auto &[b, dup] : copy.blocks) inside.insert(dup);
  std::vector<BlockParam *> placed;
  for (auto &[orig, dup] : copy.pairs) {
    std::vector<Instruction *> after;
    for (Instruction *u : orig->users())
      if (!inside.count(u->parent) && now.reachable(u->parent)) after.push_back(u);
    if (after.empty()) continue;
    SSAUpdater up(f, orig->type(), preds, placed);
    up.addAvailable(definingBlock(orig), orig);
    up.addAvailable(definingBlock(dup), dup);
    std::unordered_set<Instruction *> done;
    for (Instruction *u : after)
      if (done.insert(u).second) up.rewriteUse(u, orig);
  }
  removeTrivialParams(placed, preds);
}

} // namespace

bool peelForTypes(Function &f) {
  if (instructionCount(f) > kMaxFunction) return false;
  bool changed = false;
  for (unsigned n = 0; n < kMaxPeeled; ++n) {
    CFG cfg(f);
    TypeInfo types(cfg);
    Loops loops(cfg);
    const Loop *chosen = nullptr;
    for (auto &l : loops.all())
      if (worthPeeling(f, *l, loops, types)) {
        chosen = l.get();
        break;
      }
    if (!chosen) break;
    peel(f, *chosen); // a peeled loop is entered with the types of its passes: it is not chosen again
    removeUnreachableBlocks(f);
    changed = true;
  }
  return changed;
}

} // namespace xb::xir
