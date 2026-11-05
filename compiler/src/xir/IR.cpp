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

bool Instruction::mayFail() const {
  uint16_t e = opInfo(op).effects;
  return (e & Fails) || ((e & FailsByRef) && access != Access::Direct);
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
