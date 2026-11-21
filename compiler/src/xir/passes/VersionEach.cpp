// Versions of FOR EACH loops by the type of their collection: a loop whose collection may be a string, an array or a
// hash, among other things, is copied, and a test of the collection's type before it (a guard: `subst.check` with a
// mask, Types.h) enters the copy made for its type, where the collection's type is known. Code
// generation then takes each step's element, the variable's reads and the length test there in place, without testing
// the loop reference's collection at every pass; the last copy keeps the loop for any collection.
//
// A loop is versioned when its collection (`each.source`) is used only from its each.enter on, and its blocks, with the
// cleanup blocks only they unwind to (the restore of its variables), are few enough. The first type keeps the original
// blocks; its guard narrows the collection itself, and each next type's guard narrows the value that goes on
// (`subst.value`), copied for that type, and again for the rest. Values of the loop used after it, and the collection
// where the copies join again, are joined from the copies (joinDefinitions, Transform.h).
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"
#include "xir/Builder.h"
#include "xir/Transform.h"
#include "xir/Types.h"

#include <algorithm>
#include <string>
#include <unordered_set>

namespace xb::xir {

namespace {

constexpr size_t kMaxFunction = 4000; // instructions: the copies stay bounded
constexpr size_t kMaxRegion = 250;    // instructions of a loop copied, once per version
constexpr unsigned kMaxVersioned = 8; // loops versioned in a function
constexpr xb_type kVersions[] = {XB_T_STRING, XB_T_ARRAY, XB_T_HASH};

bool usesAny(const Instruction &i, const std::unordered_set<const Value *> &defs) {
  for (const Value *v : i.operands())
    if (defs.count(v)) return true;
  for (const Target &t : i.targets)
    for (const Value *v : t.args)
      if (defs.count(v)) return true;
  return false;
}

// The blocks of the loop that block `from` enters from its each.enter at position `at` on (through blocks only it
// enters: the cleanup's start, with a DESCEND loop's lengths), the blocks its checks lead to (typed continuations: each
// holds the check's subst.value), and the unwind blocks that only they reach and that use their values or those of
// `from` from position `at` on (a loop's restore of its variables): false when the loop's blocks unwind to a block that
// uses them and others reach.
bool loopRegion(const Function &f, const CFG &cfg, const Loops &loops, BasicBlock *from, size_t at,
                std::vector<BasicBlock *> &region) {
  const Loop *loop = nullptr;
  const BasicBlock *entry = from;
  for (unsigned n = 0; n < 2 && !loop; ++n) {
    Instruction *t = from->terminator();
    if (!t || t->op != Op::Br || (from != entry && cfg.preds(from).size() != 1)) return false;
    from = t->targets[0].block;
    const Loop *l = loops.innermost(from);
    if (l && l->header == from) loop = l;
    else region.push_back(from);
  }
  if (!loop || loop->contains(entry)) return false;
  region.insert(region.end(), loop->blocks.begin(), loop->blocks.end());
  std::unordered_set<const BasicBlock *> in(region.begin(), region.end());
  for (size_t k = 0; k < region.size(); ++k) // a check's own block, outside the loop: its subst.value is the check's
    if (Instruction *t = region[k]->terminator(); t && t->op == Op::CondBr &&
                                                  asInstruction(t->operand(0), Op::SubstCheck) &&
                                                  in.insert(t->targets[0].block).second)
      region.push_back(t->targets[0].block);
  for (size_t k = 0; k < region.size(); ++k) {
    BasicBlock *u = region[k]->unwind;
    if (!u || in.count(u)) continue;
    std::unordered_set<const Value *> defs; // the region's, and those of `entry` from the each.enter on (its split)
    for (size_t j = at; j < entry->insts.size(); ++j) defs.insert(entry->insts[j].get());
    for (const BasicBlock *b : region)
      for (auto &i : b->insts) defs.insert(i.get());
    bool uses = false;
    for (auto &i : u->insts) uses |= usesAny(*i, defs);
    if (!uses) continue;
    for (auto &b : f.blocks) { // only the region unwinds or branches there
      if (in.count(b.get())) continue;
      if (b->unwind == u) return false;
      if (Instruction *t = b->terminator())
        for (const Target &tg : t->targets)
          if (tg.block == u) return false;
    }
    region.push_back(u);
    in.insert(u);
  }
  return true;
}

// Versions the loop of `enter` (position `k` of its block) for the types in `types`, the loop's blocks in `region`.
void version(Function &f, Instruction *enter, size_t k, std::vector<BasicBlock *> region,
             const std::vector<xb_type> &types, std::unordered_set<const Instruction *> &done) {
  BasicBlock *b = enter->parent;
  Value *coll = enter->operand(1);
  BasicBlock *start = splitAfter(f, b, k - 1);
  region.insert(region.begin(), start);
  std::unordered_set<const BasicBlock *> inside(region.begin(), region.end());

  // The guards: the first type goes on into the loop; the others into their copies, the rest into the last copy.
  Builder builder(&f);
  b->takeTerminator();
  builder.setBlock(b);
  Value *tested = coll;
  BasicBlock *into = start;
  std::vector<BlockCopy> copies;
  Definitions collDefs = {{b, coll}};
  for (size_t n = 0; n <= types.size(); ++n) {
    if (n > 0) { // the copy for this version, entered from the guard before
      Value *narrowed = tested;
      copies.push_back(copyBlocks(f, region, n < types.size() ? ".each" + std::to_string(n) : ".each",
                                  [&](Value *v) { return v == coll ? narrowed : v; }));
      into = copies.back().blocks.at(start);
      for (auto &[orig, dup] : copies.back().blocks) inside.insert(dup);
      for (auto &[orig, dup] : copies.back().values)
        if (auto *i = dynamic_cast<Instruction *>(dup); i && i->op == Op::EachEnter) done.insert(i);
    }
    if (n == types.size()) {
      builder.br(into);
      break;
    }
    BasicBlock *other = f.addBlock(start->name + ".other");
    inside.insert(other);
    Instruction *check = builder.substCheck(tested);
    check->imm = typeBit(types[n]);
    builder.condBr(check, other, into);
    builder.setBlock(other);
    tested = builder.substValue(tested);
    collDefs.push_back({other, tested});
  }
  // Values of the loop used after it, from whichever version left it, and the collection where versions join.
  std::vector<Definitions> defs;
  inside.insert(b);
  for (BasicBlock *r : region) {
    auto add = [&](Value *v) {
      Definitions d = {{definingBlock(v), v}};
      for (const BlockCopy &c : copies) {
        Value *dup = c.values.at(v);
        d.push_back({definingBlock(dup), dup});
      }
      defs.push_back(std::move(d));
    };
    for (auto &p : r->params) add(p.get());
    for (auto &i : r->insts) add(i.get());
  }
  defs.push_back(collDefs);
  joinDefinitions(f, defs, inside);
}

} // namespace

bool versionEachLoops(Function &f) {
  if (isResumable(f)) return false; // its values live in the frame (and a copied YIELD would resume twice)
  bool changed = false;
  std::unordered_set<const Instruction *> done;
  for (unsigned n = 0; n < kMaxVersioned; ++n) {
    if (instructionCount(f) > kMaxFunction) break;
    CFG cfg(f);
    TypeInfo types(cfg);
    Loops loops(cfg);
    std::vector<std::pair<BasicBlock *, size_t>> candidates; // innermost loops first: they gain the most
    for (BasicBlock *b : cfg.order())
      for (size_t k = 1; k < b->insts.size(); ++k)
        if (b->insts[k]->op == Op::EachEnter && !done.count(b->insts[k].get())) candidates.push_back({b, k});
    std::stable_sort(candidates.begin(), candidates.end(), [&](const auto &x, const auto &y) {
      return loops.depth(x.first) > loops.depth(y.first);
    });
    bool found = false;
    for (auto [b, k] : candidates) {
      Instruction *enter = b->insts[k].get();
      done.insert(enter);
      Value *coll = enter->operand(1);
      TypeSet t = types.of(coll);
      if ((t & (t - 1)) == 0) continue; // one type: nothing to test
      std::vector<xb_type> versions;
      for (xb_type v : kVersions)
        if (t & typeBit(v)) versions.push_back(v);
      if (versions.empty()) continue;
      if (b->insts[k - 1].get() != coll) continue; // the guard follows the operation it checks (Verifier.cpp)
      bool usedBefore = false; // a use of the collection before the loop: the guard would narrow it there too
      for (const Instruction *u : coll->users()) {
        if (u->parent != b) {
          usedBefore |= !cfg.dominates(b, u->parent);
          continue;
        }
        for (size_t j = 0; j < k; ++j) usedBefore |= b->insts[j].get() == u;
      }
      std::vector<BasicBlock *> region;
      if (usedBefore || !loopRegion(f, cfg, loops, b, k, region)) continue;
      if (instructionCount(region) + (b->insts.size() - k) > kMaxRegion) continue;
      version(f, enter, k, std::move(region), versions, done);
      found = changed = true;
      break;
    }
    if (!found) break;
  }
  return changed;
}

} // namespace xb::xir
