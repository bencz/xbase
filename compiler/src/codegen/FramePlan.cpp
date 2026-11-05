// Frame layout of an xIR function: homes of item values, liveness, and slot
// assignment by linear scan (see FramePlan.h).
#include "codegen/FramePlan.h"

#include <algorithm>

namespace xb {

using namespace xir;

namespace {

bool isItemResult(const Value *v) { return v->kind() == Value::Kind::Instruction && v->type() == Type::Item; }

bool isGroupUser(const Instruction &i) { return i.op == Op::Call || i.op == Op::ArrayLit; }

} // namespace

FramePlan::FramePlan(const Function &f) : f_(f), cfg_(f) {
  number();
  chooseHomes();
  computeLiveness();
  allocate();
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
// STATIC, or one reached through a reference can also change through calls,
// element stores and any store (references may designate the same element).
// One pass per block: the position of the last write of each variable and of
// the last instruction that may write any variable indirectly.
void FramePlan::elideLoads(const BasicBlock &b) {
  std::unordered_map<const Var *, uint32_t> lastWrite;
  uint32_t lastIndirect = 0;
  for (auto &ip : b.insts) {
    const Instruction &u = *ip;
    if (!isGroupUser(u) && u.op != Op::Br && u.op != Op::CondBr) {
      for (const Value *v : u.operands()) {
        if (v->kind() != Value::Kind::Instruction || !v->hasOneUse()) continue;
        auto &load = static_cast<const Instruction &>(*v);
        if (load.op != Op::Load || load.parent != &b) continue;
        uint32_t at = pos_.at(&load);
        auto w = lastWrite.find(load.var);
        bool written = w != lastWrite.end() && w->second > at;
        if (!isPlain(*load.var, load.access)) written |= lastIndirect > at;
        if (!written) homes_[&load] = {Home::Kind::Place, 0, &load};
      }
    }
    uint32_t p = pos_.at(&u);
    switch (u.op) {
    case Op::Store: case Op::EachInCell: case Op::EachEnter: case Op::EachMove: case Op::EachLeave:
    case Op::RefVar: // moves the variable into a cell
      lastWrite[u.var] = p;
      lastIndirect = p;
      break;
    case Op::MakeBlock: // moves the captured variables into cells
      for (const Var *c : u.captures) lastWrite[c] = p;
      lastIndirect = p;
      break;
    case Op::Call: case Op::IndexSet:
      lastIndirect = p;
      break;
    default:
      break;
    }
  }
}

// A value computed into the variable of the store right after it. A call may
// only write into a plain frame variable: its result must not go straight
// into a heap object (a cell, an element), and the callee must not see the
// variable cleared. The address is reached before the operation, so a store
// whose address can fail (through a reference) takes no operation that can:
// the operation's error comes first.
bool FramePlan::canForward(const Instruction &value, const Instruction &store) const {
  if (store.op != Op::Store || store.operand(0) != &value || !value.hasOneUse()) return false;
  if (store.mayFail() && value.mayFail()) return false;
  switch (value.op) {
  case Op::Unary: case Op::Binary: case Op::Index: case Op::ArrayLit: case Op::MakeBlock: case Op::NumDefaultDec:
  case Op::FromBool:
    return true;
  case Op::Call:
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
    for (size_t k = 0; k < b->insts.size(); ++k) {
      const Instruction &i = *b->insts[k];
      if (i.op == Op::Store && k > 0 && canForward(*b->insts[k - 1], i)) {
        homes_[b->insts[k - 1].get()] = {Home::Kind::Place, 0, &i};
        forwardedStores_.insert(&i);
      }
      if (i.op == Op::Ret && i.operands().size() == 1) {
        const Value *v = i.operand(0);
        if (isItemResult(v) && v->hasOneUse() && static_cast<const Instruction *>(v)->parent == b &&
            static_cast<const Instruction *>(v)->op != Op::Load)
          homes_[v] = {Home::Kind::Ret, 0, nullptr};
      }
    }
  }
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
        const BasicBlock *defBlock = v->kind() == Value::Kind::Param ? static_cast<const BlockParam *>(v)->block
                                                                     : static_cast<const Instruction *>(v)->parent;
        if (defBlock != b || pos_.at(v) >= pos_.at(&i)) continue;
        groupMember_[v] = (uint32_t)k; // a block parameter here takes its arguments into the group slot too
        unit_[v] = u;
      }
    }
  }
  // Branch arguments used only there share their parameter's slot, when the
  // parameter is not used after the argument is computed. Two edges to one
  // block carry two arguments live at once: neither shares.
  for (BasicBlock *b : cfg_.order()) {
    const Instruction *t = b->terminator();
    for (const Target &tg : t->targets) {
      if (std::count_if(t->targets.begin(), t->targets.end(), [&](const Target &o) { return o.block == tg.block; }) > 1)
        continue;
      for (size_t k = 0; k < tg.args.size(); ++k) {
        const Value *v = tg.args[k];
        const BlockParam *p = tg.block->params[k].get();
        if (p->type() != Type::Item || !isItemResult(v) || !v->hasOneUse() || homes_.count(v) || unit_.count(v))
          continue;
        if (static_cast<const Instruction *>(v)->parent != b || cfg_.dominates(tg.block, b)) continue;
        unit_[v] = unitOf(p);
        auto m = groupMember_.find(p);
        if (m != groupMember_.end()) groupMember_[v] = m->second;
      }
    }
  }
  // Every other item value gets a slot of its own.
  for (BasicBlock *b : cfg_.order()) {
    for (auto &p : b->params)
      if (p->type() == Type::Item) unitOf(p.get());
    for (auto &i : b->insts)
      if (isItemResult(i.get()) && !homes_.count(i.get())) unitOf(i.get());
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
  for (auto &[v, u] : unit_) {
    ++stamp;
    const BasicBlock *def = v->kind() == Value::Kind::Param ? static_cast<const BlockParam *>(v)->block
                                                            : static_cast<const Instruction *>(v)->parent;
    uint32_t start = pos_.at(v), end = start;
    auto useIn = [&](const BasicBlock *b, uint32_t p) {
      end = std::max(end, p);
      if (b == def) return;
      uint32_t bi = blockIndex_.at(b);
      if (liveIn[bi] == stamp) return;
      liveIn[bi] = stamp;
      work.push_back(b);
    };
    for (const Instruction *user : v->users()) useIn(user->parent, pos_.at(user));
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
  }
  for (auto &[user, u] : groupUnit_) {
    uint32_t p = pos_.at(user);
    units_[u].start = std::min(units_[u].start, p);
    units_[u].end = std::max(units_[u].end, p);
  }
}

// Linear scan: units in order of start; a unit's slots are free again once
// the unit has ended before the next one starts (a result never shares a slot
// with an operand of the same instruction: calls clear their result first).
void FramePlan::allocate() {
  const uint32_t base = f_.numFrameVars();
  std::vector<uint32_t> order(units_.size());
  for (uint32_t k = 0; k < order.size(); ++k) order[k] = k;
  std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
    return units_[a].start != units_[b].start ? units_[a].start < units_[b].start : a < b;
  });
  std::vector<bool> busy; // temporaries, from slot `base`
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
  frameSize_ = base + top;
  for (auto &[user, u] : groupUnit_) groupBase_[user] = units_[u].slot;
  for (auto &[v, u] : unit_) {
    auto m = groupMember_.find(v);
    homes_[v] = {Home::Kind::Slot, units_[u].slot + (m == groupMember_.end() ? 0 : m->second), nullptr};
  }
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
