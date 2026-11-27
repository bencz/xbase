// The xIR data structures: names, use lists, blocks and functions.
#include "xir/IR.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace xb::xir {

const BasicBlock *definingBlock(const Value *v) {
  switch (v->kind()) {
  case Value::Kind::Constant: return nullptr;
  case Value::Kind::Param: return static_cast<const BlockParam *>(v)->block;
  case Value::Kind::Instruction: return static_cast<const Instruction *>(v)->parent;
  }
  return nullptr;
}

size_t instructionCount(const std::vector<BasicBlock *> &blocks) {
  size_t n = 0;
  for (const BasicBlock *b : blocks) n += b->insts.size();
  return n;
}

size_t instructionCount(const Function &f) {
  size_t n = 0;
  for (auto &b : f.blocks) n += b->insts.size();
  return n;
}

const Function *firstProgramFunction(const Module &m) {
  for (auto &f : m.functions)
    if (f->kind == Function::Kind::Program && !f->startup) return f.get();
  return nullptr;
}

bool hasDirectEntry(const Function &f) {
  // A codeblock with a NAME.plain body has none (that body's direct entry is the block's); one without parameters
  // gains nothing from one (no words to pass), and its function would become a thunk.
  if (f.kind == Function::Kind::Block && (f.plainTwin || f.numParams == 0)) return false;
  return (f.kind == Function::Kind::Program || f.kind == Function::Kind::Block) && !f.readsArgs && !f.byRefBody &&
         !f.usesRest && !f.carried;
}

bool isEvalCall(const Instruction &i) {
  return i.op == Op::Call && !i.callee.function && i.callee.native && i.callee.name == "EVAL" && !i.hasRest() &&
         !i.operands().empty();
}

bool makesCall(const Instruction &i) {
  return i.op == Op::Call || i.op == Op::CallForward || i.op == Op::Send || i.op == Op::SendMacro ||
         i.op == Op::MacroCall;
}

bool isDirectCall(const Instruction &i) {
  return i.op == Op::Call && i.callee.function && hasDirectEntry(*i.callee.function) && !i.hasRest();
}

bool sameValue(const Value *a, const Value *b) {
  if (a == b) return true;
  const Constant *x = asConstant(a), *y = asConstant(b);
  if (!x || !y || x->lit != y->lit || x->type() != y->type()) return false;
  switch (x->lit) {
  case Constant::Lit::Nil: return true;
  case Constant::Lit::Logical: case Constant::Lit::Bool: return x->logical == y->logical;
  case Constant::Lit::Integer: case Constant::Lit::I64: return x->ival == y->ival && x->width == y->width;
  case Constant::Lit::Double: // the same bits: 0.0 and -0.0 differ in display
    return std::memcmp(&x->dval, &y->dval, sizeof x->dval) == 0 && x->width == y->width && x->dec == y->dec;
  case Constant::Lit::String: return x->str == y->str;
  }
  return false;
}

const char *typeName(Type t) {
  switch (t) {
  case Type::Void: return "void";
  case Type::Bool: return "bool";
  case Type::I64: return "i64";
  case Type::Item: return "item";
  case Type::Args: return "args";
  }
  return "?";
}

bool Instruction::hasRest() const { return !operands_.empty() && operands_.back()->type() == Type::Args; }

uint16_t Instruction::effects() const {
  uint16_t e = opInfo(op).effects;
  if (hasRest()) e |= Fails | RunsCode | ReadsHeap | WritesHeap | Allocates; // xb_args_rest
  if (op == Op::TableIndex && var && var->kind == Var::Kind::Frame) e |= WritesVar | Allocates; // the call's own array
  return e;
}

bool Instruction::mayFail() const {
  uint16_t e = effects();
  return (e & Fails) || ((e & FailsByRef) && access != Access::Direct);
}

void Instruction::copyData(const Instruction &from) {
  unop = from.unop;
  binop = from.binop;
  byOne = from.byOne;
  grows = from.grows;
  moves = from.moves;
  pred = from.pred;
  imm = from.imm;
  var = from.var;
  access = from.access;
  callee = from.callee;
  message = from.message;
  index = from.index;
  classIndex = from.classIndex;
  superSend = from.superSend;
  dispatched = from.dispatched;
  blockFn = from.blockFn;
  captures = from.captures;
  valueCells = from.valueCells;
  alias = from.alias;
}

void Instruction::removeUse(Value *v, Instruction *user) {
  auto it = std::find(v->users_.begin(), v->users_.end(), user);
  if (it == v->users_.end()) { // a use list out of step with the operands: a compiler bug
    std::fputs("xbc: internal error: xIR use list corrupted\n", stderr);
    std::abort();
  }
  v->users_.erase(it);
}

Instruction::~Instruction() { dropOperands(); }

void Instruction::addOperand(Value *v) {
  operands_.push_back(v);
  v->users_.push_back(this);
}

void Instruction::dropOperands() {
  for (Value *v : operands_) removeUse(v, this);
  operands_.clear();
  for (Target &t : targets)
    for (Value *v : t.args) removeUse(v, this);
  targets.clear();
}

void Instruction::addTargetArg(size_t target, Value *v) {
  targets[target].args.push_back(v);
  v->users_.push_back(this);
}

void Value::replaceAllUsesWith(Value *with) {
  std::vector<Instruction *> users = users_;
  std::sort(users.begin(), users.end());
  users.erase(std::unique(users.begin(), users.end()), users.end());
  for (Instruction *u : users) u->replaceUses(this, with);
}

void Instruction::replaceUses(Value *from, Value *to) {
  auto swap = [&](Value *&v) {
    if (v != from) return;
    removeUse(from, this);
    v = to;
    to->users_.push_back(this);
  };
  for (Value *&v : operands_) swap(v);
  for (Target &t : targets)
    for (Value *&v : t.args) swap(v);
}

void Instruction::setTarget(size_t target, BasicBlock *to, std::vector<Value *> args) {
  for (Value *v : targets[target].args) removeUse(v, this);
  targets[target].block = to;
  targets[target].args = std::move(args);
  for (Value *v : targets[target].args) v->users_.push_back(this);
}

void Instruction::removeTargetArg(size_t target, size_t k) {
  auto &args = targets[target].args;
  removeUse(args[k], this);
  args.erase(args.begin() + (ptrdiff_t)k);
}

BlockParam *BasicBlock::addParam(Type t) {
  params.push_back(std::make_unique<BlockParam>(this, t, (uint32_t)params.size()));
  return params.back().get();
}

void BasicBlock::removeArgs(size_t k, const std::vector<Instruction *> &branches) {
  for (Instruction *t : branches)
    for (size_t e = 0; e < t->targets.size(); ++e)
      if (t->targets[e].block == this) t->removeTargetArg(e, k);
}

void BasicBlock::eraseParam(size_t k) {
  if (!params[k]->users().empty()) {
    std::fputs("xbc: internal error: xIR parameter removed while in use\n", stderr);
    std::abort();
  }
  params.erase(params.begin() + (ptrdiff_t)k);
  for (size_t i = k; i < params.size(); ++i) params[i]->index = (uint32_t)i;
}

std::unique_ptr<Instruction> BasicBlock::takeTerminator() {
  std::unique_ptr<Instruction> t = std::move(insts.back());
  insts.pop_back();
  t->dropOperands();
  return t;
}

size_t BasicBlock::indexOf(const Instruction *i) const {
  size_t k = 0;
  while (insts[k].get() != i) ++k;
  return k;
}

const Instruction *BasicBlock::paramGuard() const {
  if (insts.empty() || insts.front()->op != Op::SubstCheck || insts.front()->imm == 0) return nullptr;
  const BlockParam *p = asParam(insts.front()->operand(0));
  return p && p->block == this ? insts.front().get() : nullptr;
}

Instruction *BasicBlock::terminator() const {
  if (insts.empty() || !isTerminator(insts.back()->op)) return nullptr;
  return insts.back().get();
}

size_t BasicBlock::numSuccessors() const {
  Instruction *t = terminator();
  return (t ? t->targets.size() : 0) + (unwind ? 1 : 0);
}

BasicBlock *BasicBlock::successor(size_t k) const {
  Instruction *t = terminator();
  size_t targets = t ? t->targets.size() : 0;
  return k < targets ? t->targets[k].block : unwind;
}

Instruction *versioning(const Function &f) {
  Instruction *t = f.entry()->terminator();
  if (!t || t->op != Op::CondBr || !asInstruction(t->operand(0), Op::ParamsRef)) return nullptr;
  return t;
}

Function *referenceBody(const Instruction &versioning) {
  return versioning.targets[0].block->insts.front()->callee.function;
}

// Values are freed in an arbitrary order (blocks, then constants), so every
// use is undone first: no destructor touches a value already freed.
Function::~Function() {
  for (auto &b : blocks)
    for (auto &i : b->insts) i->dropOperands();
}

// Names are unique in the function (a second "while.body" is "while.body.2"),
// so dumps and messages name blocks unambiguously.
BasicBlock *Function::addBlock(const std::string &blockName) {
  unsigned n = ++blockNames_[blockName];
  blocks.push_back(std::make_unique<BasicBlock>(this, n == 1 ? blockName : blockName + "." + std::to_string(n)));
  return blocks.back().get();
}

Var *Function::addVar(Var::Kind k, uint32_t index, std::string varName) {
  vars.push_back(std::make_unique<Var>(Var{k, index, false, false, std::move(varName), false, {}, 0}));
  return vars.back().get();
}

Var *Function::addFrameVar(std::string varName) {
  uint32_t slot = 0;
  for (auto &v : vars)
    if (v->kind == Var::Kind::Frame) slot = std::max(slot, v->index + 1);
  return addVar(Var::Kind::Frame, slot, std::move(varName));
}

void Function::renumber() const {
  uint32_t next = 0;
  for (auto &b : blocks) {
    for (auto &p : b->params) p->id = next++;
    for (auto &i : b->insts) i->id = next++;
  }
}

} // namespace xb::xir
