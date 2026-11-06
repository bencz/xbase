// Construction of xIR instructions and constants.
#include "xir/Builder.h"

namespace xb::xir {

Instruction *Builder::make(Op op, Type t, std::initializer_list<Value *> ops) {
  bb_->insts.push_back(std::make_unique<Instruction>(op, t, bb_));
  Instruction *i = bb_->insts.back().get();
  i->loc = loc_;
  for (Value *v : ops) i->addOperand(v);
  return i;
}

Constant *Builder::constant(Constant::Lit lit, Type t) {
  fn_->constants.push_back(std::make_unique<Constant>(lit, t));
  return fn_->constants.back().get();
}

Constant *Builder::nil() { return constant(Constant::Lit::Nil, Type::Item); }

Constant *Builder::logical(bool v) {
  Constant *c = constant(Constant::Lit::Logical, Type::Item);
  c->logical = v;
  return c;
}

Constant *Builder::integer(int64_t v, uint16_t width) {
  Constant *c = constant(Constant::Lit::Integer, Type::Item);
  c->ival = v;
  c->width = width;
  return c;
}

Constant *Builder::number(const NumberExpr &n) {
  if (n.isInt) return integer(n.ival, n.width);
  Constant *c = constant(Constant::Lit::Double, Type::Item);
  c->dval = n.dval;
  c->width = n.width;
  c->dec = n.dec;
  return c;
}

Constant *Builder::string(const std::string &s) {
  Constant *c = constant(Constant::Lit::String, Type::Item);
  c->str = s;
  return c;
}

Constant *Builder::boolean(bool v) {
  Constant *c = constant(Constant::Lit::Bool, Type::Bool);
  c->logical = v;
  return c;
}

Constant *Builder::i64(int64_t v) {
  Constant *c = constant(Constant::Lit::I64, Type::I64);
  c->ival = v;
  return c;
}

Instruction *Builder::numDefaultDec(double v) {
  Instruction *i = make(Op::NumDefaultDec, Type::Item);
  i->imm = v;
  return i;
}

Instruction *Builder::load(Var *v, Access a) {
  Instruction *i = make(Op::Load, Type::Item);
  i->var = v;
  i->access = a;
  return i;
}

Instruction *Builder::store(Var *v, Access a, Value *value) {
  Instruction *i = make(Op::Store, Type::Void, {value});
  i->var = v;
  i->access = a;
  return i;
}

Instruction *Builder::barrier(Var *v, Access a) {
  Instruction *i = make(Op::Barrier, Type::Void);
  i->var = v;
  i->access = a;
  return i;
}

Instruction *Builder::unary(UnOp op, Value *a) {
  Instruction *i = make(Op::Unary, Type::Item, {a});
  i->unop = op;
  return i;
}

Instruction *Builder::binary(BinOp op, Value *a, Value *b) {
  Instruction *i = make(Op::Binary, Type::Item, {a, b});
  i->binop = op;
  return i;
}

Instruction *Builder::truth(Value *a) { return make(Op::Truth, Type::Bool, {a}); }
Instruction *Builder::fromBool(Value *b) { return make(Op::FromBool, Type::Item, {b}); }

Instruction *Builder::call(Callee c, const std::vector<Value *> &args) {
  Instruction *i = make(Op::Call, Type::Item);
  for (Value *a : args) i->addOperand(a);
  i->callee = std::move(c);
  return i;
}

Instruction *Builder::send(std::string message, Value *object, const std::vector<Value *> &args) {
  Instruction *i = make(Op::Send, Type::Item, {object});
  for (Value *a : args) i->addOperand(a);
  i->message = std::move(message);
  return i;
}

Instruction *Builder::arrayLit(const std::vector<Value *> &elems) {
  Instruction *i = make(Op::ArrayLit, Type::Item);
  for (Value *e : elems) i->addOperand(e);
  return i;
}

Instruction *Builder::index(Value *base, Value *idx) { return make(Op::Index, Type::Item, {base, idx}); }

Instruction *Builder::indexSet(Value *base, Value *idx, Value *value) {
  return make(Op::IndexSet, Type::Void, {base, idx, value});
}

Instruction *Builder::makeBlock(Function *blockFn, std::vector<Var *> captures) {
  Instruction *i = make(Op::MakeBlock, Type::Item);
  i->blockFn = blockFn;
  i->captures = std::move(captures);
  return i;
}

Instruction *Builder::paramsRef(std::vector<Var *> params) {
  Instruction *i = make(Op::ParamsRef, Type::Bool);
  i->captures = std::move(params);
  return i;
}

Instruction *Builder::seqCatch() { return make(Op::SeqCatch, Type::Bool); }
Instruction *Builder::seqValue() { return make(Op::SeqValue, Type::Item); }

Instruction *Builder::callForward(Function *callee) {
  Instruction *i = make(Op::CallForward, Type::Item);
  i->callee.function = callee;
  i->callee.name = callee->name;
  return i;
}

Instruction *Builder::forTest(Value *counter, Value *limit, Value *step) {
  return make(Op::ForTest, Type::Bool, {counter, limit, step});
}

Instruction *Builder::enumLen(Value *coll) { return make(Op::EnumLen, Type::I64, {coll}); }
Instruction *Builder::eachInCell(Var *v) {
  Instruction *i = make(Op::EachInCell, Type::Bool);
  i->var = v;
  return i;
}

Instruction *Builder::eachEnter(Var *v, Value *inCell, Value *coll) {
  Instruction *i = make(Op::EachEnter, Type::Item, {inCell, coll});
  i->var = v;
  return i;
}

Instruction *Builder::eachMove(Var *v, Value *inCell, Value *idx) {
  Instruction *i = make(Op::EachMove, Type::Void, {inCell, idx});
  i->var = v;
  return i;
}

Instruction *Builder::eachLeave(Var *v, Value *inCell, Value *saved) {
  Instruction *i = make(Op::EachLeave, Type::Void, {inCell, saved});
  i->var = v;
  return i;
}

Instruction *Builder::refVar(Var *v) {
  Instruction *i = make(Op::RefVar, Type::Item);
  i->var = v;
  return i;
}

Instruction *Builder::refIndex(Value *base, Value *idx) { return make(Op::RefIndex, Type::Item, {base, idx}); }
Instruction *Builder::switchCheck(Value *v) { return make(Op::SwitchCheck, Type::Item, {v}); }
Instruction *Builder::switchMatch(Value *v, Value *c) { return make(Op::SwitchMatch, Type::Bool, {v, c}); }
Instruction *Builder::intAdd(Value *a, Value *b) { return make(Op::IntAdd, Type::I64, {a, b}); }
Instruction *Builder::intSub(Value *a, Value *b) { return make(Op::IntSub, Type::I64, {a, b}); }

Instruction *Builder::intCmp(IntPred p, Value *a, Value *b) {
  Instruction *i = make(Op::IntCmp, Type::Bool, {a, b});
  i->pred = p;
  return i;
}

Instruction *Builder::boolAnd(Value *a, Value *b) { return make(Op::BoolAnd, Type::Bool, {a, b}); }
Instruction *Builder::boolNot(Value *a) { return make(Op::BoolNot, Type::Bool, {a}); }

Instruction *Builder::br(BasicBlock *to, const std::vector<Value *> &args) {
  Instruction *i = make(Op::Br, Type::Void);
  i->targets.push_back({to, {}});
  for (Value *a : args) i->addTargetArg(0, a);
  return i;
}

Instruction *Builder::jump(BasicBlock *from, BasicBlock *to, const std::vector<Value *> &args) {
  std::unique_ptr<Instruction> old = from->takeTerminator();
  setBlock(from);
  setLoc(old->loc);
  return br(to, args);
}

Instruction *Builder::condBr(Value *cond, BasicBlock *t, BasicBlock *f, const std::vector<Value *> &targs,
                             const std::vector<Value *> &fargs) {
  Instruction *i = make(Op::CondBr, Type::Void, {cond});
  i->targets.push_back({t, {}});
  i->targets.push_back({f, {}});
  for (Value *a : targs) i->addTargetArg(0, a);
  for (Value *a : fargs) i->addTargetArg(1, a);
  return i;
}

Instruction *Builder::ret(Value *v) {
  Instruction *i = make(Op::Ret, Type::Void);
  if (v) i->addOperand(v);
  return i;
}

} // namespace xb::xir
