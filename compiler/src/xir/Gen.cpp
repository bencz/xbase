// AST to xIR: the module, its functions and codeblocks, and how variables are
// reached.
#include "xir/Gen.h"

#include "xir/Analysis.h"

namespace xb::xir {

std::unique_ptr<Module> Generator::run(const xb::Module &ast) {
  mod_ = std::make_unique<Module>();
  mod_->name = ast.name;
  for (size_t i = 0; i < ast.statics.size(); ++i) {
    auto v = std::make_unique<Var>(Var{Var::Kind::Static, (uint32_t)i, false, false, ast.statics[i]->name});
    mod_->statics.push_back(std::move(v));
  }
  std::vector<Function *> fns;
  for (auto &f : ast.funcs) fns.push_back(declare(*f));
  for (size_t i = 0; i < ast.funcs.size(); ++i) genFunction(*ast.funcs[i], fns[i]);
  genModuleInit(ast);
  for (auto &f : mod_->functions) removeUnreachableBlocks(*f);
  return std::move(mod_);
}

Function *Generator::declare(const FuncDecl &f) {
  mod_->functions.push_back(std::make_unique<Function>(Function::Kind::Program, f.name));
  Function *fn = mod_->functions.back().get();
  fn->internal = f.isStatic;
  funcs_[f.name] = fn;
  return fn;
}

// Parameter versioning. A parameter may hold a reference only if its caller
// passed one: one that is never shared (not captured, never passed on by
// reference) cannot become one. A function with such parameters is generated
// twice. Its own body starts by testing them (params.ref): when none holds a
// reference, each is copied once into a LOCAL that stands for it, which
// mem2reg can make an SSA value; otherwise the call goes, with its arguments
// as received, to the other body (NAME.byref), which reaches the parameters
// through their references as written.
void Generator::genFunction(const FuncDecl &f, Function *fn) {
  std::vector<uint32_t> plain;
  for (uint32_t s = 0; s < f.params.size(); ++s)
    if (!(s < f.boxed.size() && f.boxed[s])) plain.push_back(s);
  if (plain.empty()) {
    genBody(f, fn, nullptr, {});
    return;
  }
  mod_->functions.push_back(std::make_unique<Function>(Function::Kind::Program, f.name + ".byref"));
  Function *byRef = mod_->functions.back().get();
  byRef->internal = true;
  genBody(f, byRef, nullptr, {});
  genBody(f, fn, byRef, plain);
}

// The frame holds the parameters, then the LOCALs; a shared one moves into a
// cell when a codeblock captures it or it is passed by reference. With
// `byRef`, the parameters `plain` stand as LOCALs when none holds a reference.
void Generator::genBody(const FuncDecl &f, Function *fn, Function *byRef, const std::vector<uint32_t> &plain) {
  cur_ = FnState{};
  cur_.fn = fn;
  cur_.b = std::make_unique<Builder>(fn);
  cur_.scope = f.name;
  cur_.function = f.name;
  fn->stackName = f.name;
  fn->numParams = (uint32_t)f.params.size();
  for (uint32_t s = 0; s < f.numLocals; ++s) {
    Var *v = fn->addVar(Var::Kind::Frame, s, s < f.params.size() ? f.params[s].name : "");
    v->shared = s < f.boxed.size() && f.boxed[s];
    v->param = s < f.params.size();
  }
  Builder &b = *cur_.b;
  b.setBlock(fn->addBlock("entry"));
  b.setLoc(f.loc);
  if (byRef) {
    std::vector<Var *> params;
    for (uint32_t s : plain) params.push_back(frameVar(s));
    BasicBlock *forward = fn->addBlock("byref"), *body = fn->addBlock("body");
    b.condBr(b.paramsRef(params), forward, body);
    b.setBlock(forward);
    b.ret(b.callForward(byRef));
    b.setBlock(body);
    for (size_t k = 0; k < plain.size(); ++k) {
      Var *param = params[k];
      Var *local = fn->addVar(Var::Kind::Frame, f.numLocals + (uint32_t)k, param->name);
      b.store(local, Access::Direct, b.load(param, Access::Direct)); // no reference: its own item
      cur_.paramLocals[plain[k]] = local;
    }
  }
  genBlock(f.body);
  if (!b.terminated()) b.ret(nullptr);
}

// STATICs get their initial values once, before the program starts; a
// constant initializer cannot fail or capture anything.
void Generator::genModuleInit(const xb::Module &ast) {
  bool any = false;
  for (LocalDecl *d : ast.statics) any |= d->init != nullptr;
  if (!any) return;
  mod_->functions.push_back(std::make_unique<Function>(Function::Kind::ModuleInit, "module.init"));
  Function *fn = mod_->functions.back().get();
  fn->internal = true;
  mod_->init = fn;
  cur_ = FnState{};
  cur_.fn = fn;
  cur_.b = std::make_unique<Builder>(fn);
  cur_.scope = "module";
  Builder &b = *cur_.b;
  b.setBlock(fn->addBlock("entry"));
  for (LocalDecl *d : ast.statics) {
    if (!d->init) continue;
    b.setLoc(d->loc);
    b.store(mod_->statics[d->slot].get(), Access::Direct, genExpr(*d->init));
  }
  b.ret(nullptr);
}

// A codeblock is a function of its own: its frame holds its parameters, and
// the variables it uses from enclosing functions or blocks are its captures.
Function *Generator::genCodeblockFunction(const BlockExpr &blk) {
  std::string name = cur_.scope + ".block" + std::to_string(++blockCount_);
  mod_->functions.push_back(std::make_unique<Function>(Function::Kind::Block, name));
  Function *fn = mod_->functions.back().get();
  fn->internal = true;
  fn->numParams = blk.numLocals;
  for (uint32_t s = 0; s < blk.numLocals; ++s) fn->addVar(Var::Kind::Frame, s, blk.params[s].name)->param = true;
  for (uint32_t k = 0; k < blk.captures.size(); ++k) fn->addVar(Var::Kind::Capture, k, "");

  FnState outer = std::move(cur_);
  cur_ = FnState{};
  cur_.fn = fn;
  cur_.b = std::make_unique<Builder>(fn);
  cur_.scope = name;
  cur_.function = outer.function;
  fn->stackName = "(b)" + outer.function;
  Builder &b = *cur_.b;
  b.setLine(outer.b->line()); // the line of the statement defining the block
  b.setBlock(fn->addBlock("entry"));
  b.setLoc(blk.loc);
  Value *result = nullptr;
  for (size_t i = 0; i < blk.body.size(); ++i) {
    if (i + 1 < blk.body.size()) genDiscard(*blk.body[i]);
    else result = genExpr(*blk.body[i]);
  }
  b.ret(result);
  cur_ = std::move(outer);
  return fn;
}

Var *Generator::frameVar(uint32_t slot) {
  auto local = cur_.paramLocals.find(slot);
  if (local != cur_.paramLocals.end()) return local->second;
  for (auto &v : cur_.fn->vars)
    if (v->kind == Var::Kind::Frame && v->index == slot) return v.get();
  return nullptr;
}

Var *Generator::varOf(const VarExpr &v) {
  switch (v.where) {
  case VarWhere::Static: return mod_->statics[v.slot].get();
  case VarWhere::Capture:
    for (auto &x : cur_.fn->vars)
      if (x->kind == Var::Kind::Capture && x->index == v.slot) {
        if (x->name.empty()) x->name = v.name;
        return x.get();
      }
    return nullptr;
  case VarWhere::Frame: {
    Var *x = frameVar(v.slot);
    if (x->name.empty()) x->name = v.name;
    return x;
  }
  }
  return nullptr;
}

bool Generator::isEachVar(const VarExpr &v) const {
  for (const EachVar &e : cur_.eachVars)
    if (e.var->where == v.where && e.var->slot == v.slot) return true;
  return false;
}

// A FOR EACH variable of a loop being generated holds a reference to the
// element. A variable that may be in a cell or hold a reference is tested at
// run time: a STATIC, a captured or shared variable (its cell may hold the
// reference of a loop), a parameter (its caller may pass a reference). Other
// LOCALs never do.
Access Generator::accessOf(const VarExpr &v) {
  return isEachVar(v) ? Access::ThroughRef : accessOf(*varOf(v));
}

Access Generator::accessOf(const Var &v) {
  return v.kind == Var::Kind::Capture || mayBeInCell(v) ? Access::MaybeRef : Access::Direct;
}

Value *Generator::load(const VarExpr &v) {
  Access a = accessOf(v);
  return cur_.b->load(varOf(v), a);
}

// A store into a variable that may live in the heap (a cell, an element
// through a reference) is followed by the write barrier.
void Generator::store(const VarExpr &v, Value *value) {
  Var *x = varOf(v);
  Access a = accessOf(v);
  cur_.b->store(x, a, value);
  if (!isPlain(*x, a)) cur_.b->barrier(x, a);
}

} // namespace xb::xir
