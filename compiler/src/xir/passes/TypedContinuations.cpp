// Typed continuations: the error block may substitute a value of any type for
// the result of an operation that fails, so type inference gives such an
// operation any type (Types.h), and the values computed from it lose their
// registers and constant type tags. Where the types the operation gives itself
// carry no heap reference (numbers, logicals, dates) or are a single type,
// this pass checks its result instead: `subst.check` right after it tells whether the value is of
// another type (only a substituted one can be), and then `subst.value` takes
// it to a generic copy of the rest of the function, whose operations keep
// giving any type. Type inference gives a checked operation its own types.
//
// The copy is made once per function and entered after each checked
// operation; the values of the function that reach it are joined to the
// copy's own by block parameters (SSA reconstruction on demand, as in Braun et
// al., "Simple and Efficient Construction of Static Single Assignment Form").
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"
#include "xir/Builder.h"
#include "xir/Transform.h"
#include "xir/Types.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace xb::xir {

namespace {

// The function is copied, and each check is an entry into the copy where the
// values live there are joined: both are bounded, so that compile time and
// code size stay so. The checks kept are those of the most nested loops.
constexpr size_t kMaxInstructions = 500;
constexpr size_t kMaxChecks = 16;

// An operation worth checking: in a loop, its error may substitute the
// result, the types it gives itself hold no heap reference (the value can
// stay out of the frame) or are a single type (its tag is a constant: a
// string concatenation), and another operation computes with it (an index
// too) or a loop carries it (a condition tests the type anyway; a variable or
// a call takes the value whatever its type).
bool worthChecking(const Instruction &i, const TypeInfo &types, const Loops &loops) {
  if (i.type() != Type::Item || !loops.innermost(i.parent) || !types.maySubstitute(i)) return false;
  TypeSet own = types.of(&i);
  bool single = (own & (own - 1)) == 0;
  if (own == 0 || ((own & kHeapTypes) != 0 && !single)) return false;
  for (const Instruction *u : i.users())
    switch (u->op) {
    case Op::Binary: case Op::Unary: case Op::ForTest: case Op::SwitchCheck: case Op::Index: case Op::IndexSet:
    case Op::Br: case Op::CondBr:
      return true;
    default:
      break;
    }
  return false;
}

} // namespace

bool typedContinuations(Function &f) {
  size_t count = 0;
  for (auto &b : f.blocks) {
    if (b->unwind) return false; // unwind blocks take no parameters: the copy could not be joined there
    if (Instruction *t = b->terminator(); t && t->op == Op::GenYield) return false; // a generator: values in the frame
    count += b->insts.size();
  }
  if (count > kMaxInstructions) return false;

  std::vector<Instruction *> checked;
  {
    CFG cfg(f);
    TypeInfo types(cfg, Substitution::None);
    Loops loops(cfg);
    for (BasicBlock *b : cfg.order())
      for (auto &i : b->insts)
        if (worthChecking(*i, types, loops)) checked.push_back(i.get());
    std::stable_sort(checked.begin(), checked.end(), [&](const Instruction *a, const Instruction *b) {
      return loops.depth(a->parent) > loops.depth(b->parent);
    });
    if (checked.size() > kMaxChecks) checked.resize(kMaxChecks);
  }
  if (checked.empty()) return false;

  // Each checked operation ends its block; the rest of the block follows.
  std::vector<BasicBlock *> after;
  for (Instruction *i : checked) {
    BasicBlock *b = i->parent;
    size_t k = 0;
    while (b->insts[k].get() != i) ++k;
    after.push_back(splitAfter(f, b, k));
  }
  CFG before(f);
  std::vector<BasicBlock *> originals;
  for (auto &b : f.blocks) originals.push_back(b.get());
  BlockCopy copy = copyBlocks(f, originals, ".any");

  // The check: a value of another type continues in the copy, right after the
  // copy of the operation.
  std::unordered_map<const BasicBlock *, std::pair<Value *, Value *>> entries; // entry block -> (checked, substituted)
  Builder builder(&f);
  for (size_t n = 0; n < checked.size(); ++n) {
    Instruction *i = checked[n];
    BasicBlock *b = i->parent;
    BasicBlock *enter = f.addBlock(b->name + ".subst");
    b->takeTerminator();
    builder.setBlock(b);
    builder.condBr(builder.substCheck(i), enter, after[n]);
    builder.setBlock(enter);
    Value *substituted = builder.substValue(i);
    builder.br(copy.blocks.at(after[n]));
    entries[enter] = {i, substituted};
  }

  // SSA form in the copy: each original value `v` whose copy `v'` the copy still uses is defined by `v'` in the copy
  // and, where a checked operation enters it, by `v` itself (or, for the operation, by the substituted value; by
  // nothing where `v` does not dominate the operation: no use is reached from there before the copy defines it). Only
  // reachable blocks count: the copy of the function's start, and the copies the checks do not lead to, go once this
  // is done.
  CFG now(f);
  Predecessors preds;
  for (auto &[b, terms] : predecessorTerminators(f))
    for (Instruction *t : terms)
      if (now.reachable(t->parent)) preds[b].push_back(t);
  std::vector<BlockParam *> placed;
  for (auto &[orig, dup] : copy.pairs) {
    if (dup->users().empty()) continue;
    SSAUpdater up(f, dup->type(), preds, placed);
    up.addAvailable(definingBlock(dup), dup);
    for (auto &[enter, e] : entries) {
      const BasicBlock *def = definingBlock(orig);
      const BasicBlock *at = static_cast<const Instruction *>(e.first)->parent;
      if (orig == e.first) up.addAvailable(enter, e.second);
      else if (before.reachable(def) && before.dominates(def, at)) up.addAvailable(enter, orig);
      else up.addAvailable(enter, undefinedValue(f, dup->type()));
    }
    std::vector<Instruction *> users = dup->users();
    std::unordered_set<Instruction *> done;
    for (Instruction *u : users)
      if (now.reachable(u->parent) && done.insert(u).second && u->parent != definingBlock(dup)) up.rewriteUse(u, dup);
  }
  removeTrivialParams(placed, preds);
  removeUnreachableBlocks(f);
  return true;
}

} // namespace xb::xir
