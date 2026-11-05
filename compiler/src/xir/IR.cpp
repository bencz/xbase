// The xIR data structures: names, use lists, blocks and functions.
#include "xir/IR.h"

#include <algorithm>

namespace xb::xir {

const char *typeName(Type t) {
  switch (t) {
  case Type::Void: return "void";
  case Type::Bool: return "bool";
  case Type::I64: return "i64";
  case Type::Item: return "item";
  }
  return "?";
}

const char *opName(Op op) {
  switch (op) {
  case Op::NumDefaultDec: return "num.default_dec";
  case Op::Load: return "load";
  case Op::Store: return "store";
  case Op::Barrier: return "barrier";
  case Op::Unary: return "unary";
  case Op::Binary: return "binary";
  case Op::Truth: return "truth";
  case Op::FromBool: return "from_bool";
  case Op::Call: return "call";
  case Op::ArrayLit: return "array";
  case Op::Index: return "index";
  case Op::IndexSet: return "index.set";
  case Op::MakeBlock: return "block";
  case Op::ForTest: return "for.test";
  case Op::EnumLen: return "enum.len";
  case Op::EachInCell: return "each.in_cell";
  case Op::EachEnter: return "each.enter";
  case Op::EachMove: return "each.move";
  case Op::EachLeave: return "each.leave";
  case Op::RefVar: return "ref";
  case Op::RefIndex: return "ref.index";
  case Op::SwitchCheck: return "switch.check";
  case Op::SwitchMatch: return "switch.match";
  case Op::IntAdd: return "i.add";
  case Op::IntSub: return "i.sub";
  case Op::IntCmp: return "i.cmp";
  case Op::BoolAnd: return "and";
  case Op::BoolNot: return "not";
  case Op::Br: return "br";
  case Op::CondBr: return "cond_br";
  case Op::Ret: return "ret";
  }
  return "?";
}

bool isTerminator(Op op) { return op == Op::Br || op == Op::CondBr || op == Op::Ret; }

bool Instruction::mayFail() const {
  switch (op) {
  case Op::Unary: case Op::Binary: case Op::Truth: case Op::Call: case Op::Index: case Op::IndexSet:
  case Op::ForTest: case Op::EnumLen: case Op::EachMove: case Op::SwitchCheck:
  case Op::RefIndex:
    return true;
  case Op::Load: case Op::Store: // through a reference: a bound error when the array shrank
    return access != Access::Direct;
  default:
    return false;
  }
}

Instruction::~Instruction() { dropOperands(); }

void Instruction::addOperand(Value *v) {
  operands_.push_back(v);
  v->users_.push_back(this);
}

void Instruction::dropOperands() {
  for (Value *v : operands_) {
    auto &users = v->users_;
    users.erase(std::find(users.begin(), users.end(), this));
  }
  operands_.clear();
  for (Target &t : targets)
    for (Value *v : t.args) {
      auto &users = v->users_;
      users.erase(std::find(users.begin(), users.end(), this));
    }
  targets.clear();
}

void Instruction::addTargetArg(size_t target, Value *v) {
  targets[target].args.push_back(v);
  v->users_.push_back(this);
}

BlockParam *BasicBlock::addParam(Type t) {
  params.push_back(std::make_unique<BlockParam>(this, t, (uint32_t)params.size()));
  return params.back().get();
}

Instruction *BasicBlock::terminator() const {
  if (insts.empty() || !isTerminator(insts.back()->op)) return nullptr;
  return insts.back().get();
}

size_t BasicBlock::numSuccessors() const {
  Instruction *t = terminator();
  return t ? t->targets.size() : 0;
}

BasicBlock *BasicBlock::successor(size_t k) const { return insts.back()->targets[k].block; }

// Values are freed in an arbitrary order (blocks, then constants), so every
// use is undone first: no destructor touches a value already freed.
Function::~Function() {
  for (auto &b : blocks)
    for (auto &i : b->insts) i->dropOperands();
}

BasicBlock *Function::addBlock(const std::string &blockName) {
  blocks.push_back(std::make_unique<BasicBlock>(this, blockName));
  return blocks.back().get();
}

Var *Function::addVar(Var::Kind k, uint32_t index, std::string varName) {
  vars.push_back(std::make_unique<Var>(Var{k, index, false, false, std::move(varName)}));
  return vars.back().get();
}

uint32_t Function::numFrameVars() const {
  uint32_t n = 0;
  for (auto &v : vars)
    if (v->kind == Var::Kind::Frame) ++n;
  return n;
}

void Function::renumber() const {
  uint32_t next = 0;
  for (auto &b : blocks) {
    for (auto &p : b->params) p->id = next++;
    for (auto &i : b->insts) i->id = next++;
  }
}

} // namespace xb::xir
