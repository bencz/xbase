// A send to an object whose class is known exactly (a class of the module whose whole line is the module's, ClassInfo),
// and a Super send of a method to its own Self (a method), reaches what the runtime would reach: found when compiling
// by the runtime's own rules (xbclass.h, through ClassModel), a method becomes a call of its function, an instance
// variable read or assigned an object.get or object.set of its slot.
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"
#include "xir/ClassModel.h"

#include <memory>
#include <unordered_map>

namespace xb::xir {

namespace {

// A read of the method's own Self (its parameter 0), which nothing in it assigns (onlyLoaded): a Super send to it
// reaches the parent's method of the method's class, which the runtime finds from that class alone (a value that is
// no object included: runtime.md). A variable's slot is the object's class's (the runtime's to find).
bool readsOwnSelf(const Function &f, const Value *v) {
  const Instruction *load = asInstruction(v, Op::Load);
  const Var *self = load ? load->var : nullptr;
  return f.method && self && self->kind == Var::Kind::Frame && self->index == 0 && onlyLoaded(f, self);
}

} // namespace

bool resolveSends(Module &m, Function &f) {
  if (m.classes.empty()) return false;
  ClassModel model(m);
  CFG cfg(f);
  ClassInfo classes(f, cfg, model);
  bool changed = false;
  for (BasicBlock *b : cfg.order()) {
    for (size_t k = 0; k < b->insts.size(); ++k) {
      Instruction &i = *b->insts[k];
      // An AWAIT fuses with the send right after it: the runtime hands it the method the send reaches.
      if (i.op != Op::Send || i.hasRest() || (k > 0 && b->insts[k - 1]->op == Op::AwaitFuse)) continue;
      int32_t c = classes.of(i.operand(0));
      bool fromMethod = c == kNoClass && i.superSend && i.classIndex >= 0 && readsOwnSelf(f, i.operand(0));
      if (fromMethod) c = i.classIndex;
      if (c == kNoClass) continue;
      std::optional<xb_member> member = model.resolve(c, i.classIndex, i.superSend, i.message);
      if (!member || (fromMethod && member->kind != XB_MK_METHOD)) continue;
      std::unique_ptr<Instruction> made;
      switch (member->kind) {
      case XB_MK_METHOD: {
        Function *fn = model.method(*member);
        if (!fn) continue;
        made = std::make_unique<Instruction>(Op::Call, Type::Item, b);
        made->callee.function = fn;
        for (Value *v : i.operands()) made->addOperand(v);
        break;
      }
      case XB_MK_GET: // the arguments are ignored, as the runtime does
        made = std::make_unique<Instruction>(Op::ObjectGet, Type::Item, b);
        made->addOperand(i.operand(0));
        made->index = model.slot(c, *member);
        break;
      case XB_MK_SET: // NIL for none, the first of several, the value a reference designates: the runtime's
        if (i.operands().size() != 2 || isReference(i.operand(1))) continue;
        made = std::make_unique<Instruction>(Op::ObjectSet, Type::Item, b);
        made->addOperand(i.operand(0));
        made->addOperand(i.operand(1));
        made->index = model.slot(c, *member);
        break;
      default: continue; // a class variable: the runtime's
      }
      made->loc = i.loc;
      i.replaceAllUsesWith(made.get());
      i.dropOperands();
      b->insts[k] = std::move(made);
      changed = true;
    }
  }
  return changed;
}

} // namespace xb::xir
