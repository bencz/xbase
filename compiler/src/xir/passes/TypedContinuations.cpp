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
bool worthChecking(const Instruction &i, const TypeInfo &types, const LoopDepths &loops) {
  if (i.type() != Type::Item || !loops.count(i.parent) || !types.maySubstitute(i)) return false;
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

// Moves the instructions after position `k` of `b` into a new block that `b`
// jumps to, and returns it.
BasicBlock *splitAfter(Function &f, BasicBlock *b, size_t k) {
  BasicBlock *tail = f.addBlock(b->name);
  for (size_t j = k + 1; j < b->insts.size(); ++j) {
    b->insts[j]->parent = tail;
    tail->insts.push_back(std::move(b->insts[j]));
  }
  b->insts.resize(k + 1);
  Builder builder(&f);
  builder.setBlock(b);
  builder.br(tail);
  return tail;
}

// A copy of blocks, with the map from each original block and value to its
// copy; constants are shared.
struct Copy {
  std::unordered_map<const BasicBlock *, BasicBlock *> blocks;
  std::unordered_map<const Value *, Value *> values;
  std::vector<std::pair<Value *, Value *>> pairs; // (original, copy) in block order: a stable order of work

  Value *of(Value *v) const {
    auto it = values.find(v);
    return it == values.end() ? v : it->second;
  }
};

Copy copyBlocks(Function &f, const std::vector<BasicBlock *> &originals) {
  Copy c;
  for (BasicBlock *b : originals) {
    BasicBlock *nb = f.addBlock(b->name + ".any");
    c.blocks[b] = nb;
    for (auto &p : b->params) {
      c.values[p.get()] = nb->addParam(p->type());
      c.pairs.push_back({p.get(), c.values[p.get()]});
    }
  }
  for (BasicBlock *b : originals) // every instruction first: operands may come from later blocks
    for (auto &ip : b->insts) {
      const Instruction &i = *ip;
      auto ni = std::make_unique<Instruction>(i.op, i.type(), c.blocks[b]);
      ni->loc = i.loc;
      ni->unop = i.unop;
      ni->binop = i.binop;
      ni->pred = i.pred;
      ni->imm = i.imm;
      ni->var = i.var;
      ni->access = i.access;
      ni->callee = i.callee;
      ni->message = i.message;
      ni->blockFn = i.blockFn;
      ni->captures = i.captures;
      c.values[&i] = ni.get();
      c.pairs.push_back({ip.get(), ni.get()});
      c.blocks[b]->insts.push_back(std::move(ni));
    }
  for (BasicBlock *b : originals)
    for (size_t k = 0; k < b->insts.size(); ++k) {
      const Instruction &i = *b->insts[k];
      Instruction &ni = *c.blocks[b]->insts[k];
      for (Value *v : i.operands()) ni.addOperand(c.of(v));
      for (size_t t = 0; t < i.targets.size(); ++t) {
        ni.targets.push_back({c.blocks.at(i.targets[t].block), {}});
        for (Value *v : i.targets[t].args) ni.addTargetArg(t, c.of(v));
      }
    }
  return c;
}

// Rebuilds SSA form in the copy: each original value `v` whose copy `v'` the
// copy still uses is defined by `v'` in the copy and, where a checked
// operation enters it, by `v` itself (or, for the operation, by the
// substituted value). Uses outside the block defining `v'` take the value
// reaching their block, through block parameters where definitions meet.
class Rebuild {
public:
  Rebuild(Function &f, const Copy &copy, std::unordered_map<const BasicBlock *, std::pair<Value *, Value *>> entries,
          const CFG &before)
      : f_(f), copy_(copy), entries_(std::move(entries)), before_(before) {
    // Only reachable blocks count: the copy of the function's start, and the
    // copies the checks do not lead to, go once this is done.
    CFG now(f);
    for (auto &b : f.blocks)
      if (now.reachable(b.get())) live_.insert(b.get());
    for (auto &[b, terms] : predecessorTerminators(f))
      for (Instruction *t : terms)
        if (live_.count(t->parent)) preds_[b].push_back(t);
  }

  void run() {
    for (auto &[orig, dup] : copy_.pairs) {
      if (dup->users().empty()) continue;
      original_ = orig;
      copied_ = dup;
      reaching_.clear();
      std::vector<Instruction *> users = dup->users();
      std::unordered_set<Instruction *> done;
      for (Instruction *u : users) {
        if (!live_.count(u->parent) || !done.insert(u).second || u->parent == definingBlock(dup)) continue;
        Value *v = atStart(u->parent);
        if (v != dup) u->replaceUses(dup, v);
      }
    }
    removeTrivialParams();
  }

private:
  // The value at the start of `b`, which does not define the copy.
  Value *atStart(BasicBlock *b) {
    auto it = reaching_.find(b);
    if (it != reaching_.end()) return it->second;
    const std::vector<Instruction *> &in = preds_[b];
    if (in.empty()) return undefined(); // not reached: the copy is entered only after checked operations
    if (in.size() == 1 && countEdges(*in[0], b) == 1) {
      Value *v = atEnd(in[0]->parent);
      reaching_[b] = v;
      return v;
    }
    BlockParam *p = b->addParam(copied_->type());
    placed_.push_back(p);
    reaching_[b] = p;
    for (Instruction *t : in)
      for (size_t k = 0; k < t->targets.size(); ++k)
        if (t->targets[k].block == b) t->addTargetArg(k, atEnd(t->parent));
    return p;
  }

  Value *atEnd(BasicBlock *b) {
    if (b == definingBlock(copied_)) return copied_;
    auto e = entries_.find(b);
    if (e != entries_.end()) { // where a checked operation (e->second.first) enters the copy
      if (original_ == e->second.first) return e->second.second;
      const BasicBlock *def = definingBlock(original_);
      const BasicBlock *checked = static_cast<const Instruction *>(e->second.first)->parent;
      if (before_.reachable(def) && before_.dominates(def, checked)) return original_;
      return undefined(); // no use is reached from here before the copy defines it
    }
    return atStart(b);
  }

  static size_t countEdges(const Instruction &t, const BasicBlock *b) {
    size_t n = 0;
    for (const Target &tg : t.targets) n += tg.block == b;
    return n;
  }

  Value *undefined() {
    Builder b(&f_);
    switch (copied_->type()) {
    case Type::Bool: return b.boolean(false);
    case Type::I64: return b.i64(0);
    default: return b.nil();
    }
  }

  // A parameter whose arguments are all one other value (or itself) is that
  // value; removing one may make others so.
  void removeTrivialParams() {
    for (bool changed = true; changed;) {
      changed = false;
      for (BlockParam *&p : placed_) { // from reachable predecessors: the others go afterwards
        if (!p) continue;
        Value *same = nullptr;
        bool trivial = true;
        for (Instruction *t : preds_[p->block])
          for (const Target &tg : t->targets) {
            if (tg.block != p->block) continue;
            Value *a = tg.args[p->index];
            if (a == p || (same && sameValue(a, same))) continue;
            if (same) trivial = false;
            same = a;
          }
        if (!trivial || !same) continue;
        BasicBlock *b = p->block;
        size_t k = p->index;
        p->replaceAllUsesWith(same);
        b->removeArgs(k, preds_[b]);
        b->eraseParam(k);
        p = nullptr;
        changed = true;
      }
    }
  }

  Function &f_;
  const Copy &copy_;
  std::unordered_map<const BasicBlock *, std::pair<Value *, Value *>> entries_; // entry block -> (checked, substituted)
  const CFG &before_;  // the function before the copy: dominance of the original values
  std::unordered_set<const BasicBlock *> live_;
  Predecessors preds_; // from reachable blocks
  Value *original_ = nullptr;
  Value *copied_ = nullptr;
  std::unordered_map<const BasicBlock *, Value *> reaching_;
  std::vector<BlockParam *> placed_;
};

} // namespace

bool typedContinuations(Function &f) {
  size_t count = 0;
  for (auto &b : f.blocks) {
    if (b->unwind) return false; // unwind blocks take no parameters: the copy could not be joined there
    count += b->insts.size();
  }
  if (count > kMaxInstructions) return false;

  std::vector<Instruction *> checked;
  {
    CFG cfg(f);
    TypeInfo types(cfg, Substitution::None);
    LoopDepths loops = loopDepths(cfg);
    for (BasicBlock *b : cfg.order())
      for (auto &i : b->insts)
        if (worthChecking(*i, types, loops)) checked.push_back(i.get());
    std::stable_sort(checked.begin(), checked.end(), [&](const Instruction *a, const Instruction *b) {
      return loops.at(a->parent) > loops.at(b->parent);
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
  Copy copy = copyBlocks(f, originals);

  // The check: a value of another type continues in the copy, right after the
  // copy of the operation.
  std::unordered_map<const BasicBlock *, std::pair<Value *, Value *>> entries;
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
  Rebuild(f, copy, std::move(entries), before).run();
  removeUnreachableBlocks(f);
  return true;
}

} // namespace xb::xir
