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
// A guard is such a check of a value (a parameter, a variable, a call's result,
// an operation whose error may substitute it) whose type the code would not
// know where it enters a loop that computes
// with numbers (a FOR's start or limit, an operand of arithmetic): checked
// once where it is made, against the numbers (`subst.check` with a mask), the
// loop keeps its values typed and out of the frame; another type goes on in
// the copy, as before.
//
// A speculation is a guard, in a loop, of a value the loop computes with: + - * of two integers whose ranges may
// overflow, against the integers alone (an overflow's double goes on in the copy). The loop's values then stay
// integers (Types.h: those whose ranges keep them integers need no guard), in registers.
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
// code size stay so. The checks kept are those of the most nested loops. A
// larger function copied grows past what the code generator optimizes
// (kLargeFunction): pi's PiDigits, of 2000 instructions, ran 4.9 times slower.
constexpr size_t kMaxInstructions = 500;
constexpr size_t kMaxChecks = 64;
// Rounds of the choice of checks past the first (worthChecking): each may find an operation that a check it did not
// choose leaves substitutable; past them, what is left only stays generic.
constexpr int kCheckRounds = 4;

// An operation of a loop that takes its operand as a number (an operand of another type is an error, or another
// operation altogether).
bool numericUse(const Instruction &u) {
  if (u.op == Op::ForTest) return true;
  if (u.op == Op::Unary) return u.unop == UnOp::Neg;
  if (u.op != Op::Binary) return false;
  switch (u.binop) {
  case BinOp::Sub: case BinOp::Mul: case BinOp::Div: case BinOp::Mod: case BinOp::Pow:
  case BinOp::Lt: case BinOp::Le: case BinOp::Gt: case BinOp::Ge:
    return true;
  default:
    return false;
  }
}

// Whether `v`, made outside loop `l`, reaches a numeric use in it: directly, or as what a header's parameter starts
// with (a FOR's variable).
bool entersAsNumber(const Value *v, const Loop &l) {
  for (const Instruction *u : v->users()) {
    if (!l.contains(u->parent)) continue;
    if (numericUse(*u)) return true;
  }
  for (const Instruction *u : v->users()) {
    if (l.contains(u->parent)) continue;
    for (const auto &t : u->targets) {
      if (t.block != l.header) continue;
      for (size_t k = 0; k < t.args.size(); ++k)
        if (t.args[k] == v)
          for (const Instruction *pu : t.block->params[k]->users())
            if (l.contains(pu->parent) && numericUse(*pu)) return true;
    }
  }
  return false;
}

// Whether `i` is made outside an innermost loop that takes it as a number.
bool entersLoopAsNumber(const Instruction &i, const Loops &loops) {
  for (auto &l : loops.all())
    if (loops.innermost(l->header) == l.get() && !l->contains(i.parent) && entersAsNumber(&i, *l)) return true;
  return false;
}

// Whether another operation computes with `i` (an index too), or a branch carries it: its type then matters to them
// (a condition tests the type anyway; a variable or a call takes the value whatever its type).
bool computedWith(const Instruction &i) {
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

// An operation worth checking: in a loop, its error may substitute the
// result, the types it gives itself (`types`: as if every operation were
// checked) hold no heap reference (the value can stay out of the frame) or are
// a single type (its tag is a constant: a string concatenation), and it is
// computed with (computedWith). Whether it may substitute is `view`'s: first
// `types`' own, then the types with the checks chosen so far only, where an
// operation not chosen may give any type (a call whose argument it gives, TOTAL
// of its arguments' types in `types`, may then fail).
bool worthChecking(const Instruction &i, const TypeInfo &types, const TypeInfo &view, const Loops &loops) {
  if (i.type() != Type::Item || !loops.innermost(i.parent) || !view.maySubstitute(i)) return false;
  TypeSet own = types.of(&i);
  bool single = (own & (own - 1)) == 0;
  if (own == 0 || ((own & kHeapTypes) != 0 && !single)) return false;
  return computedWith(i);
}

// A value worth a guard: made outside an innermost loop that takes it as a number, of a type the code would not know
// there (`types` as code generation sees them: what an error may substitute counts), which may be a number.
bool worthGuarding(const Instruction &i, const TypeInfo &types, const Loops &loops) {
  TypeSet t = types.of(&i);
  if (i.type() != Type::Item || (t & ~kNumberTypes) == 0 || (t & kNumberTypes) == 0) return false;
  return entersLoopAsNumber(i, loops);
}

// The values of the loops that are integers once the speculations hold: the greatest set where each is an
// integer already, + - * of two of them (its guard: no overflow), Int() of one divided by a constant (Types.h), or a
// block parameter all of whose arguments are (a value the loop carries: its start and what each pass computes). Found
// by taking out, until none is left to take, what breaks the rules.
std::unordered_set<const Value *> speculatedIntegers(const Function &f, const CFG &cfg, const TypeInfo &types,
                                                     const Loops &loops) {
  std::unordered_set<const Value *> set;
  std::vector<const Value *> candidates;
  for (BasicBlock *b : cfg.order()) {
    if (!loops.innermost(b)) continue;
    for (auto &p : b->params)
      if (p->type() == Type::Item) candidates.push_back(p.get());
    for (auto &i : b->insts)
      if (i->type() == Type::Item) candidates.push_back(i.get());
  }
  set.insert(candidates.begin(), candidates.end());
  std::unordered_map<const Value *, std::vector<const Value *>> incoming;
  for (auto &[block, terms] : predecessorTerminators(f))
    for (const Instruction *t : terms) {
      if (!cfg.reachable(t->parent)) continue;
      for (const Target &tg : t->targets)
        for (size_t k = 0; k < tg.args.size(); ++k) incoming[tg.block->params[k].get()].push_back(tg.args[k]);
    }
  auto integer = [&](const Value *v) { return types.of(v) == kIntType || set.count(v) != 0; };
  auto holds = [&](const Value *v) {
    if (const auto *i = dynamic_cast<const Instruction *>(v)) {
      if (arithmetic(*i)) return integer(i->operand(0)) && integer(i->operand(1));
      if (const Value *x = intQuotientDividend(*i)) return integer(x);
      return types.of(i) == kIntType;
    }
    auto it = incoming.find(v);
    if (it == incoming.end() || it->second.empty()) return false;
    return std::all_of(it->second.begin(), it->second.end(), integer);
  };
  for (bool changed = true; changed;) {
    changed = false;
    for (const Value *v : candidates)
      if (set.count(v) && !holds(v)) {
        set.erase(v);
        changed = true;
      }
  }
  return set;
}

// A value worth a speculation (see above): + - * of two integers once the speculations hold (`ints`), which may
// overflow (`types` as code generation sees them).
bool worthSpeculating(const Instruction &i, const TypeInfo &types, const std::unordered_set<const Value *> &ints) {
  return i.type() == Type::Item && ints.count(&i) && computedWith(i) && arithmetic(i) &&
         types.of(&i) != kIntType && !types.fits(i);
}

} // namespace

bool typedContinuations(Function &f) {
  if (isResumable(f)) return false; // its values live in the frame
  for (auto &b : f.blocks)
    if (b->unwind) return false; // unwind blocks take no parameters: the copy could not be joined there
  if (instructionCount(f) > kMaxInstructions) return false;

  std::vector<Instruction *> checked;
  std::unordered_map<const Instruction *, TypeSet> guards; // the types each guard lets on
  {
    CFG cfg(f);
    TypeInfo types(cfg, Substitution::None), seen(cfg);
    Loops loops(cfg);
    std::unordered_set<const Value *> ints = speculatedIntegers(f, cfg, seen, loops);
    std::unordered_set<const Instruction *> chosen;
    auto found = [&](const TypeInfo &view) {
      std::vector<const Instruction *> list;
      for (BasicBlock *b : cfg.order())
        for (auto &i : b->insts)
          if (!chosen.count(i.get()) && worthChecking(*i, types, view, loops)) list.push_back(i.get());
      return list;
    };
    for (const Instruction *i : found(types)) chosen.insert(i);
    for (int round = 0; round < kCheckRounds; ++round) {
      std::vector<const Instruction *> list = found(TypeInfo(cfg, Substitution::Any, &chosen));
      // Of those, the ones that may fail even with all of them checked: one found only because another of this round
      // gives any type (a product of Asc()'s result, while Asc() is unchecked) cannot fail once that one is checked.
      std::unordered_set<const Instruction *> all = chosen;
      all.insert(list.begin(), list.end());
      TypeInfo with(cfg, Substitution::Any, &all);
      bool grew = false;
      for (const Instruction *i : list)
        if (with.maySubstitute(*i)) grew = chosen.insert(i).second;
      if (!grew) break;
    }
    for (BasicBlock *b : cfg.order())
      for (auto &i : b->insts) {
        if (worthSpeculating(*i, seen, ints)) { // a guard against fewer types than a check's
          checked.push_back(i.get());
          guards[i.get()] = kIntType;
        } else if (chosen.count(i.get())) {
          checked.push_back(i.get());
        } else if (worthGuarding(*i, seen, loops)) {
          checked.push_back(i.get());
          guards[i.get()] = kNumberTypes;
        }
      }
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
    Instruction *check = builder.substCheck(i);
    if (auto g = guards.find(i); g != guards.end()) check->imm = g->second;
    builder.condBr(check, enter, after[n]);
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
