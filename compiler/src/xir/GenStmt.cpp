// AST to xIR: statements.
#include "xir/Gen.h"

namespace xb::xir {

void Generator::genBlock(const Block &blk) {
  for (auto &s : blk) genStmt(*s);
}

// Code after RETURN, EXIT or LOOP goes to a block nothing reaches; it is
// removed once the function is complete.
void Generator::startDeadBlock() { cur_.b->setBlock(cur_.fn->addBlock("dead")); }

void Generator::genStmt(const Stmt &s) {
  Builder &b = *cur_.b;
  b.setLoc(s.loc);
  switch (s.kind) {
  case StmtKind::Expr: genDiscard(*static_cast<const ExprStmt &>(s).expr); break;
  case StmtKind::Local: genLocals(static_cast<const LocalStmt &>(s)); break;
  case StmtKind::Static: break; // initialized by the module initialization
  case StmtKind::Return: genReturn(static_cast<const ReturnStmt &>(s)); break;
  case StmtKind::If: genIf(static_cast<const IfStmt &>(s)); break;
  case StmtKind::While: genWhile(static_cast<const WhileStmt &>(s)); break;
  case StmtKind::For: genFor(static_cast<const ForStmt &>(s)); break;
  case StmtKind::ForEach: genForEach(static_cast<const ForEachStmt &>(s)); break;
  case StmtKind::Switch: genSwitch(static_cast<const SwitchStmt &>(s)); break;
  case StmtKind::Sequence: genSequence(static_cast<const SequenceStmt &>(s)); break;
  case StmtKind::Exit:
  case StmtKind::Loop:
    b.br(s.kind == StmtKind::Exit ? cur_.loops.back().exitTo : cur_.loops.back().loopTo);
    startDeadBlock();
    break;
  }
}

// A LOCAL's initializer is an ordinary store (with the write barrier when the
// variable may be shared).
void Generator::genLocals(const LocalStmt &s) {
  for (const LocalDecl &d : s.decls) {
    if (!d.init) continue;
    Var *v = frameVar(d.slot);
    if (v->name.empty()) v->name = d.name;
    cur_.b->setLoc(d.loc);
    Value *value = genExpr(*d.init);
    Access a = accessOf(*v);
    cur_.b->store(v, a, value);
    if (!isPlain(*v, a)) cur_.b->barrier(v, a);
  }
}

// A loop variable others can see (a STATIC, a shared variable, a parameter
// passed by reference) gets its value from before the loop back on RETURN too.
void Generator::genReturn(const ReturnStmt &r) {
  Builder &b = *cur_.b;
  Value *value = r.value ? genExpr(*r.value) : nullptr;
  for (auto it = cur_.eachVars.rbegin(); it != cur_.eachVars.rend(); ++it) {
    Var *x = varOf(*it->var);
    if (mayBeInCell(*x)) b.eachLeave(x, eachInCell(*it), it->saved);
  }
  b.ret(value);
  startDeadBlock();
}

void Generator::genIf(const IfStmt &st) {
  Builder &b = *cur_.b;
  BasicBlock *end = cur_.fn->addBlock("endif");
  for (auto &arm : st.arms) {
    Value *c = genCond(*arm.cond);
    BasicBlock *then = cur_.fn->addBlock("then");
    BasicBlock *next = cur_.fn->addBlock("else");
    b.condBr(c, then, next);
    b.setBlock(then);
    genBlock(arm.body);
    if (!b.terminated()) b.br(end);
    b.setBlock(next);
  }
  genBlock(st.elseBody);
  if (!b.terminated()) b.br(end);
  b.setBlock(end);
}

void Generator::genWhile(const WhileStmt &st) {
  Builder &b = *cur_.b;
  BasicBlock *cond = cur_.fn->addBlock("while.cond");
  BasicBlock *body = cur_.fn->addBlock("while.body");
  BasicBlock *end = cur_.fn->addBlock("while.end");
  b.br(cond);
  b.setBlock(cond);
  b.condBr(genCond(*st.cond), body, end);
  b.setBlock(body);
  cur_.loops.push_back({end, cond});
  genBlock(st.body);
  cur_.loops.pop_back();
  if (!b.terminated()) b.br(cond);
  b.setBlock(end);
}

// FOR v := start TO limit [STEP step]: the limit and the step are evaluated
// again on every test, as in Clipper; the step is evaluated after the limit.
void Generator::genFor(const ForStmt &st) {
  Builder &b = *cur_.b;
  auto &var = static_cast<const VarExpr &>(*st.var);
  store(var, genExpr(*st.start));
  BasicBlock *test = cur_.fn->addBlock("for.test");
  BasicBlock *body = cur_.fn->addBlock("for.body");
  BasicBlock *incr = cur_.fn->addBlock("for.incr");
  BasicBlock *end = cur_.fn->addBlock("for.end");
  b.br(test);

  b.setBlock(test);
  Value *limit = genExpr(*st.limit);
  Value *step = st.step ? genExpr(*st.step) : b.integer(1, 0);
  b.condBr(b.forTest(load(var), limit, step), body, end);

  b.setBlock(body);
  cur_.loops.push_back({end, incr});
  genBlock(st.body);
  cur_.loops.pop_back();
  if (!b.terminated()) b.br(incr);

  b.setBlock(incr);
  step = st.step ? genExpr(*st.step) : b.integer(1, 0);
  store(var, b.binary(BinOp::Add, load(var), step));
  b.br(test);
  b.setBlock(end);
}

// Whether the loop works inside the variable's cell: for a frame variable,
// whether it was in one when the loop started (a codeblock or reference that
// takes it during an iteration keeps that iteration's element, see
// xb_each_move); a STATIC is one variable for everyone, so its cell, once
// made, is always followed.
Value *Generator::eachInCell(const EachVar &v) {
  Var *x = varOf(*v.var);
  return x->kind == Var::Kind::Static ? cur_.b->eachInCell(x) : v.inCell;
}

// FOR EACH: the collections are evaluated once. During the loop each variable
// holds a reference to the current element, so reading or assigning it reads
// or assigns the element; a variable shared with codeblocks or a caller holds
// it inside its cell, so they see the elements too. The variables get their
// previous values back when the loop ends (by EXIT too), and when unwinding
// leaves it for a BEGIN SEQUENCE of the function, or for the caller when one
// of them may be seen there.
void Generator::genForEach(const ForEachStmt &st) {
  Builder &b = *cur_.b;
  size_t n = st.vars.size();
  std::vector<Value *> colls(n);
  for (size_t k = 0; k < n; ++k) colls[k] = genExpr(*st.collections[k]);
  std::vector<EachVar> vars;
  for (size_t k = 0; k < n; ++k) {
    auto &v = static_cast<const VarExpr &>(*st.vars[k]);
    Var *x = varOf(v);
    Value *inCell = mayBeInCell(*x) ? static_cast<Value *>(b.eachInCell(x)) : b.boolean(false);
    Value *saved = b.eachEnter(x, inCell, colls[k]);
    vars.push_back({&v, inCell, saved});
  }
  BasicBlock *outer = b.unwindBlock();
  bool cleanup = outer != nullptr;
  for (const EachVar &v : vars) cleanup |= mayBeInCell(*varOf(*v.var));
  BasicBlock *restore = nullptr;
  if (cleanup) {
    restore = cur_.fn->addBlock("each.unwind");
    BasicBlock *start = cur_.fn->addBlock("each.start");
    b.br(start);
    b.setUnwind(restore);
    b.setBlock(start);
  }
  // One step counter; each collection has its own index: step + 1, or for
  // DESCEND its length at entry minus the step. Ascending loops end at the end
  // of the shortest collection, as long as it is now; descending ones when an
  // index reaches 0 (an array that shrank meanwhile gives a bound error).
  std::vector<Value *> last(n);
  if (st.descend)
    for (size_t k = 0; k < n; ++k) last[k] = b.enumLen(colls[k]);
  BasicBlock *test = cur_.fn->addBlock("each.test");
  BasicBlock *body = cur_.fn->addBlock("each.body");
  BasicBlock *incr = cur_.fn->addBlock("each.next");
  BasicBlock *end = cur_.fn->addBlock("each.end");
  Value *step = test->addParam(Type::I64);
  b.br(test, {b.i64(0)});

  b.setBlock(test);
  auto index = [&](size_t k) -> Value * {
    return st.descend ? b.intSub(last[k], step) : b.intAdd(step, b.i64(1));
  };
  Value *inRange = nullptr;
  for (size_t k = 0; k < n; ++k) {
    Value *i = index(k); // emitted before the length: one statement per instruction keeps the order fixed
    Value *ok;
    if (st.descend) {
      ok = b.intCmp(IntPred::Ge, i, b.i64(1));
    } else {
      Value *len = b.enumLen(colls[k]);
      ok = b.intCmp(IntPred::Le, i, len);
    }
    inRange = inRange ? b.boolAnd(inRange, ok) : ok;
  }
  b.condBr(inRange, body, end);

  b.setBlock(body);
  for (size_t k = 0; k < n; ++k) {
    Value *i = index(k); // emitted before the test of the cell, like the loop test does
    b.eachMove(varOf(*vars[k].var), eachInCell(vars[k]), i);
  }
  cur_.eachVars.insert(cur_.eachVars.end(), vars.begin(), vars.end());
  cur_.loops.push_back({end, incr});
  genBlock(st.body);
  cur_.loops.pop_back();
  cur_.eachVars.resize(cur_.eachVars.size() - n);
  if (!b.terminated()) b.br(incr);

  b.setBlock(incr);
  b.br(test, {b.intAdd(step, b.i64(1))});

  if (cleanup) {
    b.setUnwind(outer);
    b.setBlock(restore);
    for (const EachVar &v : vars) b.eachLeave(varOf(*v.var), eachInCell(v), v.saved);
    unwindOnward();
  }
  b.setBlock(end);
  for (const EachVar &v : vars) b.eachLeave(varOf(*v.var), eachInCell(v), v.saved);
}

// The end of an unwind block whose work is done: unwinding goes on to the
// enclosing one, or the function returns with the request pending.
void Generator::unwindOnward() {
  Builder &b = *cur_.b;
  if (BasicBlock *outer = b.unwindBlock()) b.br(outer);
  else b.ret(nullptr);
}

// BEGIN SEQUENCE: the body's blocks unwind to a handler, which takes a BREAK
// (seq.catch) and goes on with RECOVER, storing the BREAK value in the USING
// variable; any other request (QUIT, an error that ends the program) goes on
// unwinding. The handler reads variables as the failed operation left them:
// the function keeps them in memory (promote-variables leaves it alone).
void Generator::genSequence(const SequenceStmt &st) {
  Builder &b = *cur_.b;
  BasicBlock *outer = b.unwindBlock();
  BasicBlock *body = cur_.fn->addBlock("seq.body");
  BasicBlock *handler = cur_.fn->addBlock("seq.unwind");
  BasicBlock *end = cur_.fn->addBlock("seq.end");
  b.br(body);
  b.setUnwind(handler);
  b.setBlock(body);
  genBlock(st.body);
  if (!b.terminated()) b.br(end);

  b.setUnwind(outer);
  b.setBlock(handler);
  BasicBlock *recover = cur_.fn->addBlock("seq.recover");
  BasicBlock *onward = cur_.fn->addBlock("seq.onward");
  b.condBr(b.seqCatch(), recover, onward);
  b.setBlock(onward);
  unwindOnward();
  b.setBlock(recover);
  Value *value = b.seqValue(); // taken even without USING, so the context does not keep it alive
  if (st.usingVar) store(static_cast<const VarExpr &>(*st.usingVar), value);
  genBlock(st.recover);
  if (!b.terminated()) b.br(end);
  b.setBlock(end);
}

// SWITCH: the value is tested against each CASE in order; the bodies follow
// each other, so control falls through until EXIT (which leaves the SWITCH).
void Generator::genSwitch(const SwitchStmt &st) {
  Builder &b = *cur_.b;
  Value *value = genExpr(*st.value);
  if (st.cases.empty()) return; // nothing compares the value, so any type is accepted
  value = b.switchCheck(value);  // an error's handler may substitute another value
  BasicBlock *end = cur_.fn->addBlock("switch.end");
  std::vector<BasicBlock *> bodies;
  for (size_t i = 0; i < st.cases.size(); ++i) bodies.push_back(cur_.fn->addBlock("switch.case"));
  BasicBlock *otherwise = end;
  for (size_t i = 0; i < st.cases.size(); ++i) {
    if (!st.cases[i].value) {
      otherwise = bodies[i];
      continue;
    }
    BasicBlock *next = cur_.fn->addBlock("switch.test");
    b.condBr(b.switchMatch(value, genExpr(*st.cases[i].value)), bodies[i], next);
    b.setBlock(next);
  }
  b.br(otherwise);
  for (size_t i = 0; i < st.cases.size(); ++i) {
    b.setBlock(bodies[i]);
    cur_.loops.push_back({end, cur_.loops.empty() ? nullptr : cur_.loops.back().loopTo});
    genBlock(st.cases[i].body);
    cur_.loops.pop_back();
    if (!b.terminated()) b.br(i + 1 < bodies.size() ? bodies[i + 1] : end);
  }
  b.setBlock(end);
}

} // namespace xb::xir
