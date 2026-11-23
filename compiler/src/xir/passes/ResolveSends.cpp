// A send to an object whose class is known exactly (a class of the module whose whole line is the module's, ClassInfo),
// and a Super send of a method to its own Self (a method), reaches what the runtime would reach: found when compiling
// by the runtime's own rules (xbclass.h, through ClassModel), a method becomes a call of its function, an instance
// variable read or assigned an object.get or object.set of its slot.
//
// A send to an object whose class is not known (made by a macro, read from an array, a method's Self) reaches the same
// when the object is of a class whose messages are resolved here: when the classes of the module that answer the
// message all reach one method (or one class reaches an instance variable), the send is versioned on the object's
// class (`object.is`): the version for those classes reaches what resolving gives (a call the inliner may then run in
// place), the other keeps the send (`dispatched`). A class's members are fixed once it is prepared, so an object of
// the class, exactly, reaches them whatever made it. A method's sends to its own Self are left alone: its body stays as
// small as it was, to be inlined where its class is known (resolve-sends there, or its copy typed for it) or not (its
// sends are then the caller's, which this versions). But for two, which reach one thing whatever the object's class
// (the method's or a subclass of it): every send to Self in a method of a FINAL class, which has no subclass, and a
// send to Self that reaches a FINAL method, which no subclass that makes objects declares again (xb_class_check).
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"
#include "xir/Builder.h"
#include "xir/ClassModel.h"
#include "xir/Transform.h"
#include "xir/Types.h"

#include "xbrt.h" // type tags

#include <algorithm>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

namespace xb::xir {

namespace {

// A read of the method's own Self (its parameter 0), which nothing in it assigns (onlyLoaded): a Super send to it
// reaches the parent's method of the method's class, which the runtime finds from that class alone (a value that is
// no object included: runtime.md). A variable's slot is the object's class's (the runtime's to find).
// Whether send `i` to its method's own Self reaches one thing whatever the object's class: the method's class is FINAL,
// or the message reaches a FINAL method from it.
bool sealed(const Module &m, const ClassModel &model, const Instruction &i) {
  if (m.classes[(size_t)i.classIndex].kind & XB_CLASS_FINAL) return true;
  std::optional<xb_member> member = model.resolve(i.classIndex, i.classIndex, false, i.message);
  return member && member->kind == XB_MK_METHOD && (member->flags & XB_MEMBER_FINAL);
}

bool readsOwnSelf(const Function &f, const Value *v) {
  const Instruction *load = asInstruction(v, Op::Load);
  const Var *self = load ? load->var : nullptr;
  return f.method && self && self->kind == Var::Kind::Frame && self->index == 0 && onlyLoaded(f, self);
}

// What send `i` becomes in block `b` when its receiver is an object of class `c` that reaches `member`: a call of a
// method, an object.get or object.set of an instance variable; null for what the runtime answers itself (a class
// variable, an assignment of several values or of a reference).
std::unique_ptr<Instruction> resolved(const ClassModel &model, int32_t c, const xb_member &member, const Instruction &i,
                                      BasicBlock *b) {
  std::unique_ptr<Instruction> made;
  switch (member.kind) {
  case XB_MK_METHOD: {
    Function *fn = model.method(member);
    if (!fn) return nullptr;
    made = std::make_unique<Instruction>(Op::Call, Type::Item, b);
    made->callee.function = fn;
    for (Value *v : i.operands()) made->addOperand(v);
    break;
  }
  case XB_MK_GET: // the arguments are ignored, as the runtime does
    made = std::make_unique<Instruction>(Op::ObjectGet, Type::Item, b);
    made->addOperand(i.operand(0));
    made->index = model.slot(c, member);
    break;
  case XB_MK_SET: // NIL for none, the first of several, the value a reference designates: the runtime's
    if (i.operands().size() != 2 || isReference(i.operand(1))) return nullptr;
    made = std::make_unique<Instruction>(Op::ObjectSet, Type::Item, b);
    made->addOperand(i.operand(0));
    made->addOperand(i.operand(1));
    made->index = model.slot(c, member);
    break;
  default: return nullptr; // a class variable: the runtime's
  }
  made->loc = i.loc;
  return made;
}

// New sent to an object of class `c` (send `k` of block `b`) that no member of its line answers, nor names (a cast):
// what the runtime answers itself, the class's Init sent from the class with the arguments, then the object: a call
// of that method (its result unused) when it is one the class reaches, nothing when no member answers Init; the send's
// value is the object. False, the send kept, for anything else (an Init variable, a scope only a friend passes);
// `erased` tells that the send left the block with nothing in its place. The send goes to `removed`, kept to the end of
// the pass: ClassInfo knows values by address, which a new instruction must not take over.
bool newByInit(const ClassModel &model, int32_t c, BasicBlock *b, size_t k, bool &erased,
               std::vector<std::unique_ptr<Instruction>> &removed) {
  Instruction &send = *b->insts[k];
  if (send.message != "NEW" || !model.answersByDefault(c, "NEW")) return false;
  Value *object = send.operand(0);
  std::unique_ptr<Instruction> made;
  if (std::optional<xb_member> init = model.resolve(c, c, false, "INIT")) {
    if (init->kind != XB_MK_METHOD || !(made = resolved(model, c, *init, send, b))) return false;
  } else if (!model.answersByDefault(c, "INIT")) {
    return false;
  }
  send.replaceAllUsesWith(object);
  send.dropOperands();
  erased = !made;
  removed.push_back(std::move(b->insts[k]));
  if (made) b->insts[k] = std::move(made);
  else b->insts.erase(b->insts.begin() + (std::ptrdiff_t)k);
  return true;
}

// The classes a test of the receiver may take for a send, at most this many (a test each).
constexpr size_t kMaxSpeculated = 8;

// The classes of the module that make objects of their own (not SCALAR ones, whose values are no objects, nor ABSTRACT
// ones and interfaces) that answer send `i` by resolving it to
// what `*member` is set to: a method all of them reach, or an instance variable of the one class that reaches it;
// none when the classes answering reach different members, or too many of them do.
std::vector<int32_t> answering(const Module &m, const ClassModel &model, const Instruction &i,
                               std::optional<xb_member> &member) {
  std::vector<int32_t> out;
  const Function *fn = nullptr;
  for (int32_t c = 0; c < (int32_t)m.classes.size(); ++c) {
    if (m.classes[(size_t)c].scalar || (m.classes[(size_t)c].kind & (XB_CLASS_ABSTRACT | XB_CLASS_INTERFACE)))
      continue; // no objects of its own
    std::optional<xb_member> reached = model.resolve(c, i.classIndex, false, i.message);
    if (!reached) continue;
    const Function *g = reached->kind == XB_MK_METHOD ? model.method(*reached) : nullptr;
    if (!out.empty() && (!g || g != fn)) return {};
    fn = g;
    member = reached;
    out.push_back(c);
  }
  if (out.size() > kMaxSpeculated) return {};
  return out;
}

// Versions send `s` (at position `k` of block `b`) on the class of its receiver: the test goes right before it, and the
// copy that runs when the receiver is one of `classes` holds `made` instead of the send.
void version(Function &f, BasicBlock *b, size_t k, const std::vector<int32_t> &classes,
             std::unique_ptr<Instruction> made) {
  Instruction *s = b->insts[k].get();
  Builder builder(&f);
  builder.setBlock(b);
  builder.setLoc(s->loc);
  size_t before = b->insts.size();
  Value *test = nullptr;
  for (int32_t c : classes) {
    Value *is = builder.objectIs(s->operand(0), c);
    test = test ? builder.boolNot(builder.boolAnd(builder.boolNot(test), builder.boolNot(is))) : is;
  }
  size_t tests = b->insts.size() - before; // made at the end: moved before the send
  std::rotate(b->insts.begin() + (std::ptrdiff_t)k, b->insts.end() - (std::ptrdiff_t)tests, b->insts.end());
  BasicBlock *region = splitAfter(f, b, k + tests - 1);
  splitAfter(f, region, 0);
  BlockCopy copy = copyBlocks(f, {region}, ".resolved");
  BasicBlock *direct = copy.blocks.at(region);
  auto *copied = static_cast<Instruction *>(copy.values.at(s));
  made->parent = direct;
  copy.values[s] = made.get();
  copied->replaceAllUsesWith(made.get());
  copied->dropOperands();
  for (auto &slot : direct->insts)
    if (slot.get() == copied) slot = std::move(made);
  s->dispatched = true; // the version for the other classes
  b->takeTerminator();
  builder.setBlock(b);
  builder.condBr(test, direct, region);
  joinDefinitions(f, {{{region, s}, {direct, copy.values.at(s)}}}, {region, direct});
}

} // namespace

bool resolveSends(Module &m, Function &f) {
  if (m.classes.empty()) return false;
  ClassModel model(m);
  CFG cfg(f);
  ClassInfo classes(f, cfg, model);
  std::vector<std::unique_ptr<Instruction>> removed; // freed at the end: ClassInfo knows values by address
  bool changed = false;
  for (BasicBlock *b : cfg.order()) {
    for (size_t k = 0; k < b->insts.size(); ++k) {
      Instruction &i = *b->insts[k];
      // An AWAIT fuses with the send right after it: the runtime hands it the method the send reaches.
      if (i.op != Op::Send || i.hasRest() || (k > 0 && b->insts[k - 1]->op == Op::AwaitFuse)) continue;
      int32_t c = classes.of(i.operand(0));
      bool fromMethod = c == kNoClass && i.superSend && i.classIndex >= 0 && readsOwnSelf(f, i.operand(0));
      if (fromMethod) c = i.classIndex;
      bool toSelf = c == kNoClass && !i.superSend && i.classIndex >= 0 &&
                    !m.classes[(size_t)i.classIndex].scalar && readsOwnSelf(f, i.operand(0));
      if (toSelf && sealed(m, model, i)) c = i.classIndex;
      else if (toSelf) continue;
      if (c == kNoClass) continue;
      std::optional<xb_member> member = model.resolve(c, i.classIndex, i.superSend, i.message);
      bool erased = false;
      if (!member && !i.superSend && !fromMethod && !toSelf && newByInit(model, c, b, k, erased, removed)) {
        if (erased) --k; // the next instruction is at k now (unsigned: wraps, then back to 0)
        changed = true;
        continue;
      }
      if (!member || (fromMethod && member->kind != XB_MK_METHOD)) continue;
      std::unique_ptr<Instruction> made = resolved(model, c, *member, i, b);
      if (!made) continue;
      i.replaceAllUsesWith(made.get());
      i.dropOperands();
      removed.push_back(std::move(b->insts[k]));
      b->insts[k] = std::move(made);
      changed = true;
    }
  }
  return changed;
}

bool speculateSends(Module &m, Function &f) {
  if (m.classes.empty()) return false;
  ClassModel model(m);
  bool changed = false;
  for (bool again = true; again;) {
    again = false;
    CFG cfg(f);
    TypeInfo types(cfg);
    ClassInfo classes(f, cfg, model);
    for (BasicBlock *b : cfg.order()) {
      for (size_t k = 0; k < b->insts.size() && !again; ++k) {
        Instruction &s = *b->insts[k];
        if (s.op != Op::Send || s.hasRest() || s.superSend || s.dispatched ||
            (k > 0 && b->insts[k - 1]->op == Op::AwaitFuse) ||
            classes.of(s.operand(0)) != kNoClass || !(types.of(s.operand(0)) & typeBit(XB_T_OBJECT)) ||
            readsOwnSelf(f, s.operand(0)))
          continue;
        std::optional<xb_member> member;
        std::vector<int32_t> those = answering(m, model, s, member);
        if (those.empty()) continue;
        std::unique_ptr<Instruction> made = resolved(model, those.front(), *member, s, b);
        if (!made) continue;
        version(f, b, k, those, std::move(made));
        again = changed = true;
      }
      if (again) break;
    }
  }
  return changed;
}

} // namespace xb::xir
