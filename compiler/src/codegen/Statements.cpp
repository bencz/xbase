// Statements: blocks, IF, loops, FOR EACH, SWITCH, RETURN.
#include "codegen/CodeGen.h"

#include <llvm/IR/Constants.h>

using namespace llvm;

namespace xb {

void CodeGen::genBlock(const Block &blk) {
  for (auto &s : blk) {
    uint32_t mark = temps_;
    genStmt(*s);
    temps_ = mark;
  }
}

void CodeGen::genStmt(const Stmt &s) {
  switch (s.kind) {
  case StmtKind::Expr:
    genDiscard(*static_cast<const ExprStmt &>(s).expr);
    break;

  case StmtKind::Local:
    for (const LocalDecl &d : static_cast<const LocalStmt &>(s).decls) {
      if (!d.init) continue;
      if (d.slot >= fnBoxed_->size() || !(*fnBoxed_)[d.slot]) {
        genExpr(*d.init, slot(d.slot));
        continue;
      }
      // A captured LOCAL lives in a heap cell, which may age while the value is
      // computed: the value goes through a frame slot (a root), then the cell.
      uint32_t mark = temps_;
      Value *t = tempSlot();
      genExpr(*d.init, t);
      Value *cell = cellOfSlot(d.slot);
      copyItem(cell, t);
      cellWritten(cell);
      temps_ = mark;
    }
    break;

  case StmtKind::Return: {
    auto &r = static_cast<const ReturnStmt &>(s);
    if (r.value) genExpr(*r.value, fnRet_);
    for (auto it = eachVars_.rbegin(); it != eachVars_.rend(); ++it) // a STATIC outlives the function
      if (it->var->where == VarWhere::Static) copyItem(it->raw, it->saved);
    b_.CreateBr(fnExit_);
    startDeadBlock();
    break;
  }

  case StmtKind::If: {
    auto &st = static_cast<const IfStmt &>(s);
    BasicBlock *end = newBlock("endif");
    for (auto &arm : st.arms) {
      Value *c = genCond(*arm.cond);
      BasicBlock *then = newBlock("then");
      BasicBlock *next = newBlock("else");
      b_.CreateCondBr(c, then, next);
      b_.SetInsertPoint(then);
      genBlock(arm.body);
      if (!b_.GetInsertBlock()->getTerminator()) b_.CreateBr(end);
      b_.SetInsertPoint(next);
    }
    genBlock(st.elseBody);
    if (!b_.GetInsertBlock()->getTerminator()) b_.CreateBr(end);
    end->moveAfter(b_.GetInsertBlock());
    b_.SetInsertPoint(end);
    break;
  }

  case StmtKind::While: {
    auto &st = static_cast<const WhileStmt &>(s);
    BasicBlock *cond = newBlock("while.cond");
    BasicBlock *body = newBlock("while.body");
    BasicBlock *end = newBlock("while.end");
    b_.CreateBr(cond);
    b_.SetInsertPoint(cond);
    b_.CreateCondBr(genCond(*st.cond), body, end);
    b_.SetInsertPoint(body);
    loops_.push_back({end, cond});
    genBlock(st.body);
    loops_.pop_back();
    if (!b_.GetInsertBlock()->getTerminator()) b_.CreateBr(cond);
    end->moveAfter(b_.GetInsertBlock());
    b_.SetInsertPoint(end);
    break;
  }

  case StmtKind::For: {
    // The TO and STEP expressions are re-evaluated on every iteration, as in
    // Clipper.
    // The variable's address is taken at each use: it may designate an array
    // element (a FOR EACH variable), which moves when the array grows.
    auto &st = static_cast<const ForStmt &>(s);
    if (hasSideEffects(*st.start)) { // the start value may read the variable (through a codeblock)
      Value *t = tempSlot();
      genExpr(*st.start, t);
      copyItem(varAddress(*st.var), t);
    } else {
      genExpr(*st.start, varAddress(*st.var));
    }
    writeBarrier(*st.var);
    BasicBlock *test = newBlock("for.test");
    BasicBlock *body = newBlock("for.body");
    BasicBlock *incr = newBlock("for.incr");
    BasicBlock *end = newBlock("for.end");
    b_.CreateBr(test);

    b_.SetInsertPoint(test);
    uint32_t mark = temps_;
    Value *limit = genOperand(*st.limit);
    if (st.step && hasSideEffects(*st.step)) {
      Value *t = tempSlot(); // evaluating STEP must not change the limit being tested
      copyItem(t, limit);
      limit = t;
    }
    Value *step = st.step ? genOperand(*st.step) : intLiteral(1);
    Value *go = b_.CreateCall(rtForTest_, {fnCtx_, varAddress(*st.var), limit, step});
    checkPending();
    b_.CreateCondBr(b_.CreateICmpNE(go, ConstantInt::get(i32_, 0)), body, end);
    temps_ = mark;

    b_.SetInsertPoint(body);
    loops_.push_back({end, incr});
    genBlock(st.body);
    loops_.pop_back();
    if (!b_.GetInsertBlock()->getTerminator()) b_.CreateBr(incr);

    incr->moveAfter(b_.GetInsertBlock());
    b_.SetInsertPoint(incr);
    step = st.step ? genOperand(*st.step) : intLiteral(1);
    Value *var = varAddress(*st.var);
    genBinary(BinOp::Add, var, var, step);
    writeBarrier(*st.var);
    temps_ = mark;
    b_.CreateBr(test);

    end->moveAfter(b_.GetInsertBlock());
    b_.SetInsertPoint(end);
    break;
  }

  case StmtKind::Static: // initialized by the module initialization
    break;

  case StmtKind::ForEach:
    genForEach(static_cast<const ForEachStmt &>(s));
    break;

  case StmtKind::Switch:
    genSwitch(static_cast<const SwitchStmt &>(s));
    break;

  case StmtKind::Exit:
  case StmtKind::Loop:
    b_.CreateBr(s.kind == StmtKind::Exit ? loops_.back().exitTo : loops_.back().loopTo);
    startDeadBlock();
    break;
  }
}

// A local of the LLVM function, in its entry block (promoted to a register by
// mem2reg).
Value *CodeGen::entryAlloca(Type *ty, const char *name) {
  IRBuilder<> entry(&fn_->getEntryBlock(), fn_->getEntryBlock().begin());
  return entry.CreateAlloca(ty, nullptr, name);
}

// FOR EACH: the collections are evaluated once. During the loop each variable
// holds a reference (XB_T_REF) to the current element, so reading or assigning
// the variable reads or assigns the element (see varAddress). The variables
// get their previous values back when the loop ends, by EXIT too, and STATICs
// also on RETURN. A variable that a codeblock captures gets a new cell and
// reference at every iteration, so a block keeps designating the element of the
// iteration that created it.
void CodeGen::genForEach(const ForEachStmt &st) {
  size_t n = st.vars.size();
  std::vector<Value *> colls(n);
  for (size_t k = 0; k < n; ++k) {
    colls[k] = tempSlot();
    genExpr(*st.collections[k], colls[k]);
  }
  std::vector<EachVar> vars;
  for (size_t k = 0; k < n; ++k) {
    auto &v = static_cast<const VarExpr &>(*st.vars[k]);
    Value *raw = v.where == VarWhere::Static ? staticAddress(v.slot) : slot(v.slot);
    Value *saved = tempSlot();
    copyItem(saved, raw);
    vars.push_back({&v, raw, saved});
    if (!isBoxed(v)) b_.CreateCall(rtEnumRefNew_, {fnCtx_, raw, colls[k]});
  }
  auto length = [&](size_t k) {
    Value *len = b_.CreateCall(rtEnumLen_, {fnCtx_, colls[k]});
    checkPending();
    return len;
  };
  // One step counter; each collection has its own index: step + 1, or for
  // DESCEND its length at entry minus the step. Ascending loops end at the end
  // of the shortest collection, as long as it is now; descending ones when an
  // index reaches 0 (an array that shrank meanwhile gives a bound error).
  std::vector<Value *> last(n);
  if (st.descend)
    for (size_t k = 0; k < n; ++k) last[k] = length(k);
  Value *step = entryAlloca(i64_, "each.step");
  b_.CreateStore(ConstantInt::get(i64_, 0), step);
  auto index = [&](size_t k, Value *cur) {
    return st.descend ? b_.CreateSub(last[k], cur) : b_.CreateAdd(cur, ConstantInt::get(i64_, 1));
  };

  BasicBlock *test = newBlock("each.test");
  BasicBlock *body = newBlock("each.body");
  BasicBlock *incr = newBlock("each.next");
  BasicBlock *end = newBlock("each.end");
  b_.CreateBr(test);

  b_.SetInsertPoint(test);
  Value *cur = b_.CreateLoad(i64_, step);
  Value *inRange = ConstantInt::getTrue(ctx_);
  for (size_t k = 0; k < n; ++k) {
    Value *i = index(k, cur);
    inRange = b_.CreateAnd(inRange, st.descend ? b_.CreateICmpSGE(i, ConstantInt::get(i64_, 1))
                                               : b_.CreateICmpSLE(i, length(k)));
  }
  b_.CreateCondBr(inRange, body, end);

  b_.SetInsertPoint(body);
  cur = b_.CreateLoad(i64_, step);
  for (size_t k = 0; k < n; ++k) {
    Value *i = index(k, cur);
    if (isBoxed(*vars[k].var)) {
      Value *t = tempSlot();
      b_.CreateCall(rtEnumRefNew_, {fnCtx_, t, colls[k]});
      b_.CreateCall(rtEnumRefMove_, {fnCtx_, t, i});
      checkPending();
      b_.CreateCall(rtCellBox_, {fnCtx_, t});
      copyItem(vars[k].raw, t);
    } else {
      b_.CreateCall(rtEnumRefMove_, {fnCtx_, vars[k].raw, i});
      checkPending();
    }
  }
  eachVars_.insert(eachVars_.end(), vars.begin(), vars.end());
  loops_.push_back({end, incr});
  genBlock(st.body);
  loops_.pop_back();
  eachVars_.resize(eachVars_.size() - n);
  if (!b_.GetInsertBlock()->getTerminator()) b_.CreateBr(incr);

  incr->moveAfter(b_.GetInsertBlock());
  b_.SetInsertPoint(incr);
  b_.CreateStore(b_.CreateAdd(b_.CreateLoad(i64_, step), ConstantInt::get(i64_, 1)), step);
  b_.CreateBr(test);

  end->moveAfter(incr);
  b_.SetInsertPoint(end);
  for (const EachVar &v : vars) copyItem(v.raw, v.saved);
}

// SWITCH: the value is tested against each CASE in order; the bodies follow
// each other, so control falls through until EXIT (which leaves the SWITCH).
void CodeGen::genSwitch(const SwitchStmt &st) {
  Value *value = tempSlot();
  genExpr(*st.value, value);
  if (st.cases.empty()) return; // nothing compares the value, so any type is accepted
  b_.CreateCall(rtSwitchCheck_, {fnCtx_, value});
  checkPending();
  BasicBlock *end = newBlock("switch.end");
  std::vector<BasicBlock *> bodies;
  for (size_t i = 0; i < st.cases.size(); ++i) bodies.push_back(newBlock("switch.case"));
  BasicBlock *otherwise = end;
  for (size_t i = 0; i < st.cases.size(); ++i) {
    if (!st.cases[i].value) {
      otherwise = bodies[i];
      continue;
    }
    Value *match = b_.CreateCall(rtSwitchMatch_, {value, genOperand(*st.cases[i].value)});
    BasicBlock *next = newBlock("switch.test");
    b_.CreateCondBr(b_.CreateICmpNE(match, ConstantInt::get(i32_, 0)), bodies[i], next);
    b_.SetInsertPoint(next);
  }
  b_.CreateBr(otherwise);
  for (size_t i = 0; i < st.cases.size(); ++i) {
    bodies[i]->moveAfter(b_.GetInsertBlock());
    b_.SetInsertPoint(bodies[i]);
    loops_.push_back({end, loops_.empty() ? nullptr : loops_.back().loopTo});
    genBlock(st.cases[i].body);
    loops_.pop_back();
    if (!b_.GetInsertBlock()->getTerminator()) b_.CreateBr(i + 1 < bodies.size() ? bodies[i + 1] : end);
  }
  end->moveAfter(b_.GetInsertBlock());
  b_.SetInsertPoint(end);
}

} // namespace xb
