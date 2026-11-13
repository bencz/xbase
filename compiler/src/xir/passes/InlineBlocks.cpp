// Codeblock literals that Eval runs where they are made, inlined (see Passes.h).
//
// `Eval( {| p | body }, x )`: the block is made only to be called once, right there. Its body runs in the creating
// function instead, between inline.enter and inline.leave (the call stack shows the block, as its call would): its
// parameters and LOCALs become variables of the function (set again on every entry: a block call starts with fresh
// ones), its captures the variables it captured, and its result the call's. The block is no longer made, so a variable
// that only it captured needs no cell any more and can live in a register.
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"
#include "xir/Builder.h"
#include "xir/Transform.h"

#include <algorithm>
#include <unordered_map>

namespace xb::xir {

namespace {

// A block function bigger than this stays a call: its body is copied at each Eval.
constexpr size_t kMaxInstructions = 200;

// A block that can run in its creator's frame: it reads no arguments as received (PCount(), PValue(), `...`),
// creates no PRIVATE (it would be the creator's, released later), has no unwind block (a BEGIN SEQUENCE of an
// extended block), and no nested block captures its own variables (one made on an earlier pass would share the cell
// a later pass writes: a call gives each pass fresh variables).
bool inlinable(const Function &b) {
  if (b.readsArgs || b.usesRest || b.callsPrivates) return false;
  for (auto &v : b.vars)
    if (v->kind == Var::Kind::Frame && v->shared) return false;
  size_t n = 0;
  for (auto &blk : b.blocks) {
    if (blk->unwind) return false;
    for (auto &i : blk->insts) {
      if (createsPrivates(i->op) || i->op == Op::ArgsRest || i->op == Op::CallForward || i->op == Op::GenYield)
        return false;
      if (++n > kMaxInstructions) return false;
    }
  }
  return true;
}

// The Eval of a block literal made only for it: `call EVAL native %block, args...` (no `...`), where the block has no
// other use, in a block that unwinds to the function's exit (an unwind block would leave the inlined frame shown).
Instruction *blockOf(const Instruction &call) {
  if (call.op != Op::Call || call.callee.function || !call.callee.native || call.callee.name != "EVAL" ||
      call.hasRest() || call.operands().empty() || call.parent->unwind)
    return nullptr;
  Value *b = call.operand(0);
  if (b->kind() != Value::Kind::Instruction) return nullptr;
  auto *mk = static_cast<Instruction *>(b);
  if (mk->op != Op::MakeBlock || mk->users().size() != 1 || !inlinable(*mk->blockFn)) return nullptr;
  return mk;
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

void inlineCall(Function &f, Instruction *call, Instruction *mk) {
  Function &blk = *mk->blockFn;
  BasicBlock *at = call->parent;
  size_t k = 0;
  while (at->insts[k].get() != call) ++k;
  BasicBlock *tail = splitAfter(f, at, k);
  BlockParam *result = tail->addParam(Type::Item);

  // The block's variables: its captures are the cells' variables; its frame variables, new ones of the function. Its
  // parameters, when an argument is a reference (@x), keep reading and writing through what they hold.
  bool refArgs = false;
  for (size_t a = 1; a < call->operands().size(); ++a) refArgs = refArgs || isReference(call->operand(a));
  std::unordered_map<const Var *, Var *> vars;
  for (auto &v : blk.vars) {
    if (v->kind == Var::Kind::Capture) {
      vars[v.get()] = mk->captures[v->index];
      continue;
    }
    Var *nv = f.addFrameVar(v->name);
    nv->param = v->param && refArgs;
    vars[v.get()] = nv;
  }
  auto varOf = [&](Var *v) { // the module's STATICs stay
    auto it = vars.find(v);
    return it == vars.end() ? v : it->second;
  };

  // The body, copied with constants of the function's own; its variables mapped, and each `ret` the end of the
  // inlined frame, passing its value on.
  std::vector<BasicBlock *> originals;
  for (auto &b : blk.blocks) originals.push_back(b.get());
  BlockCopy body = copyBlocks(f, originals, ".inline", [&](Value *v) -> Value * {
    const Constant *c = asConstant(v);
    return c ? copyConstant(f, *c) : v;
  });
  Builder builder(&f);
  for (auto [orig, copy] : body.pairs) {
    auto *ni = copy->kind() == Value::Kind::Instruction ? static_cast<Instruction *>(copy) : nullptr;
    if (!ni) continue;
    if (ni->var) {
      ni->var = varOf(ni->var);
      if (plain(ni->var)) ni->access = Access::Direct;
    }
    for (Var *&c : ni->captures) c = varOf(c);
    if (ni->op != Op::Ret) continue;
    BasicBlock *nb = ni->parent;
    Value *value = ni->operands().empty() ? nullptr : ni->operand(0);
    SourceLoc loc = ni->loc;
    nb->takeTerminator();
    builder.setBlock(nb);
    builder.setLoc(loc);
    builder.inlineLeave();
    builder.br(tail, {value ? value : builder.nil()});
  }

  // The entry: the frame shown, the parameters given their arguments (NIL past the last) and the LOCALs NIL.
  at->takeTerminator();
  builder.setBlock(at);
  builder.setLoc(call->loc);
  builder.inlineEnter(&blk);
  for (auto &v : blk.vars) {
    if (v->kind != Var::Kind::Frame) continue;
    size_t a = v->index + 1;
    Value *init = v->param && a < call->operands().size() ? call->operand(a) : builder.nil();
    builder.store(vars.at(v.get()), Access::Direct, init);
  }
  builder.br(body.blocks.at(blk.entry()));
  call->replaceAllUsesWith(result);
  at->insts.erase(std::find_if(at->insts.begin(), at->insts.end(), [&](auto &p) { return p.get() == call; }));
  BasicBlock *made = mk->parent;
  made->insts.erase(std::find_if(made->insts.begin(), made->insts.end(), [&](auto &p) { return p.get() == mk; }));
}

// A variable that nothing captures or passes by reference any more holds no cell: it is read and written directly
// (no barrier: it is never in the heap), which lets promote-variables take it.
void unshare(Function &f) {
  std::unordered_map<const Var *, bool> needed;
  for (auto &b : f.blocks)
    for (auto &i : b->insts) {
      for (const Var *c : i->captures) needed[c] = true;
      if (i->var && i->op != Op::Load && i->op != Op::Store && i->op != Op::Barrier) needed[i->var] = true;
    }
  for (auto &v : f.vars)
    if (v->kind == Var::Kind::Frame && v->shared && !v->param && !needed[v.get()]) v->shared = false;
  for (auto &b : f.blocks) {
    for (auto &i : b->insts)
      if (plain(i->var) && i->access == Access::MaybeRef) i->access = Access::Direct;
    std::erase_if(b->insts, [](auto &i) { return i->op == Op::Barrier && i->access == Access::Direct; });
  }
}

} // namespace

bool inlineBlocks(Function &f) {
  for (auto &b : f.blocks)
    if (Instruction *t = b->terminator(); t && t->op == Op::GenYield) return false; // a generator: values in the frame
  std::vector<std::pair<Instruction *, Instruction *>> found;
  for (auto &b : f.blocks)
    for (auto &i : b->insts)
      if (Instruction *mk = blockOf(*i)) found.push_back({i.get(), mk});
  for (auto [call, mk] : found) inlineCall(f, call, mk);
  if (found.empty()) return false;
  unshare(f);
  return true;
}

} // namespace xb::xir
