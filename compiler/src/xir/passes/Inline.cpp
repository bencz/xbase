// Inlining (see Passes.h): calls of the module's functions and Evals of codeblock literals run their callee's body in
// place.
//
// The body runs in the calling function between inline.enter and inline.leave, so the call stack shows the callee as
// its call would (a body in which nothing can see the call stack loses them: trimInlinedBodies). Its parameters and
// LOCALs become variables of the function, set again on every entry (a call starts with fresh ones): the arguments,
// NIL past the last and for the LOCALs. A codeblock's captures are the variables it captured, and its result is the
// call's. Once the block is no longer made, a variable that only it captured needs no cell any more and can live in a
// register.
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"
#include "xir/Builder.h"
#include "xir/Transform.h"

#include <algorithm>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace xb::xir {

namespace {

// A codeblock bigger than this stays a call. Its body is copied only once, where the literal is.
constexpr size_t kMaxBlockInstructions = 200;
// A function bigger than this stays a call: its body is copied at each call.
constexpr size_t kMaxFunctionInstructions = 30;
// ... unless the call passes a codeblock literal, which the inlined body may then run in place too (inline-blocks):
// up to this.
constexpr size_t kMaxFunctionWithBlockInstructions = 200;
// Nothing is inlined into a function that has grown past this.
constexpr size_t kMaxCallerInstructions = 3000;


std::vector<BasicBlock *> allBlocks(const Function &f) {
  std::vector<BasicBlock *> out;
  for (auto &b : f.blocks) out.push_back(b.get());
  return out;
}

// The blocks of a callee whose body can run in its caller's frame: they read no parameters in their frame slots
// (PValue(): the inlined frame has none) and no `...`, create no PRIVATE (it would be the caller's, released later),
// and have no unwind block (BEGIN SEQUENCE). The arguments as received (PCount()) are the inlined frame's, and a
// variable a block captures gets a fresh cell on every entry, whose store replaces the cell an earlier entry left.
bool inlinable(const Function &callee, const std::vector<BasicBlock *> &body, size_t maxInstructions) {
  if (callee.readsParams || callee.usesRest || callee.callsPrivates) return false;
  if (instructionCount(body) > maxInstructions) return false;
  for (const BasicBlock *blk : body) {
    if (blk->unwind) return false;
    for (auto &i : blk->insts)
      if (createsPrivates(i->op) || i->op == Op::ArgsRest || i->op == Op::CallForward || isSuspension(i->op))
        return false;
  }
  return true;
}

// A call that can be inlined: without `...`. In a BEGIN SEQUENCE, its body unwinds through a block that leaves its
// frame (unwindBody).
bool inlinableCall(const Instruction &call) { return call.op == Op::Call && !call.hasRest(); }

// The arguments an inlined body holds while it runs, as the callee's parameters would hold them, in their positions (a
// release of a parameter's LOCAL lets go of its own): one whose hold nothing sees (heldUnseen) is NIL there; constants
// hold nothing (none at the end).
std::vector<Value *> held(Builder &builder, const std::vector<Value *> &args) {
  std::vector<Value *> out;
  for (Value *a : args) out.push_back(heldUnseen(a) ? builder.nil() : a);
  while (!out.empty() && asConstant(out.back())) out.pop_back();
  return out;
}

bool anyReference(const std::vector<Value *> &args) {
  return std::any_of(args.begin(), args.end(), isReference);
}

// What runs in place of a call: the blocks `body` of `callee`, from `start`, with parameter k given the call's operand
// firstArg + k (read when it is inlined: an earlier call inlined may have replaced one) and capture k the variable
// captures[k]; capture k by value (null there) a new variable of the caller's, given its value `captured` at the
// entry.
struct Inlined {
  Function *callee;
  std::vector<BasicBlock *> body;
  BasicBlock *start;
  size_t firstArg;
  std::vector<Var *> captures;
  std::vector<std::pair<size_t, Value *>> captured;
};

// The public function `name` of a carried runtime source, and its module; null when none has one.
std::pair<Function *, const Module *> libraryFunction(const Module &m, const std::string &name) {
  for (const Module *l : m.library)
    for (auto &f : l->functions)
      if (f->kind == Function::Kind::Program && !f->internal && f->name == name) return {f.get(), l};
  return {nullptr, nullptr};
}

std::vector<Value *> operandsFrom(const Instruction &call, size_t first) {
  std::vector<Value *> out;
  for (size_t a = first; a < call.operands().size(); ++a) out.push_back(call.operand(a));
  return out;
}

// Whether a block that `body` (a codeblock's) makes captures that codeblock's capture `k` (passes its cell on).
bool capturedByBlock(const std::vector<BasicBlock *> &body, uint32_t k) {
  for (const BasicBlock *b : body)
    for (auto &i : b->insts)
      for (const Var *c : i->captures)
        if (i->op == Op::MakeBlock && c->kind == Var::Kind::Capture && c->index == k) return true;
  return false;
}

void inlineCall(Function &f, Instruction *call, const Inlined &in) {
  const Function &callee = *in.callee;
  std::vector<Value *> args = operandsFrom(*call, in.firstArg);
  BasicBlock *at = call->parent;
  size_t k = 0;
  while (at->insts[k].get() != call) ++k;
  BasicBlock *tail = splitAfter(f, at, k);
  BlockParam *result = tail->addParam(Type::Item);

  // The callee's variables (bodyVars), and the body, copied with constants of the function's own, using them; each
  // `ret` the end of the inlined frame, passing its value on, after its variables are cleared (leaveBody).
  std::vector<Var *> captures = in.captures;
  for (const auto &c : in.captured) {
    captures[c.first] = f.addFrameVar("");
    captures[c.first]->shared = capturedByBlock(in.body, (uint32_t)c.first); // a block within takes its cell
  }
  VarMap vars = bodyVars(f, callee, anyReference(args), captures);
  BlockCopy body = copyBlocks(f, in.body, ".inline", [&](Value *v) -> Value * {
    const Constant *c = asConstant(v);
    return c ? copyConstant(f, *c) : v;
  });
  useBodyVars(body, vars);
  BasicBlock *handler = at->unwind, *pad = unwindBody(f, callee, body, handler);
  Builder builder(&f);
  for (auto [orig, copy] : body.pairs) {
    auto *ni = copy->kind() == Value::Kind::Instruction ? static_cast<Instruction *>(copy) : nullptr;
    if (!ni || ni->op != Op::Ret) continue;
    BasicBlock *nb = ni->parent;
    Value *value = ni->operands().empty() ? nullptr : ni->operand(0);
    SourceLoc loc = ni->loc;
    nb->takeTerminator();
    builder.setBlock(nb);
    builder.setLoc(loc);
    if (handler) { // the way out in a block that unwinds where the call's does
      BasicBlock *out = f.addBlock(nb->name + ".leave");
      out->unwind = handler;
      builder.br(out);
      builder.setBlock(out);
    }
    leaveBody(builder, callee, vars);
    builder.inlineLeave();
    builder.br(tail, {value ? value : builder.nil()});
  }

  // The entry: the frame shown, the parameters given their arguments (NIL past the last) and the LOCALs NIL.
  at->takeTerminator();
  builder.setBlock(at);
  builder.setLoc(call->loc);
  if (handler) { // the way in in a block that unwinds where the body's do
    BasicBlock *enter = f.addBlock("inline.enter");
    enter->unwind = pad;
    builder.br(enter);
    builder.setBlock(enter);
  }
  builder.inlineEnter(in.callee, callee.readsArgs ? args : held(builder, args));
  enterBody(builder, callee, vars, args);
  for (const auto &c : in.captured) builder.store(captures[c.first], Access::Direct, c.second);
  builder.br(body.blocks.at(in.start));
  call->replaceAllUsesWith(result);
  eraseInstruction(call);
}

// What the folding left of the inlined bodies. A body that no longer reads its arguments as received (the call of an
// ARGS function that did, gone with a branch never taken) keeps only those it holds (`held`). A body in which nothing
// can see the call stack (no call, nothing that can fail or run code, no loop, whose back edge polls and may run
// destructors, no body inlined in it) needs no frame: its inline.enter and inline.leave go (nothing can collect there:
// its arguments need no holding). A leave that several bodies reach keeps them all.
bool trimInlinedBodies(Function &f) {
  CFG cfg(f);
  std::unordered_map<const BasicBlock *, size_t> rpo;
  for (const BasicBlock *b : cfg.order()) rpo.emplace(b, rpo.size());
  std::unordered_set<const Instruction *> read, seen, enters;
  std::unordered_map<const Instruction *, std::unordered_set<const Instruction *>> leaves; // leave -> its enters
  walkBodies<const Instruction *>(cfg, [&](const BasicBlock &b, auto &open) {
    bool changed = false;
    auto see = [&](const std::unordered_set<const Instruction *> &es) {
      for (const Instruction *e : es) changed |= seen.insert(e).second;
    };
    for (auto &i : b.insts) {
      // A back edge (to a block no later in reverse postorder) polls the pending requests (CodeGen::pollOnBackEdge).
      bool backEdge = std::any_of(i->targets.begin(), i->targets.end(),
                                  [&](const Target &t) { return rpo.at(t.block) <= rpo.at(&b); });
      bool shows = i->op == Op::InlineEnter || i->mayFail() || (i->effects() & RunsCode) || backEdge;
      if (i->op == Op::InlineLeave) {
        for (const Instruction *e : open.back()) changed |= leaves[i.get()].insert(e).second;
        if (open.back().size() > 1) see(open.back());
        open.pop_back();
        continue;
      }
      if (shows)
        for (auto &level : open) see(level);
      if (i->op == Op::Call && i->callee.readsCallerArgs && !open.empty())
        for (const Instruction *e : open.back()) changed |= read.insert(e).second;
      if (i->op == Op::InlineEnter) {
        enters.insert(i.get());
        open.push_back({i.get()});
      }
    }
    return changed;
  });
  bool changed = false;
  Builder builder(&f);
  for (auto &b : f.blocks) {
    for (auto &i : b->insts) {
      if (i->op != Op::InlineEnter || read.count(i.get())) continue;
      std::vector<Value *> ops(i->operands().begin(), i->operands().end()), keep = held(builder, ops);
      if (keep == ops) continue;
      i->dropOperands();
      for (Value *v : keep) i->addOperand(v);
      changed = true;
    }
    changed |= std::erase_if(b->insts, [&](const std::unique_ptr<Instruction> &i) {
                 if (i->op == Op::InlineEnter) return enters.count(i.get()) && !seen.count(i.get());
                 if (i->op != Op::InlineLeave) return false;
                 auto l = leaves.find(i.get());
                 return l != leaves.end() && !l->second.empty() &&
                        std::none_of(l->second.begin(), l->second.end(),
                                     [&](const Instruction *e) { return seen.count(e); });
               }) > 0;
  }
  return changed;
}

// A function that calls itself: inlining it would only unroll the recursion. (Both bodies of a function versioned for
// references call the function itself.)
bool recursive(const Function &f) {
  for (auto &b : f.blocks)
    for (auto &i : b->insts)
      if (i->op == Op::Call && i->callee.function == &f) return true;
  return false;
}

// The variables bound anew: by an inlined body, at its entry and before it ends (direct stores of shared variables,
// which the code generated from the source writes through their cells), and by a FOR EACH (each.enter): a block made
// while one was bound holds the cell of that time (or, for a loop variable, a cell of its own with the element of
// that time), not what the variable holds later.
std::unordered_set<const Var *> reboundVars(const Function &f) {
  std::unordered_set<const Var *> out;
  for (auto &b : f.blocks)
    for (auto &i : b->insts)
      if ((i->op == Op::Store && i->access == Access::Direct && i->var->shared) || i->op == Op::EachEnter)
        out.insert(i->var);
  return out;
}

// The Eval of a codeblock literal of the function (`call EVAL native %block, args...`, none of the block's captured
// variables bound anew): what runs in its place, or nothing. The block is immutable and its body reaches the cells it
// captured through the same variables, so its other uses (another Eval, a call it is passed to) may keep it; a value
// it captured (a cell of its own) is the value the block was made with.
std::optional<Inlined> blockOf(const Function &f, const Instruction &call,
                               const std::unordered_set<const Var *> &rebound) {
  if (!isEvalCall(call)) return std::nullopt;
  const Instruction *mk = asInstruction(call.operand(0), Op::MakeBlock);
  if (!mk) return std::nullopt;
  for (const Var *c : mk->captures)
    if (rebound.count(c)) return std::nullopt;
  Function *blk = mk->blockFn;
  if (blk == &f) return std::nullopt; // a block that runs itself (a call inlined in it made it)
  std::vector<BasicBlock *> body = allBlocks(*blk);
  if (!inlinable(*blk, body, kMaxBlockInstructions)) return std::nullopt;
  if (mk->valueCells.empty()) return Inlined{blk, std::move(body), blk->entry(), 1, mk->captures, {}};
  std::vector<Var *> cells;
  std::vector<std::pair<size_t, Value *>> captured;
  size_t var = 0, value = 0;
  for (bool byValue : mk->valueCells) {
    if (byValue) captured.push_back({cells.size(), mk->operand(value++)});
    cells.push_back(byValue ? nullptr : mk->captures[var++]);
  }
  return Inlined{blk, std::move(body), blk->entry(), 1, std::move(cells), std::move(captured)};
}

// A call of a function of the module, or of a public function of a carried runtime source (`from` its module): what
// runs in its place, or nothing. Arguments without references run the body for them (after the versioning test, which
// they pass); with one, the body versioned for references.
std::optional<Inlined> functionOf(const Module &m, const Function &f, const Instruction &call) {
  if (call.op != Op::Call) return std::nullopt;
  Function *callee = call.callee.function;
  const Module *from = nullptr;
  if (!callee && call.callee.native) std::tie(callee, from) = libraryFunction(m, call.callee.name);
  if (!inlinableCall(call) || !callee || callee == &f || callee->kind != Function::Kind::Program ||
      callee->startup || isResumable(*callee) || recursive(*callee))
    return std::nullopt;
  std::vector<BasicBlock *> body;
  BasicBlock *start = callee->entry();
  if (const Instruction *test = versioning(*callee)) {
    BasicBlock *forward = test->targets[0].block;
    if (anyReference(operandsFrom(call, 0))) {
      callee = referenceBody(*test);
      if (callee == &f) return std::nullopt;
      body = allBlocks(*callee);
      start = callee->entry();
    } else {
      start = test->targets[1].block;
      for (auto &b : callee->blocks)
        if (b.get() != callee->entry() && b.get() != forward) body.push_back(b.get());
    }
  } else {
    body = allBlocks(*callee);
  }
  bool passesBlock = std::any_of(call.operands().begin(), call.operands().end(),
                                 [](const Value *v) { return asInstruction(v, Op::MakeBlock); });
  if (!inlinable(*callee, body, passesBlock ? kMaxFunctionWithBlockInstructions : kMaxFunctionInstructions) ||
      ((from || callee->carried) && !selfContained(body)))
    return std::nullopt;
  return Inlined{callee, std::move(body), start, 0, {}, {}};
}

// Inlines the calls found while the function stays within kMaxCallerInstructions (an Eval's block that nothing else
// uses any more is no longer made); true when it inlined one.
bool inlineFound(Function &f, std::vector<std::pair<Instruction *, Inlined>> &found, bool evals) {
  size_t size = instructionCount(allBlocks(f));
  bool changed = false;
  for (auto &[call, in] : found) {
    if (size > kMaxCallerInstructions) break;
    auto *mk = evals ? static_cast<Instruction *>(call->operand(0)) : nullptr;
    inlineCall(f, call, in);
    if (mk && mk->users().empty()) eraseInstruction(mk);
    size += instructionCount(in.body);
    changed = true;
  }
  return changed;
}

} // namespace

bool inlineBlocks(Function &f) {
  if (isResumable(f)) return false; // its values live in the frame
  std::vector<std::pair<Instruction *, Inlined>> found;
  std::unordered_set<const Var *> rebound = reboundVars(f);
  for (auto &b : f.blocks)
    for (auto &i : b->insts)
      if (auto in = blockOf(f, *i, rebound)) found.push_back({i.get(), std::move(*in)});
  if (!inlineFound(f, found, true)) return false;
  unshareVariables(f);
  return true;
}

bool unshareVariables(Function &f) {
  std::unordered_map<const Var *, bool> needed;
  for (auto &b : f.blocks)
    for (auto &i : b->insts) {
      for (const Var *c : i->captures) needed[c] = true;
      if (i->var && i->op != Op::Load && i->op != Op::Store && i->op != Op::Barrier) needed[i->var] = true;
    }
  bool changed = false;
  for (auto &v : f.vars)
    if (v->kind == Var::Kind::Frame && v->shared && !v->param && !needed[v.get()]) {
      v->shared = false;
      changed = true;
    }
  for (auto &b : f.blocks) {
    for (auto &i : b->insts)
      if (plainVar(i->var) && i->access == Access::MaybeRef) {
        i->access = Access::Direct;
        changed = true;
      }
    changed |= std::erase_if(b->insts, [](auto &i) { return i->op == Op::Barrier && i->access == Access::Direct; }) > 0;
  }
  return changed;
}

bool inlineFunctions(Module &m, Function &f) {
  if (isResumable(f)) return false;
  bool trimmed = trimInlinedBodies(f);
  std::vector<std::pair<Instruction *, Inlined>> found;
  for (auto &b : f.blocks)
    for (auto &i : b->insts)
      if (auto in = functionOf(m, f, *i)) found.push_back({i.get(), std::move(*in)});
  return inlineFound(f, found, false) || trimmed;
}

} // namespace xb::xir
