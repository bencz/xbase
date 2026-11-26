// Typed continuations: the error block may substitute a value of any type for
// the result of an operation that fails, so type inference gives such an
// operation any type (Types.h), and the values computed from it lose their
// registers and constant type tags. Where the types the operation gives itself
// carry no heap reference (numbers, logicals, dates) or are a single type,
// this pass checks its result instead: `subst.check` right after it tells whether the value is of another type (only a
// substituted one can be), and then `subst.value` takes it to a generic copy of the code around, whose operations keep
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
// A function of at most kMaxInstructions without unwind blocks is copied whole, once, and the copy runs to its end.
// Another is copied by regions, one at a time in the control flow's order: a loop nest, or a guard with the code up to
// the first loop it guards; the copy of a region leaves it where the region does, and the values that live on are
// joined there to the original's, with the types the operation that entered the copy gives itself (`subst.value
// joined`). A join a later loop follows gets a guard against the types the original's side gives it (a join's own
// type is theirs and the copy's together), the first in its block, and the next region begins there; another join of
// that block is passed on to the block after the guard and guarded there. The values that reach a copy are joined to
// the copy's own by block parameters (SSA reconstruction on demand, as in Braun et al., "Simple and Efficient
// Construction of Static Single Assignment Form").
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"
#include "xir/Builder.h"
#include "xir/Transform.h"
#include "xir/Types.h"

#include <algorithm>
#include <functional>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace xb::xir {

namespace {

// A copy (of a whole function or of a region) and its checks are bounded, so that compile time and code size stay so
// (the regions are disjoint: a function's copies are at most as large as it, as a whole copy is); the checks kept are
// those of the most nested loops. A larger copy grows past what the code generator optimizes well:
// pi's PiDigits, a function of 2000 instructions copied whole, ran 4.9 times slower.
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

// Whether `v` is made outside an innermost loop that takes it as a number.
bool entersLoopAsNumber(const Value &v, const Loops &loops) {
  for (auto &l : loops.all())
    if (loops.innermost(l->header) == l.get() && !l->contains(definingBlock(&v)) && entersAsNumber(&v, *l))
      return true;
  return false;
}

// Whether another operation computes with `i` (an index too), or a branch carries it: its type then matters to them
// (a condition tests the type anyway; a variable or a call takes the value whatever its type).
bool computedWith(const Instruction &i) {
  for (const Instruction *u : i.users())
    switch (u->op) {
    case Op::Binary: case Op::Unary: case Op::ForTest: case Op::SwitchCheck: case Op::Index: case Op::TableIndex:
    case Op::IndexSet: case Op::JoinStep: case Op::JoinEnd:
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

// A value read or received inside an innermost loop, of a type no operation of the loop decides (an element, an
// object's variable, a variable in memory, a call's or a send's result), that operations of the same loop take as a
// number twice or more: guarded once where it is made, those operations are typed (cannotFail), with no check after
// each. One such use only would move its check to the value, for nothing (and give LoopSplit a nest without a call
// where it found one: linpack's, split whole, ran 1.5 times the instructions).
bool readAsNumberInLoop(const Instruction &i, const Loops &loops) {
  switch (i.op) {
  case Op::Index: case Op::TableIndex: case Op::ObjectGet: case Op::Load: case Op::Call: case Op::Send:
    break;
  default:
    return false;
  }
  const Loop *l = loops.innermost(i.parent);
  if (!l) return false;
  size_t uses = 0;
  for (const Instruction *u : i.users())
    if (l->contains(u->parent) && numericUse(*u)) ++uses;
  return uses >= 2;
}

// A value worth a guard (`types` as code generation sees them: what an error may substitute counts), of a type the
// code would not know there, which may be a number: made outside an innermost loop that takes it as a number (an
// operation's result, or a block parameter: where the ways out of a region copied before join), or read inside one
// that does (readAsNumberInLoop).
bool worthGuarding(const Value &v, const TypeInfo &types, const Loops &loops) {
  TypeSet t = types.of(&v);
  if (v.type() != Type::Item || (t & ~kNumberTypes) == 0 || (t & kNumberTypes) == 0) return false;
  const auto *i = dynamic_cast<const Instruction *>(&v);
  return entersLoopAsNumber(v, loops) || (i && readAsNumberInLoop(*i, loops));
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

// The blocks of the region of a check that ends block `at`: the outermost loop that holds it; for a check
// outside every loop (a guard of a value entering loops), `at`, the first outermost loop `at` dominates of those
// around a loop that `guarded` tells the guard is for, and the blocks on the ways from `at` to it. A value that
// enters loops after that one is joined at its ways out, and guarded again there. Empty when it guards no loop.
std::unordered_set<BasicBlock *> regionOf(BasicBlock *at, const CFG &cfg, const Loops &loops,
                                          const std::unordered_map<const BasicBlock *, size_t> &rpo,
                                          const std::function<bool(const Loop &)> &guarded) {
  std::unordered_set<BasicBlock *> in;
  const Loop *outer = loops.innermost(at);
  while (outer && outer->parent) outer = outer->parent;
  if (outer) {
    in.insert(outer->blocks.begin(), outer->blocks.end());
    return in;
  }
  std::unordered_set<const BasicBlock *> forward{at}; // reached from `at` without leaving what it dominates
  std::vector<const BasicBlock *> work{at};
  while (!work.empty()) {
    const BasicBlock *b = work.back();
    work.pop_back();
    for (size_t k = 0; k < b->numSuccessors(); ++k) {
      const BasicBlock *n = b->successor(k);
      if (cfg.reachable(n) && cfg.dominates(at, n) && forward.insert(n).second) work.push_back(n);
    }
  }
  const Loop *first = nullptr;
  for (auto &l : loops.all()) {
    if (!guarded(*l)) continue;
    const Loop *top = l.get();
    while (top->parent) top = top->parent;
    if (forward.count(top->header) && (!first || rpo.at(top->header) < rpo.at(first->header))) first = top;
  }
  if (!first) return in;
  in.insert(first->blocks.begin(), first->blocks.end());
  std::vector<const BasicBlock *> back{first->header}; // the ways from `at` to the loop
  std::unordered_set<const BasicBlock *> seen{first->header};
  while (!back.empty()) {
    const BasicBlock *b = back.back();
    back.pop_back();
    for (const BasicBlock *p : cfg.preds(b))
      if (forward.count(p) && seen.insert(p).second) {
        in.insert(const_cast<BasicBlock *>(p));
        back.push_back(p);
      }
  }
  in.insert(at);
  return in;
}

// A check: the value tested (an operation's result, or a block parameter its block tests first), the block it ends,
// and the rest of that block, which the typed code goes on with.
struct Check {
  Value *value;
  BasicBlock *block;
  BasicBlock *after = nullptr;
  TypeSet guard = 0; // the types a guard lets on (0: a check, the operation's own types)
  bool join = false; // a guard of a join of a region's ways out, for the next loop (else: those that take it as a
                     // number)
};

// The joins of values at the ways out of the regions copied, with the types their original side gives: they hold
// another type only when the copy ran.
using Joins = std::unordered_map<const BlockParam *, TypeSet>;

// A join worth a guard (against `own`, the types of its original side): it may be of another type now, of types code
// keeps out of the frame or of a single one, and a loop follows (what it computes from the value, too, would
// otherwise be of any type there).
bool worthGuardingJoin(const BlockParam &p, TypeSet own, const TypeInfo &types, const CFG &cfg, const Loops &loops) {
  TypeSet t = types.of(&p);
  own &= t;
  if (own == 0 || own == t || ((own & kHeapTypes) != 0 && (own & (own - 1)) != 0)) return false;
  for (auto &l : loops.all())
    if (!l->contains(p.block) && cfg.dominates(p.block, l->header)) return true;
  return false;
}

// A region: its blocks, in a stable order, and its checks, the most nested first.
struct Region {
  std::vector<BasicBlock *> blocks;
  std::vector<Check> checks;
  bool whole = false; // the function's every block: the copy runs to the end
};

// The checks worth making in the blocks not in `done` (those already copied, and their copies); `joins` those of
// the regions copied.
std::vector<Check> chooseChecks(Function &f, const std::unordered_set<const BasicBlock *> &done, const Joins &joins) {
  CFG cfg(f);
  TypeInfo types(cfg, Substitution::None), seen(cfg);
  Loops loops(cfg);
  std::unordered_set<const Value *> ints = speculatedIntegers(f, cfg, seen, loops);
  // The guards first (values entering loops, speculations, joins), then the checks, chosen with the guards assumed:
  // an operation a guard makes safe (a FOR counter's next value, once its start is guarded) needs none.
  std::vector<Check> checks;
  TypeInfo::Guards guards;
  for (BasicBlock *b : cfg.order()) {
    if (done.count(b)) continue;
    for (auto &p : b->params) {
      auto j = joins.find(p.get());
      if (j != joins.end() && worthGuardingJoin(*p, j->second, seen, cfg, loops))
        checks.push_back({p.get(), b, nullptr, j->second & seen.of(p.get()), true});
      else if (worthGuarding(*p, seen, loops))
        checks.push_back({p.get(), b, nullptr, seen.of(p.get()) & kNumberTypes});
    }
    for (auto &i : b->insts) {
      if (worthSpeculating(*i, seen, ints)) { // a guard against fewer types than a check's
        checks.push_back({i.get(), b, nullptr, kIntType});
      } else if (worthGuarding(*i, seen, loops)) { // against the numbers it gives itself (Max() of integers: one)
        TypeSet own = types.of(i.get()) & kNumberTypes;
        checks.push_back({i.get(), b, nullptr, own ? own : kNumberTypes});
      }
    }
  }
  for (const Check &c : checks) guards[c.value] = c.guard;
  std::unordered_set<const Instruction *> chosen;
  auto found = [&](const TypeInfo &view) {
    std::vector<const Instruction *> list;
    for (BasicBlock *b : cfg.order())
      if (!done.count(b))
        for (auto &i : b->insts)
          if (!chosen.count(i.get()) && !guards.count(i.get()) && worthChecking(*i, types, view, loops))
            list.push_back(i.get());
    return list;
  };
  for (const Instruction *i : found(TypeInfo(cfg, Substitution::None, nullptr, &guards))) chosen.insert(i);
  for (int round = 0; round < kCheckRounds; ++round) {
    std::vector<const Instruction *> list = found(TypeInfo(cfg, Substitution::Any, &chosen, &guards));
    // Of those, the ones that may fail even with all of them checked: one found only because another of this round
    // gives any type (a product of Asc()'s result, while Asc() is unchecked) cannot fail once that one is checked.
    std::unordered_set<const Instruction *> all = chosen;
    all.insert(list.begin(), list.end());
    TypeInfo with(cfg, Substitution::Any, &all, &guards);
    bool grew = false;
    for (const Instruction *i : list)
      if (with.maySubstitute(*i)) grew = chosen.insert(i).second;
    if (!grew) break;
  }
  for (BasicBlock *b : cfg.order())
    if (!done.count(b))
      for (auto &i : b->insts)
        if (chosen.count(i.get())) checks.push_back({i.get(), b});
  // In the code's order: a block's guarded parameters, then its operations as they come.
  std::unordered_map<const BasicBlock *, size_t> rpo;
  for (BasicBlock *b : cfg.order()) rpo.emplace(b, rpo.size());
  auto at = [&](const Check &c) {
    const auto *i = dynamic_cast<const Instruction *>(c.value);
    return std::make_pair(rpo.at(c.block), i ? c.block->indexOf(i) + 1 : 0);
  };
  std::stable_sort(checks.begin(), checks.end(), [&](const Check &x, const Check &y) { return at(x) < at(y); });
  return checks;
}

// The regions of `checks` (those that share a block are one), in the control flow's order, without those that hold a
// block of `done` or one that unwinds (unwind blocks take no parameters: a copy that unwinds could not be joined
// there), or are too large.
std::vector<Region> regionsOf(Function &f, const std::vector<Check> &checks,
                              const std::unordered_set<const BasicBlock *> &done) {
  CFG cfg(f);
  Loops loops(cfg);
  auto byDepth = [&](std::vector<Check> &list) {
    std::stable_sort(list.begin(), list.end(), [&](const Check &x, const Check &y) {
      return loops.depth(x.block) > loops.depth(y.block);
    });
    if (list.size() > kMaxChecks) list.resize(kMaxChecks);
  };
  // A small function is one region, whole: its copy runs to the end, so no join of its ways out loses types.
  bool unwinds = std::any_of(cfg.order().begin(), cfg.order().end(), [](const BasicBlock *b) {
    return b->unwind != nullptr;
  });
  std::unordered_map<const BasicBlock *, size_t> rpo;
  for (BasicBlock *b : cfg.order()) rpo.emplace(b, rpo.size());
  // The blocks of a region in the function's order (the copy's then is the same).
  std::unordered_map<const BasicBlock *, size_t> position;
  for (auto &b : f.blocks)
    if (cfg.reachable(b.get())) position.emplace(b.get(), position.size());
  if (done.empty() && !unwinds && instructionCount(cfg.order()) <= kMaxInstructions) {
    Region whole{{}, checks, true};
    for (auto &b : f.blocks)
      if (cfg.reachable(b.get())) whole.blocks.push_back(b.get());
    byDepth(whole.checks);
    return {whole};
  }
  std::vector<std::unordered_set<BasicBlock *>> found;
  std::vector<size_t> parent; // union-find over the regions found
  auto root = [&](size_t r) {
    while (parent[r] != r) r = parent[r] = parent[parent[r]];
    return r;
  };
  std::unordered_map<const BasicBlock *, size_t> regionOfBlock;
  std::vector<size_t> regionOfCheck;
  for (const Check &c : checks) {
    auto guarded = [&](const Loop &l) {
      return c.join || (loops.innermost(l.header) == &l && entersAsNumber(c.value, l));
    };
    std::unordered_set<BasicBlock *> blocks = regionOf(c.block, cfg, loops, rpo, guarded);
    if (blocks.empty()) {
      regionOfCheck.push_back(SIZE_MAX);
      continue;
    }
    size_t r = found.size();
    found.push_back(std::move(blocks));
    parent.push_back(r);
    regionOfCheck.push_back(r);
    for (BasicBlock *b : found.back()) {
      auto [it, fresh] = regionOfBlock.emplace(b, r);
      if (!fresh) parent[root(r)] = root(it->second);
    }
  }
  std::map<size_t, Region> merged; // by root
  for (size_t k = 0; k < checks.size(); ++k)
    if (regionOfCheck[k] != SIZE_MAX) merged[root(regionOfCheck[k])].checks.push_back(checks[k]);
  for (auto &[b, r] : regionOfBlock) merged[root(r)].blocks.push_back(const_cast<BasicBlock *>(b));
  std::vector<Region> regions;
  for (auto &[r, region] : merged) {
    std::sort(region.blocks.begin(), region.blocks.end(), [&](const BasicBlock *x, const BasicBlock *y) {
      return position.at(x) < position.at(y);
    });
    bool excluded = std::any_of(region.blocks.begin(), region.blocks.end(), [&](const BasicBlock *b) {
      return done.count(b) != 0 || b->unwind != nullptr;
    });
    if (excluded || instructionCount(region.blocks) > kMaxInstructions) continue;
    byDepth(region.checks);
    regions.push_back(std::move(region));
  }
  auto first = [&](const Region &r) {
    size_t n = SIZE_MAX;
    for (const BasicBlock *b : r.blocks) n = std::min(n, rpo.at(b));
    return n;
  };
  std::sort(regions.begin(), regions.end(), [&](const Region &x, const Region &y) { return first(x) < first(y); });
  return regions;
}

// Copies `region`, its checks entering the copy, and joins the values that live past it (adding the joins to `joins`);
// adds the blocks it made and copied to `done`.
void copyRegion(Function &f, Region &region, std::unordered_set<const BasicBlock *> &done, Joins &joins) {
  // Each check ends its block; the rest of the block follows, in the region with it. A guarded parameter's block
  // holds its guard alone; the next guarded parameter of the same block is passed on to the block that follows as a
  // parameter of that block, guarded there (on the way into the copy before its guard, the value is the unchecked
  // parameter).
  std::unordered_set<const BasicBlock *> in(region.blocks.begin(), region.blocks.end());
  auto split = [&](BasicBlock *b, size_t k) {
    BasicBlock *after = splitBefore(f, b, k);
    in.insert(after);
    return after;
  };
  std::unordered_map<const BasicBlock *, BasicBlock *> lastGuard; // a block -> the block of its last guard so far
  for (Check &c : region.checks) {
    if (c.value->kind() != Value::Kind::Param) continue;
    auto [it, first] = lastGuard.emplace(c.block, c.block);
    BasicBlock *b = c.block;
    if (!first) { // the rest of the block, which the previous guard's block alone branches to
      Instruction *br = it->second->terminator();
      b = br->targets[0].block;
      BlockParam *q = b->addParam(Type::Item);
      c.value->replaceAllUsesWith(q);
      br->addTargetArg(0, c.value);
      c.value = q;
    }
    c.after = split(b, 0);
    c.block = b;
    it->second = b;
  }
  for (Check &c : region.checks) {
    auto *i = dynamic_cast<Instruction *>(c.value);
    if (!i) continue;
    c.after = split(i->parent, i->parent->indexOf(i) + 1);
  }
  for (Check &c : region.checks) // a later split may have moved an operation to another block
    if (auto *i = dynamic_cast<Instruction *>(c.value)) c.block = i->parent;
  region.blocks.clear(); // in the function's order, the blocks split off last
  for (auto &b : f.blocks)
    if (in.count(b.get())) region.blocks.push_back(b.get());
  CFG before(f);
  BlockCopy copy = copyBlocks(f, region.blocks, ".any");

  // The check: a value of another type continues in the copy, right after the copy of the operation.
  std::unordered_map<const BasicBlock *, std::pair<Value *, Value *>> entries; // entry -> (checked, substituted)
  Builder builder(&f);
  for (const Check &c : region.checks) {
    BasicBlock *enter = f.addBlock(c.block->name + ".subst");
    std::vector<Value *> args = c.block->terminator()->targets[0].args; // the next guarded parameter of the block
    c.block->takeTerminator();
    builder.setBlock(c.block);
    Instruction *check = builder.substCheck(c.value);
    check->imm = c.guard;
    builder.condBr(check, enter, c.after, {}, args);
    builder.setBlock(enter);
    Instruction *substituted = builder.substValue(c.value);
    substituted->imm = region.whole ? 0 : 1;
    builder.br(copy.blocks.at(c.after), args);
    entries[enter] = {c.value, substituted};
  }

  // SSA form in the copy: each original value `v` whose copy `v'` the copy still uses is defined by `v'` in the copy
  // and, where a check enters it, by `v` itself (or, for the checked value, by the substituted value; by nothing where
  // `v` does not dominate the check: no use is reached from there before the copy defines it). Only reachable blocks
  // count: the copies the checks do not lead to go once the pass is done.
  CFG now(f);
  Predecessors preds;
  for (auto &[blk, terms] : predecessorTerminators(f))
    for (Instruction *t : terms)
      if (now.reachable(t->parent)) preds[blk].push_back(t);
  std::vector<BlockParam *> placed;
  for (auto &[orig, dup] : copy.pairs) {
    if (dup->users().empty()) continue;
    SSAUpdater up(f, dup->type(), preds, placed);
    up.addAvailable(definingBlock(dup), dup);
    for (auto &[enter, e] : entries) {
      const BasicBlock *def = definingBlock(orig), *at = definingBlock(e.first);
      if (orig == e.first) up.addAvailable(enter, e.second);
      else if (before.reachable(def) && before.dominates(def, at)) up.addAvailable(enter, orig);
      else up.addAvailable(enter, undefinedValue(f, dup->type()));
    }
    std::vector<Instruction *> users = dup->users();
    std::unordered_set<Instruction *> seen;
    for (Instruction *u : users)
      if (now.reachable(u->parent) && seen.insert(u).second && u->parent != definingBlock(dup)) up.rewriteUse(u, dup);
  }
  removeTrivialParams(placed, preds);

  // Past the region, a value it made is the original's or the copy's, whichever way the code left it by.
  std::unordered_set<const BasicBlock *> inside(region.blocks.begin(), region.blocks.end());
  for (auto &[orig, dup] : copy.blocks) inside.insert(dup);
  for (auto &[enter, e] : entries) inside.insert(enter);
  std::vector<Definitions> defs;
  for (auto &[orig, dup] : copy.pairs) {
    const auto *t = dynamic_cast<const Instruction *>(dup);
    if (t && isTerminator(t->op)) continue; // no value (a check's block lost its own: `orig` is gone)
    defs.push_back({{definingBlock(orig), orig}, {definingBlock(dup), dup}});
    for (auto &[enter, e] : entries) // a checked value substituted continues in the copy too
      if (orig == e.first) defs.back().push_back({enter, e.second});
  }
  std::vector<BlockParam *> joined = joinDefinitions(f, defs, inside);
  done.insert(inside.begin(), inside.end());

  // The types each join's original side gives: those of the original values, or of the joins they come through.
  std::unordered_set<const BasicBlock *> copied;
  for (auto &[orig, dup] : copy.blocks) copied.insert(dup);
  for (auto &[enter, e] : entries) copied.insert(enter);
  CFG cfg(f);
  TypeInfo types(cfg);
  std::unordered_map<const BlockParam *, TypeSet> own;
  for (BlockParam *p : joined) own[p] = 0;
  Predecessors into = predecessorTerminators(f);
  for (bool grew = true; grew;) {
    grew = false;
    for (BlockParam *p : joined)
      for (const Instruction *t : into[p->block]) {
        if (copied.count(t->parent) || !cfg.reachable(t->parent)) continue;
        for (const Target &tg : t->targets) {
          if (tg.block != p->block) continue;
          const Value *a = tg.args[p->index];
          const BlockParam *ap = asParam(a);
          auto j = ap ? own.find(ap) : own.end();
          TypeSet at = j != own.end() ? j->second : types.of(a);
          if ((own[p] | at) != own[p]) {
            own[p] |= at;
            grew = true;
          }
        }
      }
  }
  for (auto &[p, t] : own)
    if (t != 0) joins[p] = t;
}

} // namespace

bool typedContinuations(Function &f) {
  if (isResumable(f)) return false; // its values live in the frame
  // One region at a time, the first in the control flow's order: the values that leave a region copied are joined
  // where its ways out meet, of any type there, so the checks are chosen again for the code after it, where a later
  // loop that takes one as a number gets a guard of that join's parameter. Each region adds its blocks to `done`.
  std::unordered_set<const BasicBlock *> done;
  Joins joins;
  bool changed = false;
  for (;;) {
    std::vector<Check> checks = chooseChecks(f, done, joins);
    if (checks.empty()) break;
    std::vector<Region> regions = regionsOf(f, checks, done);
    if (regions.empty()) break;
    copyRegion(f, regions.front(), done, joins);
    changed = true;
  }
  if (changed) removeUnreachableBlocks(f);
  return changed;
}

} // namespace xb::xir
