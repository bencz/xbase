// Structural checks of xIR: block shape, operand counts and types, branch
// arguments, and SSA dominance. A failure is a compiler bug, reported with the
// function's text.
#include "xir/Verifier.h"

#include "xir/Analysis.h"
#include "xir/Printer.h"

#include <unordered_map>
#include <unordered_set>

namespace xb::xir {

namespace {

class Verifier {
public:
  Verifier(const Module &m, const Function &f) : m_(m), f_(f), cfg_(f) {}

  std::string run() {
    for (auto &b : f_.blocks) owned_.insert(b.get());
    for (auto &c : f_.constants) defined_[c.get()] = {nullptr, 0};
    for (auto &b : f_.blocks) {
      for (auto &p : b->params) defined_[p.get()] = {b.get(), 0};
      for (size_t k = 0; k < b->insts.size(); ++k) defined_[b->insts[k].get()] = {b.get(), k};
    }
    for (auto &b : f_.blocks) block(*b);
    useLists();
    return error_;
  }

private:
  struct Def {
    const BasicBlock *block; // null for constants
    size_t index;            // an instruction's position in its block
  };

  void fail(const std::string &what) {
    if (!error_.empty()) return;
    error_ = f_.name + ": " + what;
    if (cur_ && cur_->loc.line) error_ += " (line " + std::to_string(cur_->loc.line) + ")";
  }

  // Every value's user list holds each instruction once per operand or branch
  // argument using the value (the passes edit both sides together).
  void useLists() {
    std::unordered_map<const Value *, size_t> uses;
    for (auto &b : f_.blocks)
      for (auto &i : b->insts) {
        for (const Value *v : i->operands()) ++uses[v];
        for (const Target &t : i->targets)
          for (const Value *v : t.args) ++uses[v];
      }
    auto check = [&](const Value &v) {
      if (v.users().size() != uses[&v]) fail("the use list of a value does not match its uses");
      for (const Instruction *u : v.users())
        if (!owned_.count(u->parent)) fail("a value is used by an instruction outside the function");
    };
    for (auto &c : f_.constants) check(*c);
    for (auto &b : f_.blocks) {
      for (auto &p : b->params) check(*p);
      for (auto &i : b->insts) check(*i);
    }
  }

  void block(const BasicBlock &b) {
    if (b.parent != &f_) fail(b.name + " belongs to another function");
    if (b.unwind && (b.unwind->parent != &f_ || !b.unwind->params.empty()))
      fail(b.name + " unwinds to a block of another function or with parameters");
    for (size_t k = 0; k < b.params.size(); ++k)
      if (b.params[k]->block != &b || b.params[k]->index != k) fail(b.name + " has a parameter with a wrong place");
    if (b.insts.empty() || !isTerminator(b.insts.back()->op)) {
      fail(b.name + " does not end with a terminator");
      return;
    }
    for (size_t k = 0; k < b.insts.size(); ++k) {
      const Instruction &i = *b.insts[k];
      if (i.parent != &b) fail(std::string(opName(i.op)) + " has a wrong parent block in " + b.name);
      cur_ = &i;
      if (isTerminator(i.op) && k + 1 != b.insts.size()) fail(b.name + " has a terminator before its end");
      inst(i, k);
    }
    cur_ = nullptr;
  }

  // `v`, used by the instruction at `pos` of block `at` (or by its branch),
  // must be defined in this function and dominate the use.
  void use(const Value *v, const BasicBlock *at, size_t pos) {
    auto it = defined_.find(v);
    if (it == defined_.end()) {
      fail("a value used in " + at->name + " is not defined in the function");
      return;
    }
    const BasicBlock *def = it->second.block;
    if (!def || !cfg_.reachable(at)) return; // constants; unreachable code has no order to check
    if (def == at) {
      if (v->kind() != Value::Kind::Param && it->second.index >= pos)
        fail("a value used in " + at->name + " is defined after its use");
      return;
    }
    if (!cfg_.reachable(def) || !cfg_.dominates(def, at)) {
      fail("a value used in " + at->name + " does not dominate it");
      return;
    }
    // The unwind edge leaves from the operation that failed, before the rest
    // of the block: the block's instructions do not reach its unwind block.
    if (v->kind() == Value::Kind::Instruction && def->unwind && cfg_.dominates(def->unwind, at))
      fail("a value used in " + at->name + " comes from a block that unwinds before it");
  }

  void expect(const Instruction &i, size_t n, std::initializer_list<Type> types) {
    const auto &ops = i.operands();
    if (ops.size() != n) {
      fail(std::string(opName(i.op)) + " has " + std::to_string(ops.size()) + " operands");
      return;
    }
    size_t k = 0;
    for (Type t : types) {
      if (k < ops.size() && ops[k]->type() != t) fail(std::string(opName(i.op)) + " operand " + std::to_string(k) +
                                                      " is not " + typeName(t));
      ++k;
    }
  }

  void needsVar(const Instruction &i) {
    if (!i.var) {
      fail(std::string(opName(i.op)) + " has no variable");
      return;
    }
    bool own = false;
    for (auto &v : f_.vars) own |= v.get() == i.var;
    for (auto &v : m_.statics) own |= v.get() == i.var;
    if (!own) fail(std::string(opName(i.op)) + " uses a variable of another function");
  }

  // Items, and in a call, a send or an array literal also args (checked in inst).
  void allItems(const Instruction &i) {
    bool lists = i.op == Op::Call || i.op == Op::Send || i.op == Op::ArrayLit;
    for (Value *v : i.operands())
      if (v->type() != Type::Item && !(lists && v->type() == Type::Args))
        fail(std::string(opName(i.op)) + " takes items only");
  }

  // A generator operation's variable: the enumerator, the resume function's parameter 0.
  void enumeratorVar(const Instruction &i) {
    if (!i.var || i.var->kind != Var::Kind::Frame || i.var->index != 0) fail("a generator's enumerator is not param 0");
  }

  void inst(const Instruction &i, size_t pos) {
    const BasicBlock *at = i.parent;
    for (Value *v : i.operands()) use(v, at, pos);
    bool lists = i.op == Op::Call || i.op == Op::Send || i.op == Op::ArrayLit;
    for (size_t k = 0; k < i.operands().size(); ++k) {
      bool last = k + 1 == i.operands().size();
      if (i.operand(k)->type() == Type::Args && (!lists || !last || (i.op == Op::Send && k == 0)))
        fail("args other than the last argument of a call, a send or an array literal");
    }
    if (i.op == Op::ArgsRest) {
      if (f_.kind != Function::Kind::Program) fail("args.rest outside a function");
      // Its one use comes right after the operands it was generated among: nothing joins it at a block parameter.
      if (!i.hasOneUse() || i.users().front()->parent != at) fail("args.rest not used once in its block");
    }
    switch (i.op) {
    case Op::NumDefaultDec: case Op::ArgsRest: expect(i, 0, {}); break;
    case Op::Load: case Op::Barrier: case Op::RefVar:
      needsVar(i);
      expect(i, 0, {});
      break;
    case Op::Append:
      if (i.var && (i.var->kind != Var::Kind::Frame || i.access != Access::Direct))
        fail("append on a variable other than a direct frame LOCAL");
      [[fallthrough]];
    case Op::Store:
      needsVar(i);
      expect(i, 1, {Type::Item});
      break;
    case Op::Unary: case Op::Truth: case Op::EnumLen: case Op::SwitchCheck:
      expect(i, 1, {Type::Item});
      break;
    case Op::Binary: case Op::Index: case Op::SwitchMatch: expect(i, 2, {Type::Item, Type::Item}); break;
    case Op::IndexSet: case Op::ForTest: expect(i, 3, {Type::Item, Type::Item, Type::Item}); break;
    case Op::Call:
      allItems(i);
      if (i.callee.function && i.callee.function->kind != Function::Kind::Program)
        fail("call to a non-program function");
      if (!i.callee.function && i.callee.name.empty()) fail("call without a callee");
      break;
    case Op::Send:
      allItems(i);
      if (i.operands().empty()) fail("send without an object");
      if (i.message.empty()) fail("send without a message");
      break;
    case Op::ArrayLit: allItems(i); break;
    case Op::ClassNew: expect(i, 0, {}); break;
    case Op::FieldGet: case Op::FieldSet:
      allItems(i);
      if (i.operands().size() > (i.op == Op::FieldSet ? 2u : 1u) || (i.op == Op::FieldSet && i.operands().empty()))
        fail("field operation with wrong operands");
      if (i.message.empty()) fail("field without a name");
      break;
    case Op::MvGet: case Op::MvField: case Op::MvRef: case Op::MvPrivate: case Op::MvPublic: case Op::MvParam:
      expect(i, 0, {});
      if (i.message.empty()) fail("memory variable without a name");
      break;
    case Op::MvSet:
      expect(i, 1, {Type::Item});
      if (i.message.empty()) fail("memory variable without a name");
      break;
    case Op::MacroGet: case Op::MacroPrivate: case Op::MacroPublic: case Op::MacroText:
      expect(i, 1, {Type::Item});
      break;
    case Op::MacroSet: expect(i, 2, {Type::Item, Type::Item}); break;
    case Op::MacroCall:
      allItems(i);
      if (i.operands().empty()) fail("macro call without a text");
      break;
    case Op::HashLit:
      allItems(i);
      if (i.operands().size() % 2) fail("hash with a key without a value");
      break;
    case Op::MakeBlock:
      expect(i, 0, {});
      if (!i.blockFn || i.blockFn->kind != Function::Kind::Block) fail("block without a block function");
      for (Var *c : i.captures) {
        bool ok = false;
        for (auto &v : f_.vars) ok |= v.get() == c && (c->kind == Var::Kind::Capture || c->shared);
        if (!ok) fail("a block captures something other than a shared variable of this function");
      }
      break;
    case Op::ParamsRef:
      expect(i, 0, {});
      for (Var *c : i.captures) {
        bool ok = false;
        for (auto &v : f_.vars) ok |= v.get() == c && c->param;
        if (!ok) fail("params.ref tests something other than a parameter of this function");
      }
      break;
    case Op::SeqCatch: case Op::SeqValue: case Op::SeqSuspend: expect(i, 0, {}); break;
    case Op::SeqWith: case Op::SeqRestore: expect(i, 1, {Type::Item}); break;
    case Op::BoxI64: expect(i, 1, {Type::I64}); break;
    case Op::BoxBool: expect(i, 1, {Type::Bool}); break;
    case Op::UnboxI64: case Op::UnboxBool: expect(i, 1, {Type::Item}); break;
    case Op::GenNew:
      allItems(i);
      if (!i.callee.function) fail("gen.new without a resume function");
      break;
    case Op::GenRestore: case Op::GenDone:
      expect(i, 0, {});
      enumeratorVar(i);
      break;
    case Op::SeqResume: expect(i, 2, {Type::I64, Type::Item}); break;
    case Op::EachKey: expect(i, 2, {Type::Item, Type::I64}); break;
    case Op::EachSource: expect(i, 2, {Type::Item, Type::Bool}); break;
    case Op::SubstCheck: { // nothing between the operation, its check and the subst.value its branch leads to
      expect(i, 1, {Type::Item});
      const auto &insts = i.parent->insts;
      if (pos == 0 || i.operand(0) != insts[pos - 1].get()) fail("subst.check does not follow its operation");
      const Instruction *br = pos + 1 < insts.size() ? insts[pos + 1].get() : nullptr;
      const BasicBlock *to = br && br->op == Op::CondBr && br->operand(0) == &i ? br->targets[0].block : nullptr;
      if (!to || !br->targets[0].args.empty() || to->insts.empty() || to->insts[0]->op != Op::SubstValue ||
          to->insts[0]->operand(0) != i.operand(0))
        fail("subst.check is not followed by the branch to its subst.value");
      break;
    }
    case Op::SubstValue: // first in a block reached only from the operation's check
      expect(i, 1, {Type::Item});
      if (pos != 0 || (cfg_.reachable(i.parent) && (cfg_.preds(i.parent).size() != 1 ||
                                                     cfg_.preds(i.parent)[0] != definingBlock(i.operand(0)))))
        fail("subst.value is not where the check of its operation leads");
      break;
    case Op::CallForward:
      expect(i, 0, {});
      if (!i.callee.function || i.callee.function->kind != Function::Kind::Program ||
          i.callee.function->numParams != f_.numParams)
        fail("call.forward to a function that does not take this one's parameters");
      break;
    case Op::EachInCell: case Op::EachEnter: case Op::EachMove: case Op::EachLeave:
      needsVar(i);
      if (i.var && i.var->kind == Var::Kind::Capture) fail("a FOR EACH over a captured variable");
      if (i.op == Op::EachInCell) expect(i, 0, {});
      else if (i.op == Op::EachEnter) expect(i, 2, {Type::Bool, Type::Item});
      else if (i.op == Op::EachMove) expect(i, 2, {Type::Bool, Type::I64});
      else expect(i, 2, {Type::Bool, Type::Item});
      break;
    case Op::RefIndex: expect(i, 2, {Type::Item, Type::Item}); break;
    case Op::IntAdd: case Op::IntSub: case Op::IntCmp: expect(i, 2, {Type::I64, Type::I64}); break;
    case Op::BoolAnd: expect(i, 2, {Type::Bool, Type::Bool}); break;
    case Op::FromBool: case Op::BoolNot: expect(i, 1, {Type::Bool}); break;
    case Op::Br: case Op::CondBr: {
      size_t n = i.op == Op::Br ? 1 : 2;
      if (i.targets.size() != n) fail(std::string(opName(i.op)) + " has a wrong number of targets");
      if (i.op == Op::CondBr) expect(i, 1, {Type::Bool});
      else expect(i, 0, {});
      for (const Target &t : i.targets) {
        if (!owned_.count(t.block)) {
          fail(at->name + " branches to a block of another function");
          continue;
        }
        if (t.args.size() != t.block->params.size()) fail(at->name + " passes a wrong number of arguments to " +
                                                          t.block->name);
        for (size_t k = 0; k < t.args.size() && k < t.block->params.size(); ++k) {
          if (t.args[k]->type() != t.block->params[k]->type())
            fail(at->name + " passes a wrong type to " + t.block->name);
          if (t.args[k]->type() == Type::Args) fail(at->name + " passes args to a block");
          use(t.args[k], at, pos);
        }
      }
      break;
    }
    case Op::GenYield:
      expect(i, 1, {Type::Item});
      enumeratorVar(i);
      if (i.targets.size() != 1 || !owned_.count(i.targets[0].block) || !i.targets[0].args.empty() ||
          !i.targets[0].block->params.empty())
        fail("gen.yield resumes in a block of its own without arguments");
      break;
    case Op::Ret:
      if (i.operands().size() > 1 || (i.operands().size() == 1 && i.operand(0)->type() != Type::Item))
        fail("ret takes at most one item");
      break;
    }
  }

  const Module &m_;
  const Function &f_;
  CFG cfg_;
  std::unordered_set<const BasicBlock *> owned_;
  std::unordered_map<const Value *, Def> defined_;
  const Instruction *cur_ = nullptr; // the instruction being checked, for the line in messages
  std::string error_;
};

} // namespace

std::string verify(const Module &m, const Function &f) {
  std::string e = Verifier(m, f).run();
  return e.empty() ? e : e + "\n" + print(f);
}

std::string verify(const Module &m) {
  for (auto &f : m.functions)
    if (std::string e = verify(m, *f); !e.empty()) return e;
  return {};
}

} // namespace xb::xir
