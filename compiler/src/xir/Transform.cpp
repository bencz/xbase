// Changes to the shape of functions that passes share (see Transform.h).
#include "xir/Transform.h"

#include "xir/Builder.h"

namespace xb::xir {

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

BlockCopy copyBlocks(Function &f, const std::vector<BasicBlock *> &originals, const std::string &suffix,
                     const std::function<Value *(Value *)> &outside) {
  BlockCopy c;
  for (BasicBlock *b : originals) {
    BasicBlock *nb = f.addBlock(b->name + suffix);
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
      ni->copyData(i);
      c.values[&i] = ni.get();
      c.pairs.push_back({ip.get(), ni.get()});
      c.blocks[b]->insts.push_back(std::move(ni));
    }
  auto of = [&](Value *v) {
    auto it = c.values.find(v);
    return it != c.values.end() ? it->second : outside ? outside(v) : v;
  };
  for (BasicBlock *b : originals) {
    BasicBlock *nb = c.blocks[b];
    nb->unwind = b->unwind ? (c.blocks.count(b->unwind) ? c.blocks[b->unwind] : b->unwind) : nullptr;
    for (size_t k = 0; k < b->insts.size(); ++k) {
      const Instruction &i = *b->insts[k];
      Instruction &ni = *nb->insts[k];
      for (Value *v : i.operands()) ni.addOperand(of(v));
      for (size_t t = 0; t < i.targets.size(); ++t) {
        auto in = c.blocks.find(i.targets[t].block);
        ni.targets.push_back({in == c.blocks.end() ? i.targets[t].block : in->second, {}});
        for (Value *v : i.targets[t].args) ni.addTargetArg(t, of(v));
      }
    }
  }
  return c;
}

Value *undefinedValue(Function &f, Type t) {
  Builder b(&f);
  switch (t) {
  case Type::Bool: return b.boolean(false);
  case Type::I64: return b.i64(0);
  default: return b.nil();
  }
}

static size_t countEdges(const Instruction &t, const BasicBlock *b) {
  size_t n = 0;
  for (const Target &tg : t.targets) n += tg.block == b;
  return n;
}

Value *SSAUpdater::atEnd(BasicBlock *b) {
  auto it = atEnd_.find(b);
  return it != atEnd_.end() ? it->second : atStart(b);
}

Value *SSAUpdater::atStart(BasicBlock *b) {
  auto it = reaching_.find(b);
  if (it != reaching_.end()) return it->second;
  auto in = preds_.find(b);
  if (in == preds_.end() || in->second.empty()) return reaching_[b] = undefinedValue(f_, type_);
  const std::vector<Instruction *> &terms = in->second;
  if (terms.size() == 1 && countEdges(*terms[0], b) == 1) {
    Value *v = atEnd(terms[0]->parent);
    reaching_[b] = v;
    return v;
  }
  BlockParam *p = b->addParam(type_);
  placed_.push_back(p);
  reaching_[b] = p; // before the arguments: a loop's branch back passes the parameter itself
  for (Instruction *t : terms)
    for (size_t k = 0; k < t->targets.size(); ++k)
      if (t->targets[k].block == b) t->addTargetArg(k, atEnd(t->parent));
  return p;
}

void SSAUpdater::rewriteUse(Instruction *u, Value *from) {
  Value *v = atStart(u->parent);
  if (v != from) u->replaceUses(from, v);
}

void removeTrivialParams(std::vector<BlockParam *> &placed, const Predecessors &preds) {
  for (bool changed = true; changed;) {
    changed = false;
    for (BlockParam *&p : placed) {
      if (!p) continue;
      auto in = preds.find(p->block);
      if (in == preds.end()) continue;
      Value *same = nullptr;
      bool trivial = true;
      for (Instruction *t : in->second)
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
      b->removeArgs(k, in->second);
      b->eraseParam(k);
      p = nullptr;
      changed = true;
    }
  }
}

} // namespace xb::xir
