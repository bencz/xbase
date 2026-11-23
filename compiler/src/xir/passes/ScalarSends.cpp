// Sends to values of a built-in type that the runtime's own class of the type answers (its SCALAR CLASSes, written in
// xBase in a carried runtime source: arrays, strings and hashes are Enumerable) become calls of its methods, which the
// inliner then runs in place: `a:Where( b1 ):Select( b2 ):Sum()` turns into the bodies of Where, Select and Sum, whose
// generators fuse into one loop (fuse-generators) whose blocks run in place (inline-blocks).
//
// A program may answer a type's messages before the runtime does (a SCALAR CLASS of its own, ClassAssociate()), which
// only the running program knows: the calls run behind a test of that (`scalar.plain`, xb_scalar_replaced), the sends
// as they were otherwise. A chain is taken whole: its first send goes to a value of the type, each next one to the
// enumerator the method before made (a generator, whose messages are always Enumerable's), so one test covers it, and
// the second version keeps what the first one's values are. The chain's instructions (its sends, and the arguments
// computed between them) are copied into the version that calls; the values they make are joined after it.
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"
#include "xir/Builder.h"
#include "xir/ClassModel.h"
#include "xir/Transform.h"
#include "xir/Types.h"

#include "xbrt.h" // xb_type_class_name

#include <algorithm>
#include <array>
#include <bit>
#include <memory>

namespace xb::xir {

namespace {

// The runtime's class every collection inherits (rtl/collections.prg), whose methods answer a generator's messages.
constexpr const char *kEnumerable = "ENUMERABLE";

// The runtime's classes the carried sources define: of each type, and Enumerable (a generator's other messages).
struct RuntimeClasses {
  std::unique_ptr<ClassModel> model;
  std::array<int32_t, XB_T_COUNT_> ofType;
  int32_t enumerable = -1;

  explicit RuntimeClasses(const Module &m) {
    ofType.fill(-1);
    for (const Module *l : m.library) {
      bool any = false;
      for (size_t c = 0; c < l->classes.size(); ++c) {
        const Module::Class &k = l->classes[c];
        if (k.name == kEnumerable) enumerable = (int32_t)c;
        for (uint32_t t = 0; k.scalar && t < XB_T_COUNT_; ++t)
          if (const char *name = xb_type_class_name(t); name && k.name == name) ofType[t] = (int32_t)c;
        any |= k.scalar || k.name == kEnumerable;
      }
      if (any) {
        model = std::make_unique<ClassModel>(*l);
        return;
      }
    }
  }

  // The method of class `k` a send of `message` from elsewhere reaches, or null.
  Function *method(int32_t k, const std::string &message) const {
    if (!model || k < 0) return nullptr;
    std::optional<xb_member> member = model->resolve(k, -1, false, message);
    return member && member->kind == XB_MK_METHOD ? model->method(*member) : nullptr;
  }
};

// A function whose every return gives the enumerator it made (gen.new): a generator, or a method that is one.
bool makesGenerator(const Function &f) {
  bool any = false;
  for (auto &b : f.blocks)
    if (Instruction *t = b->terminator(); t && t->op == Op::Ret) {
      if (t->operands().empty() || !asInstruction(t->operand(0), Op::GenNew)) return false;
      any = true;
    }
  return any;
}

bool plainSend(const Instruction &i) {
  return i.op == Op::Send && !i.hasRest() && !i.superSend && !i.dispatched;
}

struct Link {
  Instruction *send;
  Function *method;
};

// Versions the instructions of block `b` from position `first` to `last` (the chain's sends) on the test of the
// receiver's type (one of `types`, a mask) and of its class: in the copy that runs when the runtime's class answers,
// each send of `chain` calls its method.
void version(Function &f, BasicBlock *b, size_t first, size_t last, TypeSet types, const std::vector<Link> &chain) {
  BasicBlock *region = splitBefore(f, b, first);
  splitAfter(f, region, last - first);
  BlockCopy copy = copyBlocks(f, {region}, ".runtime");
  BasicBlock *direct = copy.blocks.at(region);
  for (const Link &l : chain) {
    auto *send = static_cast<Instruction *>(copy.values.at(l.send));
    auto call = std::make_unique<Instruction>(Op::Call, Type::Item, direct);
    copy.values[l.send] = call.get();
    call->loc = send->loc;
    call->callee.function = l.method;
    for (Value *v : send->operands()) call->addOperand(v);
    send->replaceAllUsesWith(call.get());
    send->dropOperands();
    for (auto &slot : direct->insts)
      if (slot.get() == send) slot = std::move(call);
  }
  for (const Link &l : chain) l.send->dispatched = true; // the version for a program's class
  Builder builder(&f);
  SourceLoc loc = chain.front().send->loc;
  b->takeTerminator();
  builder.setBlock(b);
  builder.setLoc(loc);
  builder.condBr(builder.scalarPlain(chain.front().send->operand(0), types), direct, region);
  std::vector<Definitions> defs;
  for (auto &i : region->insts) defs.push_back({{region, i.get()}, {direct, copy.values.at(i.get())}});
  joinDefinitions(f, defs, {region, direct});
}

} // namespace

bool resolveScalarSends(Module &m, Function &f) {
  if (m.library.empty()) return false;
  RuntimeClasses classes(m);
  if (!classes.model) return false;
  std::unique_ptr<ClassModel> own = m.classes.empty() ? nullptr : std::make_unique<ClassModel>(m);
  bool changed = false;
  for (bool again = true; again;) {
    again = false;
    CFG cfg(f);
    TypeInfo types(cfg);
    std::unique_ptr<ClassInfo> objects = own ? std::make_unique<ClassInfo>(f, cfg, *own) : nullptr;
    for (BasicBlock *b : cfg.order()) {
      for (size_t k = 1; k < b->insts.size() && !again; ++k) {
        Instruction *s = b->insts[k].get();
        if (!plainSend(*s) || (objects && objects->of(s->operand(0)) != kNoClass)) continue; // an object's class
        // The types the receiver may have whose classes answer with one method (the most of them; arrays first)
        TypeSet may = types.of(s->operand(0)), mask = 0;
        Function *fn = nullptr;
        for (uint32_t t : {XB_T_ARRAY, XB_T_STRING, XB_T_HASH, XB_T_INT, XB_T_DOUBLE, XB_T_DATE, XB_T_LOGICAL,
                           XB_T_BLOCK, XB_T_SYMBOL, XB_T_NIL}) {
          Function *g = (may & typeBit((xb_type)t)) ? classes.method(classes.ofType[t], s->message) : nullptr;
          if (!g) continue;
          TypeSet same = 0;
          for (uint32_t u = 0; u < XB_T_COUNT_; ++u)
            if ((may & typeBit((xb_type)u)) && classes.method(classes.ofType[u], s->message) == g)
              same |= typeBit((xb_type)u);
          if (std::popcount(same) > std::popcount(mask)) {
            mask = same;
            fn = g;
          }
        }
        if (!fn) continue;
        std::vector<Link> chain{{s, fn}};
        for (Instruction *at = s; makesGenerator(*chain.back().method) && at->hasOneUse();) {
          Instruction *u = at->users().front();
          if (!plainSend(*u) || u->parent != b || u->operand(0) != at) break;
          Function *g = classes.method(classes.enumerable, u->message);
          if (!g) break;
          chain.push_back({u, g});
          at = u;
        }
        // A receiver that may be an object: only a chain, or a send of a codeblock literal (Enumerable's way), is worth
        // the test and the copy (an object's own Count() or Add() is not).
        bool literal = std::any_of(s->operands().begin(), s->operands().end(),
                                   [](const Value *v) { return asInstruction(v, Op::MakeBlock) != nullptr; });
        if ((may & typeBit(XB_T_OBJECT)) && chain.size() < 2 && !literal) continue;
        size_t last = k;
        while (b->insts[last].get() != chain.back().send) ++last;
        version(f, b, k, last, mask, chain);
        again = changed = true;
      }
      if (again) break;
    }
  }
  return changed;
}

} // namespace xb::xir
