// Frame layout of an xIR function: homes of item values, liveness, and slot
// assignment by linear scan (see FramePlan.h).
#include "codegen/FramePlan.h"

#include <algorithm>

namespace xb {

using namespace xir;

namespace {

bool isItemResult(const Value *v) { return v->kind() == Value::Kind::Instruction && v->type() == Type::Item; }

// Operations whose operands go in consecutive slots (a call to a direct entry passes words in registers).
bool isGroupUser(const Instruction &i) {
  return (i.op == Op::Call && !isDirectCall(i)) || i.op == Op::Send || i.op == Op::SendMacro || i.op == Op::ArrayLit ||
         i.op == Op::HashLit || i.op == Op::MacroCall || i.op == Op::GenNew || i.op == Op::AgenNew ||
         i.op == Op::AsyncNew || (i.op == Op::InlineEnter && i.blockFn->readsArgs); // else it holds them in place
}

} // namespace

bool returnsWords(const Function &f) {
  TypeSet r = f.returnTypes & kAnyType;
  return hasDirectEntry(f) && (r & kHeapTypes) == 0 && (r & ~typeBit(XB_T_NIL)) != 0;
}

FramePlan::FramePlan(const Function &f) : f_(f), cfg_(f), types_(cfg_) {
  findUnwritten();
  findEachLoads();
  findRegisterParams();
  placeVariables();
  number();
  chooseHomes();
  pinMovedParams();
  computeLiveness();
  extendArgumentGroups();
  findBodyUnits();
  allocate();
  placeClears();
  placeLoopClears();
}

// The parameters of a typed copy (Function::paramTypes) with a direct entry, which its calls pass in registers, that
// hold no heap reference and that nothing writes: nothing reads them in the frame (a typed copy reads no arguments as
// received and forwards none), and the collector need not see them. Only in a function without loops, whose calls
// are short, so that writing them into the frame weighs: a value held through a loop keeps a register from it.
void FramePlan::findRegisterParams() {
  params_.assign(f_.numParams, nullptr);
  for (auto &v : f_.vars)
    if (isParameter(v.get()) && v->index < f_.numParams) params_[v->index] = v.get();
  if (f_.paramTypes.empty() || !hasDirectEntry(f_) || !Loops(cfg_).all().empty()) return;
  for (const Var *v : params_)
    if (v && !mayHoldHeap(*v) && unwritten_.count(v)) registerParams_.insert(v);
}

// A parameter of a typed copy whose types have no heap reference cannot hold one; any other may.
bool FramePlan::mayHoldHeap(const Var &param) const {
  return param.index >= f_.paramTypes.size() || (f_.paramTypes[param.index] & kHeapTypes) != 0;
}

// Parameters keep their slots, in order (but those in registers); the other frame variables something still loads,
// stores or captures follow, in declaration order.
void FramePlan::placeVariables() {
  std::unordered_set<const Var *> used;
  for (auto &b : f_.blocks)
    for (auto &i : b->insts) {
      if (i->var) used.insert(i->var);
      used.insert(i->captures.begin(), i->captures.end());
    }
  for (const Var *p : params_)
    if (p && !registerParams_.count(p)) varSlot_[p] = paramSlots_++;
  numVarSlots_ = paramSlots_;
  for (auto &v : f_.vars)
    if (v->kind == Var::Kind::Frame && !varSlot_.count(v.get()) && !registerParams_.count(v.get()) &&
        (v->param || used.count(v.get())))
      varSlot_[v.get()] = numVarSlots_++;
}

// The frame variables nothing changes once the function runs (a parameter the
// body versioned for references leaves to the other body, a LOCAL never
// assigned): not shared, reached directly only, never written.
void FramePlan::findUnwritten() {
  for (auto &v : f_.vars)
    if (v->kind == Var::Kind::Frame && !v->shared) unwritten_.insert(v.get());
  for (auto &b : f_.blocks)
    for (auto &i : b->insts) {
      bool writes = (i->effects() & WritesVar) != 0;
      if (i->var && (writes || i->access != Access::Direct)) unwritten_.erase(i->var);
      if (writes)
        for (const Var *c : i->captures) unwritten_.erase(c);
    }
}

void FramePlan::number() {
  uint32_t p = 0;
  for (BasicBlock *b : cfg_.order()) {
    blockIndex_[b] = (uint32_t)blockIndex_.size();
    start_[b] = p++;
    for (auto &param : b->params) pos_[param.get()] = start_[b];
    for (auto &i : b->insts) pos_[i.get()] = p++;
    end_[b] = p - 1;
  }
}

// A load whose single use comes later in the same block, with nothing in
// between that can write the variable, is served by the variable's own item.
// A plain frame variable changes only by its own stores; one in a cell, a
// STATIC, or one reached through a reference can also change through anything
// that writes a variable or the heap or runs code (see Effect in Ops.h), which
// includes an operation whose error may substitute its result: the code goes
// on after the error block ran.
// One pass per block: the position of the last write of each variable and of
// the last instruction that may write any variable indirectly.
void FramePlan::elideLoads(const BasicBlock &b) {
  std::unordered_map<const Var *, uint32_t> lastWrite;
  uint32_t lastIndirect = 0;
  for (auto &ip : b.insts) {
    const Instruction &u = *ip;
    if (!isGroupUser(u) && u.op != Op::Br && u.op != Op::CondBr && u.op != Op::InlineEnter) { // held: own slots
      for (const Value *v : u.operands()) {
        if (v->kind() != Value::Kind::Instruction || !v->hasOneUse()) continue;
        auto &load = static_cast<const Instruction &>(*v);
        if (load.op != Op::Load || load.parent != &b) continue;
        uint32_t at = pos_.at(&load);
        auto w = lastWrite.find(load.var);
        bool written = w != lastWrite.end() && w->second > at;
        if (!isPlain(*load.var, load.access)) written |= lastIndirect > at;
        if (written) continue;
        homes_[&load] = {Home::Kind::Place, 0, &load};
        if (load.access != Access::Direct) shadowed_.insert(&load);
      }
    }
    uint32_t p = pos_.at(&u);
    uint16_t e = u.effects();
    if (e & WritesVar) {
      if (u.var) lastWrite[u.var] = p;
      for (const Var *c : u.captures) lastWrite[c] = p;
    }
    // An operation that may fail runs the error block, the program's code (also a load through a reference).
    if ((e & (WritesVar | WritesHeap | RunsCode | Substitutes)) || u.mayFail()) lastIndirect = p;
  }
}

// A value computed into the variable of the store right after it: only into
// the variable's own item (a store through a cell or a reference may find a
// shared cell or an element, which the runtime writes whole). A call may only
// write into a plain frame variable: the callee must not see the variable
// cleared (a STATIC it may read). In a block
// that unwinds (to a BEGIN SEQUENCE handler, which may read the variable, or a
// FOR EACH cleanup), an operation that can fail is not computed in place: a
// call may clear its result first.
bool FramePlan::canForward(const Instruction &value, const Instruction &store) const {
  if (store.op != Op::Store || store.operand(0) != &value || !value.hasOneUse()) return false;
  if (store.access != Access::Direct) return false;
  if (value.mayFail() && store.parent->unwind) return false;
  switch (value.op) {
  case Op::Unary: case Op::Binary: case Op::Index: case Op::ArrayLit: case Op::MakeBlock: case Op::NumDefaultDec:
  case Op::FromBool: case Op::ObjectGet: case Op::ObjectSet:
    return true;
  case Op::Call: case Op::Send: case Op::SendMacro:
    return isPlain(*store.var, store.access);
  default:
    return false;
  }
}

uint32_t FramePlan::unitOf(const Value *v) {
  auto it = unit_.find(v);
  if (it != unit_.end()) return it->second;
  units_.push_back({});
  unit_[v] = (uint32_t)units_.size() - 1;
  return unit_[v];
}

void FramePlan::chooseHomes() {
  for (BasicBlock *b : cfg_.order()) {
    elideLoads(*b);
    for (auto &i : b->insts) // a variable nothing writes serves every use of a load of it
      if (i->op == Op::Load && i->access == Access::Direct && unwritten_.count(i->var) && !homes_.count(i.get()))
        homes_[i.get()] = {Home::Kind::Place, 0, i.get()};
    for (size_t k = 0; k < b->insts.size(); ++k) {
      const Instruction &i = *b->insts[k];
      if (i.op == Op::Store && k > 0 && canForward(*b->insts[k - 1], i)) {
        homes_[b->insts[k - 1].get()] = {Home::Kind::Place, 0, &i};
        forwardedStores_.insert(&i);
      }
      // A body that returns its result's words in registers has no caller's item to compute it into.
      if (i.op == Op::Ret && i.operands().size() == 1 && !returnsWords(f_)) {
        const Value *v = i.operand(0);
        if (isItemResult(v) && v->hasOneUse() && static_cast<const Instruction *>(v)->parent == b &&
            static_cast<const Instruction *>(v)->op != Op::Load)
          homes_[v] = {Home::Kind::Ret, 0, nullptr};
      }
    }
  }
  // Values the collector need not see live outside the frame (Local): block
  // parameters first, so that their arguments do not share a slot with them;
  // other values once groups and parameters have taken those computed in
  // their slots, which saves a copy.
  for (BasicBlock *b : cfg_.order())
    for (auto &p : b->params)
      if (p->type() == Type::Item && types_.noHeap(p.get())) homes_[p.get()] = {Home::Kind::Local, 0, nullptr};
  // Groups: the operands of a call or array literal in consecutive slots.
  for (BasicBlock *b : cfg_.order()) {
    for (auto &ip : b->insts) {
      const Instruction &i = *ip;
      if (!isGroupUser(i) || i.operands().empty()) continue;
      units_.push_back({});
      uint32_t u = (uint32_t)units_.size() - 1;
      units_[u].size = (uint32_t)i.operands().size();
      groupUnit_[&i] = u;
      for (size_t k = 0; k < i.operands().size(); ++k) {
        const Value *v = i.operand(k);
        if (v->type() != Type::Item || v->kind() == Value::Kind::Constant || !v->hasOneUse() || homes_.count(v) ||
            unit_.count(v))
          continue;
        const BasicBlock *defBlock = definingBlock(v);
        if (defBlock != b || pos_.at(v) >= pos_.at(&i)) continue;
        groupMember_[v] = (uint32_t)k; // a block parameter here takes its arguments into the group slot too
        unit_[v] = u;
      }
    }
  }
  // Branch arguments used only there share their parameter's slot, when the
  // parameter is not used after the argument is computed. On a forward edge
  // the parameter is not live yet; on a back edge (a loop carrying a value)
  // the jump must be the block's only way out, and nothing in the block may
  // read the parameter after the argument is computed, the jump included (the
  // argument may be computed from the parameter: results may alias operands).
  // Two edges to one block carry two arguments live at once: neither shares.
  auto readAfter = [&](const BlockParam *p, const Value *v, const BasicBlock *b) {
    for (const Instruction *u : p->users())
      if (u->parent == b && pos_.at(u) > pos_.at(v)) return true;
    return false;
  };
  // A value an inlined body holds (an inline.enter's operand, the argument of a parameter) keeps a slot of its own: a
  // block parameter sharing it would take it over while the body runs.
  std::unordered_set<const Value *> heldValues;
  for (BasicBlock *b : cfg_.order())
    for (auto &i : b->insts)
      if (i->op == Op::InlineEnter && !isGroupUser(*i)) heldValues.insert(i->operands().begin(), i->operands().end());
  for (BasicBlock *b : cfg_.order()) {
    const Instruction *t = b->terminator();
    for (const Target &tg : t->targets) {
      if (std::count_if(t->targets.begin(), t->targets.end(), [&](const Target &o) { return o.block == tg.block; }) > 1)
        continue;
      for (size_t k = 0; k < tg.args.size(); ++k) {
        const Value *v = tg.args[k];
        const BlockParam *p = tg.block->params[k].get();
        if (p->type() != Type::Item || homes_.count(p) || !isItemResult(v) || !v->hasOneUse() || homes_.count(v) ||
            unit_.count(v) || heldValues.count(p) || heldValues.count(v))
          continue;
        if (static_cast<const Instruction *>(v)->parent != b) continue;
        if (cfg_.dominates(tg.block, b) && (t->op != Op::Br || groupMember_.count(p) || readAfter(p, v, b))) continue;
        unit_[v] = unitOf(p);
        auto m = groupMember_.find(p);
        if (m != groupMember_.end()) groupMember_[v] = m->second;
      }
    }
  }
  for (BasicBlock *b : cfg_.order())
    for (auto &i : b->insts)
      if (isItemResult(i.get()) && !homes_.count(i.get()) && !unit_.count(i.get()) && types_.noHeap(i.get()) &&
          !types_.guardNeedsHome(*i))
        homes_[i.get()] = {Home::Kind::Local, 0, nullptr};
  // Every other item value gets a slot of its own.
  for (BasicBlock *b : cfg_.order()) {
    for (auto &p : b->params)
      if (p->type() == Type::Item && !homes_.count(p.get())) unitOf(p.get());
    for (auto &i : b->insts)
      if ((isItemResult(i.get()) && !homes_.count(i.get())) || shadowed_.count(i.get())) unitOf(i.get());
  }
}

// Live intervals over the linear order: a value lives from its definition to
// its last use, extended to the end of every block it is live out of (loops).
// Liveness by path exploration: from each use, walk the predecessors back to
// the defining block; each value costs only the blocks it is live in.
void FramePlan::computeLiveness() {
  const auto &order = cfg_.order();
  std::vector<uint32_t> liveIn(order.size(), 0); // stamp: the value whose walk marked the block
  uint32_t stamp = 0;
  std::vector<const BasicBlock *> work;
  std::vector<const Instruction *> releases;
  for (const BasicBlock *b : order)
    for (auto &i : b->insts)
      if (i->op == Op::FrameRelease) releases.push_back(i.get());
  for (auto &[v, u] : unit_) {
    ++stamp;
    const BasicBlock *def = definingBlock(v);
    uint32_t start = pos_.at(v), end = start;
    auto useIn = [&](const BasicBlock *b, uint32_t p) {
      end = std::max(end, p);
      if (b == def) return;
      uint32_t bi = blockIndex_.at(b);
      if (liveIn[bi] == stamp) return;
      liveIn[bi] = stamp;
      work.push_back(b);
    };
    auto readers = sourceReaders_.find(v);
    auto forUsers = [&](auto &&fn) {
      for (const Instruction *user : v->users()) fn(user);
      if (readers != sourceReaders_.end())
        for (const Instruction *user : readers->second) fn(user);
    };
    forUsers([&](const Instruction *user) { useIn(user->parent, pos_.at(user)); });
    while (!work.empty()) {
      const BasicBlock *b = work.back();
      work.pop_back();
      for (const BasicBlock *p : cfg_.preds(b)) {
        end = std::max(end, end_.at(p)); // live out of the predecessor
        if (p == def) continue;
        uint32_t pi = blockIndex_.at(p);
        if (liveIn[pi] == stamp) continue;
        liveIn[pi] = stamp;
        work.push_back(p);
      }
    }
    Unit &unit = units_[u];
    unit.start = std::min(unit.start, start);
    unit.end = std::max(unit.end, end);
    // Live at a frame.release: not defined after it in its block, and used after it there or live into a successor.
    for (const Instruction *r : releases) {
      const BasicBlock *b = r->parent;
      uint32_t at = pos_.at(r);
      if (def == b && start > at) continue;
      bool live = false;
      forUsers([&](const Instruction *user) { live |= user->parent == b && pos_.at(user) > at; });
      for (size_t k = 0; !live && k < b->numSuccessors(); ++k) // a block with a use is live-in (but the def's)
        live = liveIn[blockIndex_.at(b->successor(k))] == stamp;
      if (live) liveAtRelease_[r].insert(u);
    }
  }
  for (auto &[user, u] : groupUnit_) {
    uint32_t p = pos_.at(user);
    units_[u].start = std::min(units_[u].start, p);
    units_[u].end = std::max(units_[u].end, p);
  }
}

// Linear scan: units in order of start; a unit's slots are free again once
// the unit has ended before the next one starts. Slots shared on purpose (a
// value computed into its loop parameter's slot) are one unit already;
// otherwise a result never shares a slot with an operand of the same
// instruction (calls clear their result first).
// Shadow slots come last, in a run of their own (none is live across a call or a yield), after the temporaries.
void FramePlan::allocate() {
  std::unordered_set<uint32_t> shadowUnits;
  for (auto &[v, u] : unit_)
    if (v->kind() == Value::Kind::Instruction && shadowed_.count(static_cast<const Instruction *>(v)))
      shadowUnits.insert(u);
  std::vector<uint32_t> temps, shadows;
  for (uint32_t k = 0; k < units_.size(); ++k)
    if (!units_[k].pinned) (shadowUnits.count(k) ? shadows : temps).push_back(k);
  // A resumable function's values that live across a suspension are in its variables (Resumable.cpp): a step saves
  // those slots alone, its temporaries dead there.
  uint32_t tempSlots = scan(temps, numVarSlots_);
  savedSize_ = xir::isResumable(f_) ? numVarSlots_ : numVarSlots_ + tempSlots;
  frameSize_ = numVarSlots_ + tempSlots;
  frameSize_ += scan(shadows, frameSize_);
  for (auto &[user, u] : groupUnit_) groupBase_[user] = units_[u].slot;
  for (auto &[v, u] : unit_) {
    if (units_[u].pinned) continue; // served by its parameter's slot (a Place)
    if (shadowUnits.count(u)) {
      shadowSlot_[static_cast<const Instruction *>(v)] = units_[u].slot;
      continue;
    }
    auto m = groupMember_.find(v);
    homes_[v] = {Home::Kind::Slot, units_[u].slot + (m == groupMember_.end() ? 0 : m->second), nullptr};
  }
}

// Linear scan of `order`'s units from slot `base`; returns the slots taken.
uint32_t FramePlan::scan(std::vector<uint32_t> order, uint32_t base) {
  std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
    return units_[a].start != units_[b].start ? units_[a].start < units_[b].start : a < b;
  });
  std::vector<bool> busy; // from slot `base`
  std::vector<uint32_t> active;
  uint32_t top = 0;
  for (uint32_t u : order) {
    Unit &unit = units_[u];
    active.erase(std::remove_if(active.begin(), active.end(),
                                [&](uint32_t a) {
                                  if (units_[a].end >= unit.start) return false;
                                  for (uint32_t s = 0; s < units_[a].size; ++s) busy[units_[a].slot - base + s] = false;
                                  return true;
                                }),
                 active.end());
    // First fit: the lowest run of `size` free slots (a run reaching the end
    // of the frame grows it).
    uint32_t first = 0;
    while (true) {
      uint32_t n = 0;
      while (n < unit.size && first + n < busy.size() && !busy[first + n]) ++n;
      if (n == unit.size || first + n == busy.size()) break;
      first += n + 1; // busy[first + n] is taken
    }
    if (busy.size() < first + unit.size) busy.resize(first + unit.size, false);
    for (uint32_t s = 0; s < unit.size; ++s) busy[first + s] = true;
    unit.slot = base + first;
    top = std::max(top, first + unit.size);
    active.push_back(u);
  }
  return top;
}

// The arguments an inline.enter passes stay in their slots while its body runs, as the callee's parameters would hold
// them: the group, the frame's argv, of a body that reads them as received; else each argument's own unit (`heldArgs_`,
// a value in a slot of its own). Either lives to the body's last instruction (on every path, through the copies of the
// body).
void FramePlan::extendArgumentGroups() {
  for (const BasicBlock *b : cfg_.order())
    for (auto &i : b->insts)
      if (i->op == Op::InlineEnter && !groupUnit_.count(i.get()))
        for (uint32_t k = 0; k < i->operands().size(); ++k) {
          const Value *v = i->operand(k);
          auto u = unit_.find(v);
          auto h = homes_.find(v); // served in place: its variable holds it (a parameter: its pinned unit)
          bool place = h != homes_.end() && h->second.kind == Home::Kind::Place;
          if (u != unit_.end() && (!place || units_[u->second].pinned))
            heldArgs_[i.get()].push_back({k, u->second});
        }
  xir::walkBodies<const Instruction *>(cfg_, [&](const BasicBlock &b, auto &open) {
    bool changed = false;
    for (auto &i : b.insts) {
      if (i->op == Op::InlineEnter) {
        open.push_back({i.get()});
        continue;
      }
      if (i->op == Op::InlineLeave) {
        open.pop_back();
        continue;
      }
      uint32_t p = pos_.at(i.get());
      for (auto &enters : open)
        for (const Instruction *e : enters) {
          if (auto g = groupUnit_.find(e); g != groupUnit_.end() && units_[g->second].end < p) {
            units_[g->second].end = p;
            changed = true;
          }
          if (auto h = heldArgs_.find(e); h != heldArgs_.end())
            for (auto [k, u] : h->second) {
              changed |= heldIn_[i.get()].insert(u).second;
              if (units_[u].end < p) {
                units_[u].end = p;
                changed = true;
              }
            }
        }
    }
    return changed;
  });
}

// The units of each inlined body: values and argument groups of its instructions, the arguments it received, and a
// nested body's units still live where that one ends.
void FramePlan::findBodyUnits() {
  xir::walkBodies<uint32_t>(cfg_, [&](const BasicBlock &b, auto &open) {
    bool changed = false;
    auto add = [&](const Value *v) {
      auto u = unit_.find(v);
      if (u != unit_.end() && !open.empty()) open.back().insert(u->second);
    };
    auto addGroup = [&](const Instruction *i) {
      if (auto g = groupUnit_.find(i); g != groupUnit_.end() && !open.empty()) open.back().insert(g->second);
      if (auto h = heldArgs_.find(i); h != heldArgs_.end() && !open.empty())
        for (auto [k, u] : h->second) open.back().insert(u);
    };
    for (auto &p : b.params) add(p.get());
    for (auto &i : b.insts) {
      if (i->op == Op::InlineEnter) {
        open.emplace_back();
        addGroup(i.get());
        continue;
      }
      if (i->op == Op::InlineLeave) {
        auto &mine = bodyUnits_[i.get()];
        size_t before = mine.size();
        mine.insert(open.back().begin(), open.back().end());
        changed |= mine.size() != before;
        open.pop_back();
        uint32_t p = pos_.at(i.get());
        if (!open.empty())
          for (uint32_t u : mine)
            if (units_[u].start <= p && p <= units_[u].end) open.back().insert(u);
        continue;
      }
      add(i.get());
      addGroup(i.get());
    }
    return changed;
  });
}

void FramePlan::holdSlots(std::unordered_set<uint32_t> &held, uint32_t u) const {
  for (uint32_t s = 0; s < units_[u].size; ++s) held.insert(units_[u].slot + s);
}

// Adds to `slots` (kept sorted, without repeats) those of `units` that no unit live at position `at` holds. Live
// intervals cover every point where their value is live, so a slot a live value holds is never among them.
void FramePlan::addClears(std::vector<uint32_t> &slots, const std::unordered_set<uint32_t> &units, uint32_t at) const {
  std::unordered_set<uint32_t> held(slots.begin(), slots.end());
  for (uint32_t u = 0; u < units_.size(); ++u)
    if (units_[u].start <= at && at <= units_[u].end) holdSlots(held, u);
  for (uint32_t k : units)
    for (uint32_t s = 0; !units_[k].pinned && s < units_[k].size; ++s) // a parameter's: its own release alone
      if (held.insert(units_[k].slot + s).second) slots.push_back(units_[k].slot + s);
  std::sort(slots.begin(), slots.end());
}

void FramePlan::placeClears() {
  for (auto &[leave, units] : bodyUnits_) addClears(clears_[leave], units, pos_.at(leave));
  // The inline.enters whose bodies are open at each frame.release (their arguments are held there), and those of the
  // innermost body, the one the release is in.
  std::unordered_map<const Instruction *, std::unordered_set<const Instruction *>> openAt, innerAt;
  xir::walkBodies<const Instruction *>(cfg_, [&](const BasicBlock &b, auto &open) {
    bool changed = false;
    for (auto &i : b.insts) {
      if (i->op == Op::InlineEnter) open.push_back({i.get()});
      else if (i->op == Op::InlineLeave) open.pop_back();
      else if (i->op == Op::FrameRelease)
        for (auto &enters : open)
          for (const Instruction *e : enters) {
            changed |= openAt[i.get()].insert(e).second;
            if (&enters == &open.back()) changed |= innerAt[i.get()].insert(e).second;
          }
    }
    return changed;
  });
  // Whether release `i` lets go of parameter k of `fn` (in the body of `e`, inlined; null: the function's own): it is
  // that parameter's release, or one is on every path to it (a copy of the value still live there kept it: once dead,
  // nothing else would let go of it).
  std::vector<const Instruction *> paramReleases;
  for (const BasicBlock *b : cfg_.order())
    for (auto &i : b->insts)
      if (i->op == Op::FrameRelease && i->index) paramReleases.push_back(i.get());
  auto releases = [&](const Instruction *r, const Function *fn, uint32_t k, const Instruction *e) {
    return r->index == k + 1 && r->blockFn == fn && (e ? innerAt[r].count(e) != 0 : openAt[r].empty());
  };
  auto letsGo = [&](const Instruction *i, const Function *fn, uint32_t k, const Instruction *e) {
    if (releases(i, fn, k, e)) return true;
    for (const Instruction *r : paramReleases)
      if (r != i && releases(r, fn, k, e) &&
          (r->parent == i->parent ? pos_.at(r) < pos_.at(i) : cfg_.dominates(r->parent, i->parent)))
        return true;
    return false;
  };
  for (const BasicBlock *b : cfg_.order()) {
    std::vector<uint32_t> nilNow; // the slots NIL since the block's last release (while no unit started), and where
    uint32_t beforeAt = 0;
    bool before = false;
    for (auto &i : b->insts) {
      if (i->op != Op::FrameRelease) continue;
      // Every slot of a value written before it and dead there, unless a live value (or an argument group being
      // filled around it, or an open inlined body's) holds the slot: precise liveness, not the intervals, which span
      // the blocks laid out between a value's definition and its last use (an inlined body's arguments: its copies).
      uint32_t at = pos_.at(i.get());
      const std::unordered_set<uint32_t> &live = liveAtRelease_[i.get()];
      const std::unordered_set<const Instruction *> &open = openAt[i.get()];
      std::unordered_set<uint32_t> held;
      for (uint32_t u : live) holdSlots(held, u);
      for (auto &[user, u] : groupUnit_)
        if (user->op == Op::InlineEnter ? open.count(user) != 0 : units_[u].start <= at && at <= units_[u].end)
          holdSlots(held, u);
      // The arguments open bodies hold, but those this release lets go of (its parameter's, in the body it is in, or
      // one let go of before): another parameter holding it too, of that body or of an outer one, or a later use, keeps
      // it.
      for (const Instruction *e : open)
        if (auto h = heldArgs_.find(e); h != heldArgs_.end())
          for (auto [k, u] : h->second)
            if (!letsGo(i.get(), e->blockFn, k, e)) holdSlots(held, u);
      // A release right after another (`a := NIL`, `b := NIL`, ...), with no unit started in between, finds the slots
      // that one cleared still NIL: only the slots of the values that died since are left to clear.
      bool started = false;
      for (uint32_t u = 0; before && u < units_.size(); ++u)
        started |= beforeAt < units_[u].start && units_[u].start < at;
      if (!before || started) nilNow.clear();
      // The release of this function's parameter's LOCAL: the parameter's slot, its pinned unit's (or, when no load of
      // it is left, its own: nothing reads it). An argument group (a body that reads its arguments as received) keeps
      // them as received, as the function called would.
      bool own = i->index && i->blockFn == &f_;
      std::vector<uint32_t> &slots = clears_[i.get()];
      held.insert(nilNow.begin(), nilNow.end());
      const Var *param = own && i->index <= params_.size() ? params_[i->index - 1] : nullptr;
      if (param && varSlot_.count(param) && !loadedParams_.count(param))
        if (uint32_t s = varSlot_.at(param); held.insert(s).second) slots.push_back(s);
      for (uint32_t u = 0; u < units_.size(); ++u)
        if (units_[u].start < at && (!units_[u].pinned || letsGo(i.get(), &f_, units_[u].param, nullptr)))
          for (uint32_t s = 0; s < units_[u].size; ++s)
            if (held.insert(units_[u].slot + s).second) slots.push_back(units_[u].slot + s);
      std::sort(slots.begin(), slots.end());
      nilNow.insert(nilNow.end(), slots.begin(), slots.end());
      before = true;
      beforeAt = at;
    }
  }
}

// The units of each loop (values and argument groups of its blocks), cleared where it exits.
void FramePlan::placeLoopClears() {
  xir::Loops loops(cfg_);
  for (auto &loop : loops.all()) {
    std::unordered_set<uint32_t> units;
    auto add = [&](const Value *v) {
      if (auto u = unit_.find(v); u != unit_.end() && !units_[u->second].pinned) units.insert(u->second);
    };
    for (const BasicBlock *b : loop->blocks) {
      for (auto &p : b->params) add(p.get());
      for (auto &i : b->insts) {
        add(i.get());
        if (auto g = groupUnit_.find(i.get()); g != groupUnit_.end()) units.insert(g->second);
      }
    }
    for (const BasicBlock *b : loop->blocks)
      for (size_t k = 0; k < b->numSuccessors(); ++k) {
        const BasicBlock *exit = b->successor(k);
        if (loop->contains(exit)) continue;
        const std::vector<BasicBlock *> &preds = cfg_.preds(exit);
        bool onlyLoop = std::all_of(preds.begin(), preds.end(), [&](const BasicBlock *p) { return loop->contains(p); });
        const xir::Instruction *leave = b->terminator();
        bool branch = leave && (leave->op == xir::Op::Br || leave->op == xir::Op::CondBr) &&
                      k < leave->targets.size(); // not the unwind edge, which no branch takes
        if (!onlyLoop && branch) // a branch's edge may get a block of its own
          addClears(edgeClears_[{b, exit}], units, start_.at(exit));
        else
          addClears(blockClears_[exit], units, start_.at(exit));
      }
  }
}

const std::vector<uint32_t> &FramePlan::clearsOnEdge(const BasicBlock *from, const BasicBlock *to) const {
  static const std::vector<uint32_t> none;
  auto c = edgeClears_.find({from, to});
  return c == edgeClears_.end() ? none : c->second;
}

const std::vector<uint32_t> &FramePlan::clearsAtStart(const BasicBlock *b) const {
  static const std::vector<uint32_t> none;
  auto c = blockClears_.find(b);
  return c == blockClears_.end() ? none : c->second;
}

const std::vector<uint32_t> &FramePlan::clearsAt(const Instruction *at) const {
  static const std::vector<uint32_t> none;
  auto c = clears_.find(at);
  return c == clears_.end() ? none : c->second;
}

bool FramePlan::movesOut(const Instruction &load) const {
  const Var *v = load.var;
  return load.moves && load.access == Access::Direct && isParameter(v) && !registerParams_.count(v) && mayHoldHeap(*v);
}

void FramePlan::pinMovedParams() {
  for (BasicBlock *b : cfg_.order())
    for (auto &i : b->insts) {
      if (i->op == Op::Load && isParameter(i->var)) loadedParams_.insert(i->var);
      auto h = homes_.find(i.get());
      if (i->op != Op::Load || !movesOut(*i) || h == homes_.end() || h->second.kind != Home::Kind::Place) continue;
      uint32_t u = unitOf(i.get());
      units_[u].pinned = true;
      units_[u].slot = varSlot_.at(i->var);
      units_[u].param = i->var->index;
    }
}

// The loads of a FOR EACH variable that read its element from the loop's source and position (eachLoads_: the load ->
// the each.move whose element it is) rather than through the loop's reference: a variable of the frame alone (not
// shared, not a parameter: in no cell, and no reference or codeblock reaches it, nor a macro), whose last change on
// every path to the load is that each.move, outside a cell (no assignment between, nor an inner loop's restore): a
// forward dataflow of the last change of each such variable, none where paths bring different ones, and none where an
// unwinding enters (it may leave a block at any point). The source is live at those loads and at their uses in their
// block, which code generation reads it for (sourceReaders_). The variables all of whose loads are such
// (directEachVars_) need nothing of their reference's item over a string.
void FramePlan::findEachLoads() {
  std::unordered_map<const Var *, size_t> index;
  for (const BasicBlock *b : cfg_.order())
    for (auto &i : b->insts) {
      const Var *v = i->var;
      if (i->op == Op::EachMove && v->kind == Var::Kind::Frame && !v->shared && !v->param)
        index.emplace(v, index.size());
    }
  if (index.empty()) return;
  using State = std::vector<const Instruction *>; // per variable: the each.move that last changed it, or none
  std::unordered_set<const BasicBlock *> unwindTargets;
  for (const BasicBlock *b : cfg_.order())
    if (b->unwind) unwindTargets.insert(b->unwind);
  auto step = [&](const Instruction &i, State &s) {
    auto k = i.var ? index.find(i.var) : index.end();
    if (k == index.end() || (opInfo(i.op).effects & WritesVar) == 0) return;
    const Constant *inCell = i.op == Op::EachMove ? asBool(i.operand(0)) : nullptr;
    s[k->second] = inCell && !inCell->logical ? &i : nullptr;
  };
  std::unordered_map<const BasicBlock *, State> out;
  auto entering = [&](const BasicBlock *b) {
    State in(index.size(), nullptr);
    if (unwindTargets.count(b) || b == cfg_.order().front()) return in;
    bool first = true;
    for (const BasicBlock *p : cfg_.preds(b)) {
      auto o = out.find(p);
      if (o == out.end()) continue; // not reached yet: no change of its own to join
      for (size_t k = 0; k < in.size(); ++k)
        if (first) in[k] = o->second[k];
        else if (in[k] != o->second[k]) in[k] = nullptr;
      first = false;
    }
    return in;
  };
  for (bool changed = true; changed;) {
    changed = false;
    for (const BasicBlock *b : cfg_.order()) {
      State s = entering(b);
      for (auto &i : b->insts) step(*i, s);
      auto o = out.find(b);
      if (o == out.end() || o->second != s) {
        out[b] = std::move(s);
        changed = true;
      }
    }
  }
  std::unordered_set<const Var *> through; // variables with a load that is not such
  for (const BasicBlock *b : cfg_.order()) {
    State s = entering(b);
    for (auto &i : b->insts) {
      if (i->op == Op::Load && i->access == Access::ThroughRef) {
        auto k = index.find(i->var);
        const Instruction *move = k == index.end() ? nullptr : s[k->second];
        if (!move) {
          through.insert(i->var);
        } else {
          eachLoads_[i.get()] = move;
          directEachVars_.insert(i->var);
          auto &readers = sourceReaders_[move->operand(2)];
          readers.push_back(i.get());
          for (const Instruction *u : i->users())
            if (u->parent == b) readers.push_back(u);
        }
      }
      step(*i, s);
    }
  }
  for (const Var *v : through) directEachVars_.erase(v);
}

const Instruction *FramePlan::eachMoveOf(const Instruction *load) const {
  auto m = eachLoads_.find(load);
  return m == eachLoads_.end() ? nullptr : m->second;
}

std::optional<uint32_t> FramePlan::lastUse(const Value *v, const Instruction *user) const {
  auto u = unit_.find(v);
  if (u == unit_.end() || types_.noHeap(v)) return std::nullopt; // nothing to let go
  if (auto h = heldIn_.find(user); h != heldIn_.end() && h->second.count(u->second)) return std::nullopt; // a body's
  const Unit &unit = units_[u->second];
  if (unit.end != pos_.at(user) || unit.start >= pos_.at(user)) return std::nullopt;
  const Home &h = homes_.at(v); // a load served in place has a unit for its shadow slot: the variable is not the call's
  if (h.kind != Home::Kind::Slot) return std::nullopt;
  return h.slot;
}

const Home &FramePlan::home(const Value *v) const {
  static const Home none{}, constant{Home::Kind::Constant, 0, nullptr};
  if (v->kind() == Value::Kind::Constant) return v->type() == Type::Item ? constant : none;
  auto h = homes_.find(v);
  return h == homes_.end() ? none : h->second;
}

// Operand `operand` of a call or array literal was computed straight into its
// slot of the group.
bool FramePlan::inGroup(const Instruction *user, size_t operand) const {
  auto m = groupMember_.find(user->operand(operand));
  return m != groupMember_.end() && m->second == operand && unit_.at(user->operand(operand)) == groupUnit_.at(user);
}

} // namespace xb
