#pragma once

#include "xir/IR.h"

namespace xb::xir {

// Creates instructions at the end of the current block, and constants of the
// current function.
class Builder {
public:
  explicit Builder(Function *f) : fn_(f) {}

  // While generating code, the blocks it fills unwind to the block given here
  // (setUnwind); a builder used by a pass leaves unwind blocks as they are.
  void setBlock(BasicBlock *b) {
    bb_ = b;
    if (assignUnwind_) b->unwind = unwind_;
  }
  void setUnwind(BasicBlock *u) {
    assignUnwind_ = true;
    unwind_ = u;
  }
  BasicBlock *unwindBlock() const { return unwind_; }
  bool terminated() const { return bb_->terminator() != nullptr; }
  void setLoc(SourceLoc l) { loc_ = l; }
  // The line every instruction made from here on carries (the statement's,
  // Stmt::line), whatever setLoc says; 0: setLoc's.
  void setLine(uint32_t line) { line_ = line; }
  uint32_t line() const { return line_; }

  // Constants
  Constant *nil();
  Constant *logical(bool v);
  Constant *integer(int64_t v, uint16_t width);
  Constant *number(const NumberExpr &n);
  Constant *string(const std::string &s);
  Constant *boolean(bool v);
  Constant *i64(int64_t v);

  // Values and effects (see Op in IR.h)
  Instruction *numDefaultDec(double v);
  Instruction *load(Var *v, Access a);
  Instruction *store(Var *v, Access a, Value *value);
  Instruction *barrier(Var *v, Access a);
  Instruction *unary(UnOp op, Value *a);
  Instruction *binary(BinOp op, Value *a, Value *b);
  Instruction *stepByOne(bool up, Value *a); // ++ (up) or --: a binary add or sub of 1, byOne
  Instruction *memvar(Op op, const std::string &name, Value *value = nullptr); // MvGet, MvSet, MvRef, MvPrivate, ...
  Instruction *memvarParam(const std::string &name, uint32_t index);
  Instruction *macro(Op op, Value *text, const std::vector<Value *> &more = {}); // MacroGet, MacroSet, ...
  Instruction *truth(Value *a);
  Instruction *fromBool(Value *b);
  Instruction *call(Callee c, const std::vector<Value *> &args);
  Instruction *send(std::string message, Value *object, const std::vector<Value *> &args);
  Instruction *classNew(int32_t classIndex);
  Instruction *arrayLit(const std::vector<Value *> &elems);
  Instruction *hashLit(const std::vector<Value *> &pairs);
  Instruction *index(Value *base, Value *idx);
  Instruction *indexSet(Value *base, Value *idx, Value *value);
  Instruction *makeBlock(Function *blockFn, std::vector<Var *> captures);
  Instruction *paramsRef(std::vector<Var *> params);
  Instruction *callForward(Function *callee);
  Instruction *argsRest();
  Instruction *seqCatch();
  Instruction *seqValue();
  Instruction *seqWith(Value *handler);
  Instruction *seqRestore(Value *old);
  Instruction *seqSuspend();
  Instruction *seqResume(Value *requests, Value *value);
  Instruction *substCheck(Value *checked);
  Instruction *substValue(Value *checked);
  Instruction *forTest(Value *counter, Value *limit, Value *step);
  Instruction *enumLen(Value *coll);
  Instruction *eachSource(Value *coll, Value *descend);
  Instruction *eachInCell(Var *v);
  Instruction *eachEnter(Var *v, Value *inCell, Value *coll);
  Instruction *eachMove(Var *v, Value *inCell, Value *idx);
  Instruction *eachLeave(Var *v, Value *inCell, Value *saved);
  Instruction *eachKey(Value *coll, Value *idx);
  Instruction *refVar(Var *v);
  Instruction *refIndex(Value *base, Value *idx);
  Instruction *switchCheck(Value *v);
  Instruction *switchMatch(Value *v, Value *c);
  Instruction *intAdd(Value *a, Value *b);
  Instruction *intSub(Value *a, Value *b);
  Instruction *intCmp(IntPred p, Value *a, Value *b);
  Instruction *boolAnd(Value *a, Value *b);
  Instruction *boolNot(Value *a);

  // Terminators
  Instruction *br(BasicBlock *to, const std::vector<Value *> &args = {});
  // Replaces the terminator of `from` with `br to(args)`, keeping its source
  // location; the builder then points at the end of `from`.
  Instruction *jump(BasicBlock *from, BasicBlock *to, const std::vector<Value *> &args);
  Instruction *condBr(Value *cond, BasicBlock *t, BasicBlock *f, const std::vector<Value *> &targs = {},
                      const std::vector<Value *> &fargs = {});
  Instruction *ret(Value *v);

private:
  Instruction *make(Op op, Type t, std::initializer_list<Value *> ops = {});
  Constant *constant(Constant::Lit lit, Type t);

  Function *fn_;
  BasicBlock *bb_ = nullptr;
  uint32_t line_ = 0;
  BasicBlock *unwind_ = nullptr;
  bool assignUnwind_ = false;
  SourceLoc loc_;
};

} // namespace xb::xir
