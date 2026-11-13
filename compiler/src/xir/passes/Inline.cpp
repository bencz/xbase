// Inlining (see Passes.h): calls of the module's functions and Evals of codeblock literals run their callee's body in
// place.
//
// The body runs in the calling function between inline.enter and inline.leave, so the call stack shows the callee as
// its call would. Its parameters and LOCALs become variables of the function, set again on every entry (a call starts
// with fresh ones): the arguments, NIL past the last and for the LOCALs. A codeblock's captures are the variables it
// captured, and its result is the call's. Once the block is no longer made, a variable that only it captured needs no
// cell any more and can live in a register.
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

size_t countInstructions(const std::vector<BasicBlock *> &blocks) {
  size_t n = 0;
  for (const BasicBlock *b : blocks) n += b->insts.size();
  return n;
}

std::vector<BasicBlock *> allBlocks(const Function &f) {
  std::vector<BasicBlock *> out;
  for (auto &b : f.blocks) out.push_back(b.get());
  return out;
}

void eraseInstruction(Instruction *i) {
  BasicBlock *b = i->parent;
  b->insts.erase(std::find_if(b->insts.begin(), b->insts.end(), [&](auto &p) { return p.get() == i; }));
}

// The blocks of a callee whose body can run in its caller's frame: they read no parameters in their frame slots
// (PValue(): the inlined frame has none) and no `...`, create no PRIVATE (it would be the caller's, released later),
// and have no unwind block (BEGIN SEQUENCE). The arguments as received (PCount()) are the inlined frame's, and a
// variable a block captures gets a fresh cell on every entry, whose store replaces the cell an earlier entry left.
bool inlinable(const Function &callee, const std::vector<BasicBlock *> &body, size_t maxInstructions) {
  if (callee.readsParams || callee.usesRest || callee.callsPrivates) return false;
  if (countInstructions(body) > maxInstructions) return false;
  for (const BasicBlock *blk : body) {
    if (blk->unwind) return false;
    for (auto &i : blk->insts)
      if (createsPrivates(i->op) || i->op == Op::ArgsRest || i->op == Op::CallForward || i->op == Op::GenYield)
        return false;
  }
  return true;
}

// A call that can be inlined: without `...`, in a block that unwinds to the function's exit (an unwind block would
// leave the inlined frame shown).
bool inlinableCall(const Instruction &call) {
  return call.op == Op::Call && !call.hasRest() && !call.parent->unwind;
}

Constant *copyConstant(Function &f, const Constant &c) {
  f.constants.push_back(std::make_unique<Constant>(c.lit, c.type()));
  Constant *n = f.constants.back().get();
  n->logical = c.logical;
  n->ival = c.ival;
  n->dval = c.dval;
  n->width = c.width;
  n->dec = c.dec;
  n->str = c.str;
  return n;
}

// A frame variable with no cell and no reference passed in: read and written directly.
bool plain(const Var *v) { return v && v->kind == Var::Kind::Frame && !mayBeInCell(*v); }

bool isReference(const Value *v) {
  return asInstruction(v, Op::RefVar) || asInstruction(v, Op::RefIndex) || asInstruction(v, Op::MvRef);
}

bool anyReference(const std::vector<Value *> &args) {
  return std::any_of(args.begin(), args.end(), isReference);
}

// What runs in place of a call: the blocks `body` of `callee`, from `start`, with parameter k given the call's operand
// firstArg + k (read when it is inlined: an earlier call inlined may have replaced one) and capture k the variable
// captures[k].
struct Inlined {
  Function *callee;
  std::vector<BasicBlock *> body;
  BasicBlock *start;
  size_t firstArg;
  std::vector<Var *> captures;
};

// Whether `body`, of a function of a carried runtime source, needs nothing of its module in another one: no
// STATIC of it, none of its classes, no function of it called or made (its STATIC functions and codeblocks run in place
// there already, once its own calls are inlined); it calls the runtime only.
bool selfContained(const std::vector<BasicBlock *> &body) {
  auto isStatic = [](const Var *v) { return v && v->kind == Var::Kind::Static; };
  for (const BasicBlock *b : body)
    for (auto &i : b->insts)
      if (isStatic(i->var) || std::any_of(i->captures.begin(), i->captures.end(), isStatic) ||
          i->op == Op::ClassNew || i->classIndex >= 0 || i->op == Op::MakeBlock || i->callee.function)
        return false;
  return true;
}

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

void inlineCall(Function &f, Instruction *call, const Inlined &in) {
  const Function &callee = *in.callee;
  std::vector<Value *> args = operandsFrom(*call, in.firstArg);
  BasicBlock *at = call->parent;
  size_t k = 0;
  while (at->insts[k].get() != call) ++k;
  BasicBlock *tail = splitAfter(f, at, k);
  BlockParam *result = tail->addParam(Type::Item);

  // The callee's variables: its captures are the cells' variables; its frame variables, new ones of the function. Its
  // parameters, when an argument is a reference (@x), keep reading and writing through what they hold, as do its other
  // variables marked so (the parameters of a body inlined in it before).
  bool refArgs = anyReference(args);
  std::unordered_map<const Var *, Var *> vars;
  for (auto &v : callee.vars) {
    if (v->kind == Var::Kind::Capture) {
      vars[v.get()] = in.captures[v->index];
      continue;
    }
    Var *nv = f.addFrameVar(v->name);
    nv->param = v->param && (refArgs || v->index >= callee.numParams);
    nv->shared = v->shared;
    vars[v.get()] = nv;
  }
  auto varOf = [&](Var *v) { // the module's STATICs stay
    auto it = vars.find(v);
    return it == vars.end() ? v : it->second;
  };

  // The body, copied with constants of the function's own; its variables mapped, and each `ret` the end of the
  // inlined frame, passing its value on, after its variables are cleared: as the callee's frame would at its return,
  // they no longer keep what they held (promote-variables drops the stores of those it takes).
  BlockCopy body = copyBlocks(f, in.body, ".inline", [&](Value *v) -> Value * {
    const Constant *c = asConstant(v);
    return c ? copyConstant(f, *c) : v;
  });
  Builder builder(&f);
  for (auto [orig, copy] : body.pairs) {
    auto *ni = copy->kind() == Value::Kind::Instruction ? static_cast<Instruction *>(copy) : nullptr;
    if (!ni) continue;
    if (ni->var) {
      ni->var = varOf(ni->var);
      if (plain(ni->var) && ni->access == Access::MaybeRef) ni->access = Access::Direct;
    }
    for (Var *&c : ni->captures) c = varOf(c);
    if (ni->op != Op::Ret) continue;
    BasicBlock *nb = ni->parent;
    Value *value = ni->operands().empty() ? nullptr : ni->operand(0);
    SourceLoc loc = ni->loc;
    nb->takeTerminator();
    builder.setBlock(nb);
    builder.setLoc(loc);
    for (auto &v : callee.vars)
      if (v->kind == Var::Kind::Frame) builder.store(vars.at(v.get()), Access::Direct, builder.nil());
    builder.inlineLeave();
    builder.br(tail, {value ? value : builder.nil()});
  }

  // The entry: the frame shown, the parameters given their arguments (NIL past the last) and the LOCALs NIL.
  at->takeTerminator();
  builder.setBlock(at);
  builder.setLoc(call->loc);
  builder.inlineEnter(in.callee, callee.readsArgs ? args : std::vector<Value *>{});
  for (auto &v : callee.vars) {
    if (v->kind != Var::Kind::Frame) continue;
    bool given = v->param && v->index < callee.numParams && v->index < args.size();
    builder.store(vars.at(v.get()), Access::Direct, given ? args[v->index] : builder.nil());
  }
  builder.br(body.blocks.at(in.start));
  call->replaceAllUsesWith(result);
  eraseInstruction(call);
}

// The arguments an inlined body no longer reads (the call of an ARGS function that did, gone with a branch never
// taken) stop being passed: the values they were stay free to go (a codeblock literal no longer made, say).
bool dropUnreadArguments(Function &f) {
  std::unordered_set<const Instruction *> read;
  walkBodies<const Instruction *>(CFG(f), [&](const BasicBlock &b, auto &open) {
    bool changed = false;
    for (auto &i : b.insts) {
      if (i->op == Op::InlineEnter) open.push_back({i.get()});
      else if (i->op == Op::InlineLeave) open.pop_back();
      else if (i->op == Op::Call && i->callee.readsCallerArgs && !open.empty())
        for (const Instruction *e : open.back()) changed |= read.insert(e).second;
    }
    return changed;
  });
  bool changed = false;
  for (auto &b : f.blocks)
    for (auto &i : b->insts)
      if (i->op == Op::InlineEnter && !i->operands().empty() && !read.count(i.get())) {
        i->dropOperands();
        changed = true;
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

bool isGenerator(const Function &f) {
  for (auto &b : f.blocks)
    if (Instruction *t = b->terminator(); t && t->op == Op::GenYield) return true;
  return false;
}

// The variables an inlined body binds anew, at its entry and before it ends (direct stores of shared variables, which
// the code generated from the source writes through their cells): a block made while one was bound holds the cell of
// that time, not what the variable holds later.
std::unordered_set<const Var *> reboundVars(const Function &f) {
  std::unordered_set<const Var *> out;
  for (auto &b : f.blocks)
    for (auto &i : b->insts)
      if (i->op == Op::Store && i->access == Access::Direct && i->var->shared) out.insert(i->var);
  return out;
}

// The Eval of a codeblock literal of the function (`call EVAL native %block, args...`, none of the block's captured
// variables bound anew): what runs in its place, or nothing. The block is immutable and its body reaches the cells it
// captured through the same variables, so its other uses (another Eval, a call it is passed to) may keep it.
std::optional<Inlined> blockOf(const Function &f, const Instruction &call,
                               const std::unordered_set<const Var *> &rebound) {
  if (!inlinableCall(call) || call.callee.function || !call.callee.native || call.callee.name != "EVAL" ||
      call.operands().empty())
    return std::nullopt;
  const Instruction *mk = asInstruction(call.operand(0), Op::MakeBlock);
  if (!mk) return std::nullopt;
  for (const Var *c : mk->captures)
    if (rebound.count(c)) return std::nullopt;
  Function *blk = mk->blockFn;
  if (blk == &f) return std::nullopt; // a block that runs itself (a call inlined in it made it)
  std::vector<BasicBlock *> body = allBlocks(*blk);
  if (!inlinable(*blk, body, kMaxBlockInstructions)) return std::nullopt;
  return Inlined{blk, std::move(body), blk->entry(), 1, mk->captures};
}

// A function versioned for references starts with `params.ref` and a branch to the call of its other body (`byref`)
// or to its own (`body`): that branch, or null.
const Instruction *versioning(const Function &f) {
  const Instruction *t = f.entry()->terminator();
  if (!t || t->op != Op::CondBr || !asInstruction(t->operand(0), Op::ParamsRef)) return nullptr;
  return t;
}

// A call of a function of the module, or of a public function of a carried runtime source (`from` its module): what
// runs in its place, or nothing. Arguments without references run the body for them (after the versioning test, which
// they pass); with one, the body versioned for references.
std::optional<Inlined> functionOf(const Module &m, const Function &f, const Instruction &call) {
  if (call.op != Op::Call) return std::nullopt;
  Function *callee = call.callee.function;
  const Module *from = nullptr;
  if (!callee && call.callee.native) std::tie(callee, from) = libraryFunction(m, call.callee.name);
  if (!inlinableCall(call) || !callee || callee == &f || callee->kind != Function::Kind::Program || callee->method ||
      callee->startup || isGenerator(*callee) || recursive(*callee))
    return std::nullopt;
  std::vector<BasicBlock *> body;
  BasicBlock *start = callee->entry();
  if (const Instruction *test = versioning(*callee)) {
    BasicBlock *forward = test->targets[0].block;
    if (anyReference(operandsFrom(call, 0))) {
      callee = forward->insts.front()->callee.function;
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
      (from && !selfContained(body)))
    return std::nullopt;
  return Inlined{callee, std::move(body), start, 0, {}};
}

// Inlines the calls found while the function stays within kMaxCallerInstructions (an Eval's block that nothing else
// uses any more is no longer made); true when it inlined one.
bool inlineFound(Function &f, std::vector<std::pair<Instruction *, Inlined>> &found, bool evals) {
  size_t size = countInstructions(allBlocks(f));
  bool changed = false;
  for (auto &[call, in] : found) {
    if (size > kMaxCallerInstructions) break;
    auto *mk = evals ? static_cast<Instruction *>(call->operand(0)) : nullptr;
    inlineCall(f, call, in);
    if (mk && mk->users().empty()) eraseInstruction(mk);
    size += countInstructions(in.body);
    changed = true;
  }
  return changed;
}

} // namespace

bool inlineBlocks(Function &f) {
  if (isGenerator(f)) return false; // its values live in the frame
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
      if (plain(i->var) && i->access == Access::MaybeRef) {
        i->access = Access::Direct;
        changed = true;
      }
    changed |= std::erase_if(b->insts, [](auto &i) { return i->op == Op::Barrier && i->access == Access::Direct; }) > 0;
  }
  return changed;
}

bool inlineFunctions(Module &m, Function &f) {
  if (isGenerator(f)) return false;
  bool dropped = dropUnreadArguments(f);
  std::vector<std::pair<Instruction *, Inlined>> found;
  for (auto &b : f.blocks)
    for (auto &i : b->insts)
      if (auto in = functionOf(m, f, *i)) found.push_back({i.get(), std::move(*in)});
  return inlineFound(f, found, false) || dropped;
}

} // namespace xb::xir
