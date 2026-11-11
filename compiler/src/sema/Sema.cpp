#include "sema/Sema.h"

#include "common/Text.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace xb {

struct Sema::Scope {
  uint32_t next = 0;
  unsigned loopDepth = 0;
  unsigned switchDepth = 0;
  unsigned blockDepth = 0;       // nesting inside a statement's body
  bool seenExecutable = false;   // declarations must precede executable statements
  // Inside a part RETURN, EXIT and LOOP may not leave (a BEGIN SEQUENCE's body, the Clipper rule; ALWAYS and
  // FINALLY, which run while a request may wait to go on): its name, and the loops and SWITCHes around it.
  const char *fence = nullptr;
  unsigned fenceLoops = 0, fenceSwitches = 0;
  bool inBlock = false;          // the statements of an extended codeblock
  bool inSequence = false;       // any part of a BEGIN SEQUENCE or TRY (YIELD cannot suspend there)
};

void Sema::run(Module &m) {
  module_ = &m;
  frames_.push_back({{}, {}, {}, nullptr, nullptr}); // the file's STATICs and MEMVARs
  Scope fileScope;
  for (LocalDecl &d : m.fileStatics) declare(d, true, fileScope);
  for (const LocalDecl &d : m.fileMemvars) declareMemvar(d);
  std::unordered_map<std::string, SourceLoc> seen;
  for (auto &f : m.funcs) {
    // INIT and EXIT procedures have names of their own: nothing calls them.
    auto [it, fresh] = f->startup == FuncDecl::Startup::None ? seen.emplace(f->name, f->loc)
                                                             : std::pair{seen.end(), true};
    if (!fresh) {
      diag_.error(f->loc, "function '" + f->name + "' is already defined");
      diag_.note(it->second, "previous definition is here");
    }
    func(*f);
  }
  frames_.pop_back();
}

// A STATIC is initialized once, before the program starts, so its initializer
// must be constant after folding: literals, arrays of constants, codeblocks
// whose variables are their parameters and STATICs (Clipper rule).
// Where a constant is required (a STATIC's initializer, a CASE of SWITCH), a
// string with `&name` in it is the string as written: nothing is substituted
// there, as in Clipper (a STATIC gets its value before any memory variable
// exists). Codeblocks keep theirs: they run later.
static void literalStrings(ExprPtr &e) {
  switch (e->kind) {
  case ExprKind::MacroText:
    e = std::make_unique<StringExpr>(e->loc, static_cast<MacroTextExpr &>(*e).value);
    break;
  case ExprKind::ArrayLit:
    for (auto &el : static_cast<ArrayLitExpr &>(*e).elems) literalStrings(el);
    break;
  case ExprKind::HashLit: {
    auto &h = static_cast<HashLitExpr &>(*e);
    for (size_t k = 0; k < h.keys.size(); ++k) {
      literalStrings(h.keys[k]);
      literalStrings(h.values[k]);
    }
    break;
  }
  default:
    break;
  }
}

bool Sema::constantInit(const Expr &e, unsigned blockDepth) const {
  switch (e.kind) {
  case ExprKind::Nil: case ExprKind::Logical: case ExprKind::Number: case ExprKind::String:
    return true;
  case ExprKind::ArrayLit:
    for (auto &el : static_cast<const ArrayLitExpr &>(e).elems)
      if (!constantInit(*el, blockDepth)) return false;
    return true;
  case ExprKind::HashLit: {
    auto &h = static_cast<const HashLitExpr &>(e);
    for (size_t k = 0; k < h.keys.size(); ++k)
      if (!constantInit(*h.keys[k], blockDepth) || !constantInit(*h.values[k], blockDepth)) return false;
    return true;
  }
  case ExprKind::Block:
    if (static_cast<const BlockExpr &>(e).extended) return false; // statements: they may do anything
    for (auto &b : static_cast<const BlockExpr &>(e).body)
      if (!constantInit(*b, blockDepth + 1)) return false;
    return true;
  case ExprKind::Var: // a codeblock's own parameters and STATICs; a captured LOCAL has no value yet
    return blockDepth > 0 && static_cast<const VarExpr &>(e).where != VarWhere::Capture;
  default: // anything else runs code: allowed only inside a codeblock body
    return blockDepth > 0;
  }
}

// LOCAL and STATIC declarations. A LOCAL's initializer is resolved before
// its name is visible; a STATIC is visible in its own initializer, so a
// codeblock stored in it can call itself.
void Sema::declare(LocalDecl &d, bool isStatic, Scope &s) {
  if (!isStatic && d.init) expr(*d.init, s);
  bool duplicate = frames_.back().names.count(d.name) != 0 || frames_.back().memvars.count(d.name) != 0;
  if (duplicate) {
    diag_.error(d.loc, "'" + d.name + "' is already declared");
  } else if (isStatic) {
    d.slot = (uint32_t)module_->statics.size();
    module_->statics.push_back(&d);
    frames_.back().names.emplace(d.name, Binding{VarWhere::Static, d.slot});
  } else {
    d.slot = s.next++;
    frames_.back().names.emplace(d.name, Binding{VarWhere::Frame, d.slot});
  }
  if (isStatic && d.init) {
    literalStrings(d.init);
    expr(*d.init, s);
    if (!constantInit(*d.init, 0))
      diag_.error(d.init->loc, "illegal initializer for STATIC '" + d.name + "': it must be constant");
  }
}

// MEMVAR: the name is a memory variable in the frame (the file, or a
// function); it cannot also be one of the frame's LOCALs, parameters or
// STATICs.
void Sema::declareMemvar(const LocalDecl &d) {
  if (frames_.back().names.count(d.name)) diag_.error(d.loc, "'" + d.name + "' is already declared");
  frames_.back().memvars.insert(d.name);
}

// PRIVATE, PUBLIC and PARAMETERS create memory variables: the name cannot be a
// LOCAL or parameter of the function, and is no longer undeclared there.
void Sema::createdMemvar(const LocalDecl &d) {
  if (frames_.back().names.count(d.name)) diag_.error(d.loc, "'" + d.name + "' is already declared");
  frames_.back().created.insert(d.name);
}

bool Sema::memvarCreated(const std::string &name) const {
  for (const Frame &f : frames_)
    if (f.created.count(name)) return true;
  return false;
}

void Sema::func(FuncDecl &f) {
  curFunc_ = &f;
  Scope s;
  if (f.generator) f.params.insert(f.params.begin(), Param{f.loc, "(GENERATOR)"}); // the enumerator it runs for
  frames_.push_back({{}, {}, {}, &f.boxed, nullptr});
  for (const Param &p : f.params) {
    if (!frames_.back().names.emplace(p.name, Binding{VarWhere::Frame, s.next}).second)
      diag_.error(p.loc, "duplicate parameter '" + p.name + "'");
    else ++s.next;
  }
  block(f.body, s);
  if (f.generator && f.variadic) diag_.error(f.loc, "a generator cannot take '...'");
  if (f.generator && f.readsArgs) // its frame is the resume function's, whose argument is the enumerator
    diag_.error(f.loc, "a generator cannot read its arguments as received (PCount(), PValue(), PARAMETERS)");
  f.numLocals = s.next;
  f.boxed.resize(f.numLocals);
  frames_.pop_back();
  curFunc_ = nullptr;
}

void Sema::markBoxed(Frame &f, uint32_t slot) {
  std::vector<bool> &boxed = *f.boxed;
  if (boxed.size() <= slot) boxed.resize(slot + 1);
  boxed[slot] = true;
}

// Whether `name` is a LOCAL, STATIC or parameter where the code is (a MEMVAR
// declaration of the name wins, as in resolve).
bool Sema::declaredVariable(const std::string &name) const {
  for (size_t fi = frames_.size(); fi-- > 0;) {
    if (frames_[fi].names.count(name)) return true;
    if (frames_[fi].memvars.count(name)) return false;
  }
  return false;
}

// Finds a variable in the current frame or an enclosing one. A variable of the
// enclosing function is captured: it is marked shared in its frame, and every codeblock
// between there and here gets a capture that passes its cell along. Parameters
// of an enclosing codeblock cannot be captured.
bool Sema::resolve(VarExpr &v) {
  if (v.memvarAlias) {
    v.where = VarWhere::Memvar;
    return true;
  }
  for (size_t fi = frames_.size(); fi-- > 0;) {
    auto it = frames_[fi].names.find(v.name);
    if (it == frames_[fi].names.end()) {
      if (!frames_[fi].memvars.count(v.name)) continue;
      v.where = VarWhere::Memvar; // MEMVAR in this frame: before a STATIC of an enclosing one
      return true;
    }
    uint32_t slot = it->second.index;
    if (it->second.where == VarWhere::Static) { // module storage: no capture needed
      v.where = VarWhere::Static;
      v.slot = slot;
      return true;
    }
    if (fi + 1 == frames_.size()) {
      v.where = VarWhere::Frame;
      v.slot = slot;
      return true;
    }
    markBoxed(frames_[fi], slot); // a codeblock's parameters and LOCALs too (Clipper reaches only a function's)
    BlockExpr::Capture cap{false, slot};
    for (size_t k = fi + 1; k < frames_.size(); ++k) {
      auto &caps = frames_[k].block->captures;
      auto pos = std::find(caps.begin(), caps.end(), cap);
      uint32_t index = (uint32_t)(pos - caps.begin());
      if (pos == caps.end()) caps.push_back(cap);
      cap = {true, index};
    }
    v.where = VarWhere::Capture;
    v.slot = cap.index;
    return true;
  }
  return false;
}

// Super:message, in a method where no variable is named Super, and
// Self:Super:message (::Super:message) in a method, send the ancestors'
// member to Self; elsewhere they are ordinary sends.
void Sema::superSend(SendExpr &m) {
  bool inMethod = curFunc_ && curFunc_->ofClass;
  if (m.object->kind == ExprKind::Var) {
    auto &object = static_cast<VarExpr &>(*m.object);
    if (!inMethod || resolve(object) || memvarCreated(object.name)) {
      m.super = false;
      return;
    }
    auto self = std::make_unique<VarExpr>(object.loc, "SELF");
    self->methodSelf = true;
    m.object = std::move(self);
    return;
  }
  if (!inMethod) {
    m.super = false;
    return;
  }
  m.object = std::move(static_cast<SendExpr &>(*m.object).object);
}

// A codeblock's frame: its parameters, then an extended codeblock's LOCALs, which inner codeblocks capture and
// references reach like a function's (BlockExpr::boxed).
void Sema::codeblock(BlockExpr &b, Scope &s) {
  frames_.push_back({{}, {}, {}, &b.boxed, &b});
  uint32_t n = 0;
  for (const Param &p : b.params) {
    if (!frames_.back().names.emplace(p.name, Binding{VarWhere::Frame, n}).second)
      diag_.error(p.loc, "duplicate parameter '" + p.name + "'");
    else ++n;
  }
  for (auto &e : b.body) expr(*e, s);
  if (b.extended) { // its statements, a frame of their own after the parameters
    Scope inner;
    inner.next = n;
    inner.inBlock = true;
    block(b.statements, inner);
    n = inner.next;
  }
  b.numLocals = n;
  b.boxed.resize(n);
  frames_.pop_back();
}

void Sema::block(Block &b, Scope &s) {
  for (auto &st : b) {
    // PARAMETERS may come before the LOCALs, as the reference accepts.
    if (st->kind != StmtKind::Local && st->kind != StmtKind::Static && st->kind != StmtKind::Memvar &&
        st->kind != StmtKind::Parameters)
      s.seenExecutable = true;
    stmt(*st, s);
  }
}

void Sema::nestedBlock(Block &b, Scope &s) {
  ++s.blockDepth;
  block(b, s);
  --s.blockDepth;
}

void Sema::stmt(Stmt &st, Scope &s) {
  switch (st.kind) {
  case StmtKind::Expr:
    expr(*static_cast<ExprStmt &>(st).expr, s);
    break;
  case StmtKind::Local:
  case StmtKind::Static: {
    bool isStatic = st.kind == StmtKind::Static;
    if (isStatic && s.inBlock) diag_.error(st.loc, "STATIC is not allowed in a codeblock");
    if (s.seenExecutable || s.blockDepth)
      diag_.error(st.loc, std::string(isStatic ? "STATIC" : "LOCAL") + " declaration follows executable statement");
    for (LocalDecl &d : static_cast<LocalStmt &>(st).decls) declare(d, isStatic, s);
    break;
  }
  case StmtKind::Memvar:
    if (s.inBlock) diag_.error(st.loc, "MEMVAR is not allowed in a codeblock");
    if (s.seenExecutable || s.blockDepth) diag_.error(st.loc, "MEMVAR declaration follows executable statement");
    for (const LocalDecl &d : static_cast<LocalStmt &>(st).decls) declareMemvar(d);
    break;
  case StmtKind::Private:
  case StmtKind::Public:
    if (s.inBlock) diag_.error(st.loc, "PRIVATE and PUBLIC are not allowed in a codeblock");
    if (curFunc_->generator) diag_.error(st.loc, "PRIVATE and PUBLIC are not allowed in a generator");
    for (LocalDecl &d : static_cast<LocalStmt &>(st).decls) {
      if (d.init) expr(*d.init, s); // before the variable exists: it reads the one visible
      if (d.macro) expr(*d.macro, s);
      else createdMemvar(d);
    }
    break;
  case StmtKind::Parameters: // PRIVATEs holding the arguments received
    if (s.inBlock) diag_.error(st.loc, "PARAMETERS is not allowed in a codeblock");
    if (curFunc_->generator) diag_.error(st.loc, "PARAMETERS is not allowed in a generator");
    curFunc_->readsArgs = true;
    for (const LocalDecl &d : static_cast<LocalStmt &>(st).decls) createdMemvar(d);
    break;
  case StmtKind::ForEach: {
    auto &f = static_cast<ForEachStmt &>(st);
    auto loopVar = [&](Expr &v) {
      expr(v, s);
      if (v.kind == ExprKind::Var && static_cast<const VarExpr &>(v).where == VarWhere::Memvar)
        diag_.error(v.loc, "a memory variable as the variable of FOR EACH is not supported yet");
    };
    if (f.key) loopVar(*f.key);
    for (auto &v : f.vars) loopVar(*v);
    for (auto &c : f.collections) expr(*c, s);
    ++s.loopDepth;
    nestedBlock(f.body, s);
    --s.loopDepth;
    break;
  }
  case StmtKind::Switch: {
    auto &w = static_cast<SwitchStmt &>(st);
    expr(*w.value, s);
    ++s.switchDepth;
    std::set<int64_t> ints;
    std::set<std::string> strings;
    for (auto &c : w.cases) {
      if (c.value) {
        literalStrings(c.value);
        expr(*c.value, s);
        ExprKind k = c.value->kind;
        bool fresh = true;
        if (k == ExprKind::Number && static_cast<const NumberExpr &>(*c.value).isInt)
          fresh = ints.insert(static_cast<const NumberExpr &>(*c.value).ival).second;
        else if (k == ExprKind::String)
          fresh = strings.insert(static_cast<const StringExpr &>(*c.value).value).second;
        else
          diag_.error(c.value->loc, "CASE requires either numeric or string constant");
        if (!fresh) diag_.error(c.value->loc, "duplicate CASE value");
      }
      nestedBlock(c.body, s);
    }
    --s.switchDepth;
    break;
  }
  case StmtKind::Return: {
    auto &r = static_cast<ReturnStmt &>(st);
    if (s.fence) diag_.error(r.loc, std::string("RETURN cannot leave ") + s.fence);
    if (r.value && curFunc_->generator && !s.inBlock)
      diag_.error(r.loc, "a generator cannot RETURN a value: YIELD gives its elements");
    if (r.value) {
      if (curFunc_->isProcedure && !curFunc_->implicit && !s.inBlock)
        diag_.error(r.loc, "PROCEDURE '" + curFunc_->name + "' cannot return a value");
      expr(*r.value, s);
    }
    break;
  }
  case StmtKind::If: {
    auto &i = static_cast<IfStmt &>(st);
    for (auto &arm : i.arms) {
      expr(*arm.cond, s);
      nestedBlock(arm.body, s);
    }
    nestedBlock(i.elseBody, s);
    break;
  }
  case StmtKind::While: {
    auto &w = static_cast<WhileStmt &>(st);
    expr(*w.cond, s);
    ++s.loopDepth;
    nestedBlock(w.body, s);
    --s.loopDepth;
    break;
  }
  case StmtKind::For: {
    auto &f = static_cast<ForStmt &>(st);
    expr(*f.var, s);
    expr(*f.start, s);
    expr(*f.limit, s);
    if (f.step) expr(*f.step, s);
    ++s.loopDepth;
    nestedBlock(f.body, s);
    --s.loopDepth;
    break;
  }
  case StmtKind::Yield:
    expr(*static_cast<YieldStmt &>(st).value, s);
    if (s.inSequence) diag_.error(st.loc, "YIELD cannot be inside BEGIN SEQUENCE or TRY");
    break;
  case StmtKind::WithObject: {
    auto &w = static_cast<WithObjectStmt &>(st);
    expr(*w.object, s);
    declare(w.hidden, false, s);
    nestedBlock(w.body, s);
    break;
  }
  case StmtKind::Sequence: {
    auto &q = static_cast<SequenceStmt &>(st);
    if (q.handler) expr(*q.handler, s);
    bool wasInSequence = s.inSequence;
    s.inSequence = true;
    auto fenced = [&](Block &b, const char *fence) { // b, which RETURN, EXIT and LOOP may not leave
      Scope inner = s;
      inner.fence = fence;
      inner.fenceLoops = s.loopDepth;
      inner.fenceSwitches = s.switchDepth;
      nestedBlock(b, inner);
      s.next = inner.next;
      s.seenExecutable = inner.seenExecutable;
    };
    if (q.isTry) nestedBlock(q.body, s);
    else fenced(q.body, "a BEGIN SEQUENCE");
    if (q.usingVar) expr(*q.usingVar, s);
    nestedBlock(q.recover, s);
    if (q.hasAlways) fenced(q.always, q.isTry ? "FINALLY" : "ALWAYS");
    s.inSequence = wasInSequence;
    break;
  }
  case StmtKind::Exit: // leaves the innermost loop or SWITCH
    if (!s.loopDepth && !s.switchDepth) diag_.error(st.loc, "EXIT outside of a loop");
    else if (s.fence && s.loopDepth == s.fenceLoops && s.switchDepth == s.fenceSwitches)
      diag_.error(st.loc, std::string("EXIT cannot leave ") + s.fence);
    break;
  case StmtKind::Loop:
    if (!s.loopDepth) diag_.error(st.loc, "LOOP outside of a loop");
    else if (s.fence && s.loopDepth == s.fenceLoops) diag_.error(st.loc, std::string("LOOP cannot leave ") + s.fence);
    break;
  }
}

void Sema::expr(Expr &e, Scope &s) {
  switch (e.kind) {
  case ExprKind::Nil: case ExprKind::Logical: case ExprKind::Number: case ExprKind::String:
    break;
  case ExprKind::Spread: // the arguments the function received past its parameters
    if (frames_.back().block)
      diag_.error(e.loc, "'...' cannot be used inside a codeblock");
    else if (!frames_.back().boxed || !curFunc_->variadic)
      diag_.error(e.loc, "'...' is valid only in a function declared with '...' after its parameters");
    break;
  case ExprKind::Var: {
    auto &v = static_cast<VarExpr &>(e);
    if (v.methodSelf && !(curFunc_ && curFunc_->ofClass)) { // even where a variable is named Self
      diag_.error(v.loc, "'::' is valid only in a method");
      break;
    }
    if (resolve(v)) break;
    v.where = VarWhere::Memvar; // looked up by name when it runs, as in Clipper
    if (memvarCreated(v.name) || undeclared_ == Undeclared::Ignore) break;
    std::string text = "'" + v.name + "' is not declared: a memory variable found by name at run time (MEMVAR or M->" +
                       v.name + " declares one)";
    if (undeclared_ == Undeclared::Error) diag_.error(v.loc, text);
    else diag_.warning(v.loc, text);
    break;
  }
  case ExprKind::ArrayLit:
    for (auto &el : static_cast<ArrayLitExpr &>(e).elems) expr(*el, s);
    break;
  case ExprKind::HashLit: {
    auto &h = static_cast<HashLitExpr &>(e);
    for (size_t k = 0; k < h.keys.size(); ++k) {
      expr(*h.keys[k], s);
      expr(*h.values[k], s);
    }
    break;
  }
  case ExprKind::Index: {
    auto &ix = static_cast<IndexExpr &>(e);
    expr(*ix.base, s);
    expr(*ix.index, s);
    break;
  }
  case ExprKind::Block:
    codeblock(static_cast<BlockExpr &>(e), s);
    break;
  case ExprKind::Call: {
    auto &c = static_cast<CallExpr &>(e);
    for (auto &a : c.args) expr(*a, s);
    if (c.callee && c.callee->privates) {
      if (Frame &f = frames_.back(); f.block) f.block->callsPrivates = true;
      else if (curFunc_) curFunc_->callsPrivates = true;
    }
    if (!c.callee || !c.callee->args) break;
    // The callee reads the arguments this function or codeblock received, in its frame; one whose parameters it
    // also reads and writes keeps them there, as written (a codeblock's own parameters always stay there).
    if (Frame &f = frames_.back(); f.block) {
      f.block->readsArgs = true;
    } else if (f.boxed) {
      curFunc_->readsArgs = true;
      if (c.callee->params)
        for (uint32_t k = 0; k < curFunc_->params.size(); ++k) markBoxed(f, k);
    }
    break;
  }
  case ExprKind::Send: {
    auto &m = static_cast<SendExpr &>(e);
    if (m.super) superSend(m);
    expr(*m.object, s);
    for (auto &a : m.args) expr(*a, s);
    break;
  }
  case ExprKind::Unary:
    expr(*static_cast<UnaryExpr &>(e).operand, s);
    break;
  case ExprKind::Binary: {
    auto &b = static_cast<BinaryExpr &>(e);
    expr(*b.lhs, s);
    expr(*b.rhs, s);
    break;
  }
  case ExprKind::And: case ExprKind::Or: {
    auto &b = static_cast<LogicExpr &>(e);
    expr(*b.lhs, s);
    expr(*b.rhs, s);
    break;
  }
  case ExprKind::Assign: {
    auto &a = static_cast<AssignExpr &>(e);
    expr(*a.value, s);
    expr(*a.target, s);
    break;
  }
  case ExprKind::IncDec:
    expr(*static_cast<IncDecExpr &>(e).target, s);
    break;
  case ExprKind::Ref: {
    // A variable passed by reference is shared with the callee: a function's
    // or a codeblock's LOCAL or parameter is marked; STATICs and captured
    // variables are reached with a run-time test anyway.
    Expr &target = *static_cast<RefExpr &>(e).target;
    expr(target, s);
    if (target.kind == ExprKind::Var) {
      auto &v = static_cast<VarExpr &>(target);
      if (v.where == VarWhere::Frame) markBoxed(frames_.back(), v.slot);
    }
    break;
  }
  case ExprKind::List:
    for (auto &x : static_cast<ListExpr &>(e).exprs) expr(*x, s);
    break;
  case ExprKind::IIf: {
    auto &i = static_cast<IIfExpr &>(e);
    expr(*i.cond, s);
    expr(*i.then, s);
    expr(*i.otherwise, s);
    break;
  }
  case ExprKind::MacroText: // the names are memory variables: a LOCAL or STATIC would never be seen
    for (std::string_view written : MacroTextExpr::names(static_cast<MacroTextExpr &>(e).value)) {
      std::string name = upperCase(written);
      if (declaredVariable(name))
        diag_.error(e.loc, "'&" + name + "' names a LOCAL, STATIC or parameter, which a macro's text cannot see");
    }
    break;
  case ExprKind::Macro: // the text is evaluated here; what it names, when it runs (never this function's LOCALs)
    expr(*static_cast<MacroExpr &>(e).text, s);
    break;
  case ExprKind::MacroCall: {
    auto &m = static_cast<MacroCallExpr &>(e);
    expr(*m.text, s);
    for (auto &a : m.args) expr(*a, s);
    break;
  }
  }
}

} // namespace xb
