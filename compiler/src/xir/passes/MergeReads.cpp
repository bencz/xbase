// merge-reads: a read that repeats one made before it, with nothing between that could change what it reads, gives the
// value the first one gave (docs/xir.md, Passes). A read merges only where it can give nothing else, and where running
// it once instead of twice shows nowhere:
// - it cannot fail and go on: an instance variable (object.get), a table's length, or an element read whose position
//   is a number, of a base that is an array (index) or of a table (table.index): one out of bounds raises an error
//   that cannot go on, so a read that fails does not reach the second;
// - between the two, nothing writes the heap or runs code, and nothing can fail (but an operator that cannot,
//   TypeInfo::cannotFail): an error whose block substitutes a value may change what is read and go on, and an
//   operation that can fail tests the pending requests on its way on, where a collection's destructors run (also the
//   slow path of an element read that succeeds). A truth test is no barrier: its error unwinds, past what follows it
//   here (a handler is never part of the way, below), and its way on tests nothing (CodeGen::testTruth).
// An instance variable read after an assignment of it (object.set: the item itself, no method), with nothing between
// as above, gives the value assigned.
// An operator that cannot fail (TypeInfo::cannotFail) repeated on the same operands gives its first value too, up to a
// call (or other code run: a value kept across it costs more than the operator; levenshtein's j + 1, 2% more
// instructions): so do the positions of two reads (a[i % 3 + 1] twice). The reads and operators before a block are
// known in it when it is entered only from the block that dominates it, by its branch (an extended block; not a
// handler, which the unwinding of any operation enters). A repeated read or operator that a check follows
// (subst.check: of its own types, or a guard, with a mask) merges only where the first one's check let it through on
// the way to it, with no more types than the second expects: the check goes too. The block a check's failure leads to
// (subst.value, then the generic copy) knows nothing: the first read's value there is not of the guard's types.
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"
#include "xir/Builder.h"
#include "xir/Transform.h"
#include "xir/Types.h"

#include <bit>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace xb::xir {

namespace {

// What an operation computes: its op, its operands, its variable, and its instance variable's slot or its operator.
using Key = std::tuple<Op, const Value *, const Value *, const Var *, uint32_t>;

// A value a check let through: its own types (mask 0), or a guard's.
struct Passed {
  const Value *value;
  TypeSet mask;
};

struct Known {
  std::map<Key, Value *> reads;     // what a barrier forgets: a read's value, or the value assigned
  std::map<Key, Value *> operators; // what a call forgets
  std::vector<Passed> passed;
};

bool onlyNumbers(TypeSet t) { return t && (t & ~kNumberTypes) == 0; }

// Each use of a constant is a Constant of its own: the operands of keys are the first constant seen of each value
// (sameValue), found among those of its kind and payload.
class Constants {
public:
  const Value *canonical(const Value *v) {
    const Constant *c = asConstant(v);
    if (!c) return v;
    size_t bucket = std::hash<std::string>{}(c->str) ^ std::hash<int64_t>{}(c->ival) ^
                    std::hash<uint64_t>{}(std::bit_cast<uint64_t>(c->dval)) ^ (size_t)c->lit;
    std::vector<const Constant *> &same = seen_[bucket];
    for (const Constant *s : same)
      if (sameValue(s, c)) return s;
    same.push_back(c);
    return c;
  }

private:
  std::unordered_map<size_t, std::vector<const Constant *>> seen_;
};

// The read `i` is one that merges (see the top): its key, or none.
std::optional<Key> readKey(const Instruction &i, const TypeInfo &types, Constants &k) {
  switch (i.op) {
  case Op::ObjectGet: return Key{i.op, i.operand(0), nullptr, nullptr, i.index};
  case Op::TableLen: return Key{i.op, nullptr, nullptr, i.var, 0};
  case Op::TableIndex:
    if (!onlyNumbers(types.of(i.operand(0)))) return std::nullopt;
    return Key{i.op, k.canonical(i.operand(0)), nullptr, i.var, 0};
  case Op::Index:
    if (types.of(i.operand(0)) != typeBit(XB_T_ARRAY) || !onlyNumbers(types.of(i.operand(1)))) return std::nullopt;
    return Key{i.op, i.operand(0), k.canonical(i.operand(1)), nullptr, 0};
  default: return std::nullopt;
  }
}

// An operator that cannot fail: its key, or none.
std::optional<Key> operatorKey(const Instruction &i, const TypeInfo &types, Constants &k) {
  if ((i.op != Op::Binary && i.op != Op::Unary) || !types.cannotFail(i)) return std::nullopt;
  uint32_t which = i.op == Op::Binary ? (uint32_t)i.binop : (uint32_t)i.unop;
  which |= (i.byOne ? 1u << 16 : 0) | (i.grows ? 1u << 17 : 0);
  return Key{i.op, k.canonical(i.operand(0)), i.operands().size() > 1 ? k.canonical(i.operand(1)) : nullptr, nullptr,
             which};
}

// After `i`, what was read before may read another value.
bool isBarrier(const Instruction &i, const TypeInfo &types) {
  if (i.effects() & (WritesHeap | RunsCode)) return true;
  return i.mayFail() && i.op != Op::Truth && !types.cannotFail(i);
}

class Merger {
public:
  Merger(const CFG &cfg, const TypeInfo &types, Builder &b, const std::unordered_set<const BasicBlock *> &handlers)
      : cfg_(cfg), types_(types), b_(b), handlers_(handlers) {}

  bool run(BasicBlock *entry) {
    std::vector<std::pair<BasicBlock *, Known>> work{{entry, {}}};
    while (!work.empty()) {
      auto [bb, known] = std::move(work.back());
      work.pop_back();
      visit(bb, known);
      const auto &children = cfg_.children(bb);
      size_t last = children.size(); // the last child that inherits takes what is known (a chain copies nothing)
      for (size_t k = 0; k < children.size(); ++k)
        if (inherits(bb, children[k])) last = k;
      for (size_t k = 0; k < children.size(); ++k) {
        Known start;
        if (k == last) start = std::move(known);
        else if (inherits(bb, children[k])) start = known;
        if (inherits(bb, children[k])) passThrough(bb, children[k], start);
        work.push_back({children[k], std::move(start)});
      }
    }
    for (Instruction *i : gone_) eraseInstruction(i);
    return !gone_.empty();
  }

private:
  // The block's reads and operators, given what is known where it starts; what is known at its end.
  void visit(BasicBlock *bb, Known &known) {
    auto &insts = bb->insts;
    for (size_t k = 0; k < insts.size(); ++k) {
      Instruction &i = *insts[k];
      std::optional<Key> key = readKey(i, types_, constants_);
      std::map<Key, Value *> *table = &known.reads;
      if (!key) {
        key = operatorKey(i, types_, constants_);
        table = &known.operators;
      }
      if (!key) {
        if (isBarrier(i, types_)) known.reads.clear();
        if (i.effects() & RunsCode) known.operators.clear();
        if (i.op == Op::ObjectSet) // what a read of it gives now
          known.reads[Key{Op::ObjectGet, i.operand(0), nullptr, nullptr, i.index}] = i.operand(1);
        continue;
      }
      auto [it, added] = table->try_emplace(*key, &i);
      if (added) {
        if (table == &known.reads && isBarrier(i, types_)) { // an element read's slow path: the others are forgotten
          known.reads.clear();
          known.reads[*key] = &i;
        }
        continue;
      }
      Value *first = it->second;
      Instruction *check = k + 1 < insts.size() && insts[k + 1]->op == Op::SubstCheck &&
                                   insts[k + 1]->operand(0) == &i
                               ? insts[k + 1].get()
                               : nullptr;
      if (check && !letThrough(known, first, (TypeSet)check->imm)) continue; // its check stays: so does the read
      i.replaceAllUsesWith(first);
      gone_.push_back(&i);
      if (!check) continue;
      check->replaceAllUsesWith(b_.boolean(false));
      gone_.push_back(check);
      ++k;
    }
    Instruction *t = bb->terminator();
    if (const Constant *c = t && t->op == Op::CondBr ? asBool(t->operand(0)) : nullptr) { // a check folded
      Target taken = t->targets[c->logical ? 0 : 1];
      b_.jump(bb, taken.block, taken.args);
    }
  }

  // A block its dominator alone enters, by its branch, but where a check's failure leads: it starts with what that
  // block knows at its end.
  bool inherits(const BasicBlock *parent, const BasicBlock *child) const {
    if (cfg_.preds(child).size() != 1 || handlers_.count(child)) return false;
    const Instruction *t = parent->terminator();
    return !(t && t->op == Op::CondBr && asInstruction(t->operand(0), Op::SubstCheck) && t->targets[0].block == child);
  }

  // From a check's way on, the value it let through.
  static void passThrough(const BasicBlock *parent, const BasicBlock *child, Known &known) {
    const Instruction *t = parent->terminator();
    const Instruction *check = t && t->op == Op::CondBr ? asInstruction(t->operand(0), Op::SubstCheck) : nullptr;
    if (check && t->targets[1].block == child && t->targets[0].block != child)
      known.passed.push_back({check->operand(0), (TypeSet)check->imm});
  }

  // A check of `value` (of its own types: mask 0; or a guard's) that one on the way here let through: it would too.
  static bool letThrough(const Known &known, const Value *value, TypeSet mask) {
    for (const Passed &p : known.passed)
      if (p.value == value && (mask == 0 ? p.mask == 0 : p.mask != 0 && (p.mask & ~mask) == 0)) return true;
    return false;
  }

  const CFG &cfg_;
  const TypeInfo &types_;
  Builder &b_;
  const std::unordered_set<const BasicBlock *> &handlers_;
  std::vector<Instruction *> gone_;
  Constants constants_;
};

} // namespace

bool mergeReads(Function &f) {
  CFG cfg(f);
  TypeInfo types(cfg);
  Builder b(&f);
  std::unordered_set<const BasicBlock *> handlers = unwindTargets(f);
  return Merger(cfg, types, b, handlers).run(f.entry());
}

} // namespace xb::xir
