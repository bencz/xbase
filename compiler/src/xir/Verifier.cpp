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

  void block(const BasicBlock &b) {
    if (b.parent != &f_) fail(b.name + " belongs to another function");
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
    if (!cfg_.reachable(def) || !cfg_.dominates(def, at)) fail("a value used in " + at->name + " does not dominate it");
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

  void allItems(const Instruction &i) {
    for (Value *v : i.operands())
      if (v->type() != Type::Item) fail(std::string(opName(i.op)) + " takes items only");
  }

  void inst(const Instruction &i, size_t pos) {
    const BasicBlock *at = i.parent;
    for (Value *v : i.operands()) use(v, at, pos);
    switch (i.op) {
    case Op::NumDefaultDec: expect(i, 0, {}); break;
    case Op::Load: case Op::Barrier: case Op::RefVar:
      needsVar(i);
      expect(i, 0, {});
      break;
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
    case Op::ArrayLit: allItems(i); break;
    case Op::MakeBlock:
      expect(i, 0, {});
      if (!i.blockFn || i.blockFn->kind != Function::Kind::Block) fail("block without a block function");
      for (Var *c : i.captures) {
        bool ok = false;
        for (auto &v : f_.vars) ok |= v.get() == c && (c->kind == Var::Kind::Capture || c->shared);
        if (!ok) fail("a block captures something other than a shared variable of this function");
      }
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
          use(t.args[k], at, pos);
        }
      }
      break;
    }
    case Op::Ret:
      if (i.operands().size() > 1 || (i.operands().size() == 1 && i.operand(0)->type() != Type::Item))
        fail("ret takes at most one item");
      break;
    }
    if (i.unwind && !owned_.count(i.unwind)) fail("unwind to a block of another function");
    if (i.unwind && !i.mayFail()) fail(std::string(opName(i.op)) + " cannot fail but has an unwind block");
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

std::string verify(const Module &m) {
  for (auto &f : m.functions) {
    std::string e = Verifier(m, *f).run();
    if (!e.empty()) return e + "\n" + print(*f);
  }
  return {};
}

} // namespace xb::xir
