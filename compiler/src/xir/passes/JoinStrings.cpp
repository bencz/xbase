// `a + b + c + ...`: a chain of three pieces or more, each sum used only as the left operand of the next `+`. When its
// pieces may all be strings and one of them is one for sure, each `+` becomes a join step in its own place
// (xb_op_join): the pieces are computed, and a `+` that fails fails, in the order they were; while the pieces so far
// are strings a step makes no string, its result the pending mark, and the last step joins them all into one string,
// so the chain allocates once instead of once per `+` (once per kMaxJoinPieces pieces of a longer one). A piece that
// is no string turns its step, and every step after it, into the `+` itself, of the sum so far. A chain an append
// appends is left to split-appends, which appends its pieces without making a sum.
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"
#include "xir/Transform.h"
#include "xir/Types.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>

namespace xb::xir {

namespace {

// A `+` that a chain may hold: no `++` (by one) nor an append's `+` (grows).
Instruction *plainAdd(Value *v) {
  if (!asInstruction(v, Op::Binary)) return nullptr;
  auto *i = static_cast<Instruction *>(v);
  return i->binop == BinOp::Add && !i->byOne && !i->grows ? i : nullptr;
}

// A sum the next `+` of its chain takes as its left operand, and nothing else uses.
bool inner(const Instruction *sum) {
  if (!sum->hasOneUse()) return false;
  Instruction *u = sum->users().front();
  return plainAdd(u) && u->operand(0) == sum && u->operand(1) != sum;
}

struct Chain {
  std::vector<Instruction *> sums; // in order: sums[k] adds pieces[k + 1]
  std::vector<Value *> pieces;
};

} // namespace

bool joinStrings(Function &f) {
  CFG cfg(f);
  TypeInfo types(cfg);
  const TypeSet string = typeBit(XB_T_STRING);
  std::vector<Chain> chains;
  for (auto &b : f.blocks)
    for (auto &i : b->insts) {
      Instruction *last = plainAdd(i.get());
      if (!last || inner(last)) continue;
      Chain c;
      for (Instruction *s = last;;) {
        c.sums.push_back(s);
        c.pieces.push_back(s->operand(1));
        Instruction *left = plainAdd(s->operand(0));
        if (!left || !inner(left)) break;
        s = left;
      }
      c.pieces.push_back(c.sums.back()->operand(0));
      if (c.pieces.size() < 3) continue;
      std::reverse(c.sums.begin(), c.sums.end());
      std::reverse(c.pieces.begin(), c.pieces.end());
      bool mayAll = std::all_of(c.pieces.begin(), c.pieces.end(), [&](const Value *p) { return types.of(p) & string; });
      bool one = std::any_of(c.pieces.begin(), c.pieces.end(), [&](const Value *p) { return types.of(p) == string; });
      bool appended = std::any_of(last->users().begin(), last->users().end(),
                                  [](const Instruction *u) { return u->op == Op::Append; });
      if (mayAll && one && !appended) chains.push_back(std::move(c));
    }
  // A piece may be the sum of a chain joined before (an inlined body's result, say): its join stands for it.
  std::unordered_map<const Value *, Value *> joined;
  auto now = [&](Value *v) {
    auto it = joined.find(v);
    return it == joined.end() ? v : it->second;
  };
  // A chain past kMaxJoinPieces pieces is joined in parts, each part's sum the first piece of the next (a last part of
  // two pieces stays a `+`).
  for (Chain &c : chains)
    for (size_t from = 0; from + 1 < c.sums.size(); from += kMaxJoinPieces - 1) {
      size_t to = std::min(c.sums.size(), from + kMaxJoinPieces - 1);
      std::vector<Value *> pieces{from ? c.sums[from - 1] : c.pieces[0]};
      pieces.insert(pieces.end(), c.pieces.begin() + (long)from + 1, c.pieces.begin() + (long)to + 1);
      Instruction *step = nullptr;
      for (size_t k = from; k < to; ++k) {
        Instruction *sum = c.sums[k];
        bool end = k + 1 == to;
        auto join = std::make_unique<Instruction>(end ? Op::JoinEnd : Op::JoinStep, Type::Item, sum->parent);
        join->loc = sum->loc;
        if (step) join->addOperand(step);
        for (size_t p = 0; p <= k - from + 1; ++p) join->addOperand(now(pieces[p]));
        Instruction *made = join.get();
        auto &insts = sum->parent->insts;
        auto at = std::find_if(insts.begin(), insts.end(), [&](const auto &x) { return x.get() == sum; });
        insts.insert(at, std::move(join));
        sum->replaceAllUsesWith(made);
        joined[sum] = made;
        step = made;
      }
      for (size_t k = from; k < to; ++k) eraseInstruction(c.sums[k]);
    }
  return !chains.empty();
}

} // namespace xb::xir
