// Generators fused into the FOR EACH that goes through them: a loop over an enumerator made right there (`gen.new` of a
// generator of the module, once its function is inlined) that nothing else sees runs the generator's body in place. The
// body (its resumable form, its values that live across YIELD in frame variables) is copied into the function, between
// inline.enter and inline.leave of the generator, so the call stack shows it as its steps would; it starts where a
// first step would, each YIELD assigns the loop variable and enters the loop's body, the body's end goes on after that
// YIELD (the state, when there are several, in a variable of its own), and the generator's end leaves the loop. The
// loop variable is then an ordinary variable: over an enumerator, assigning it changes nothing else.
//
// A loop is fused when its enumerator goes only into its each.source (ascending, one collection, no key), the source
// only into its each.enter, enum.len and each.move, it is entered as often as its enumerator is made (a loop around it
// goes round the gen.new too), its variable is the frame's alone (not shared, not a parameter) with no other FOR EACH
// on it inside this one, and the generator's body has no unwind block (BEGIN SEQUENCE), keeps no handler across YIELD,
// reads no parameter in its frame (PValue()), creates no PRIVATE, and is small enough. Values the loop carries in its
// header are joined again through the generator's blocks (SSAUpdater). A generator passed to another one (FOR EACH x IN
// Evens( Range() )) fuses in turn once promote-variables shows the inner enumerator's gen.new to the copied loop. In a
// BEGIN SEQUENCE, the body unwinds through a block that leaves its frame (unwindBody).
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"
#include "xir/Builder.h"
#include "xir/Transform.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace xb::xir {

namespace {

constexpr size_t kMaxGenerator = 300;  // instructions of a generator's body copied
constexpr size_t kMaxFunction = 3000;  // nothing is fused into a function that has grown past this

// The resume function's start and the blocks a step runs (from the start and from each YIELD's target), without its
// entry's dispatch; false when its body cannot run in place.
struct Body {
  BasicBlock *start = nullptr;
  std::vector<BasicBlock *> blocks;
  std::vector<Instruction *> yields;
};

bool resumableBody(const Function &r, Body &out) {
  if (r.readsParams || r.callsPrivates || r.vars.empty()) return false; // PCount() and `...`: Sema rejects them
  const Var *gen = r.vars.front().get();
  CFG cfg(r);
  Instruction *restore = nullptr;
  for (BasicBlock *b : cfg.order())
    for (auto &i : b->insts)
      if (i->op == Op::GenRestore) {
        if (restore) return false;
        restore = i.get();
      }
  if (!restore || restore->var != gen) return false;
  // The dispatch: blocks ending in `cond_br state == k` to each YIELD's target, the last one's other way the start
  // (or a block that only branches there).
  auto dispatches = [&](const BasicBlock *b) {
    const Instruction *t = b->terminator();
    const Instruction *cmp = t->op == Op::CondBr ? asInstruction(t->operand(0), Op::IntCmp) : nullptr;
    return cmp && cmp->operand(0) == restore && t->targets[0].args.empty() && t->targets[1].args.empty();
  };
  std::unordered_set<const BasicBlock *> dispatch;
  BasicBlock *at = restore->parent;
  if (!dispatches(at)) return false;
  while (dispatches(at)) {
    dispatch.insert(at);
    at = at->terminator()->targets[1].block;
  }
  out.start = at;
  if (at->insts.size() == 1 && at->terminator()->op == Op::Br && at->terminator()->targets[0].args.empty())
    out.start = at->terminator()->targets[0].block;
  if (dispatch.count(out.start) || !out.start->params.empty()) return false;
  std::unordered_set<const BasicBlock *> seen{out.start};
  std::vector<BasicBlock *> work{out.start};
  while (!work.empty()) {
    BasicBlock *b = work.back();
    work.pop_back();
    out.blocks.push_back(b);
    if (b->unwind) return false;
    for (auto &i : b->insts) {
      if (i->op == Op::GenRestore || createsPrivates(i->op) || i->op == Op::ArgsRest || i->op == Op::CallForward ||
          i->op == Op::SeqWith || i->op == Op::SeqSuspend || i->op == Op::SeqResume || i->op == Op::SeqRestore ||
          i->op == Op::AsyncSuspend || i->op == Op::AsyncYield || i->op == Op::AsyncRestore)
        return false;
      if (i->var == gen && i->op != Op::GenYield && i->op != Op::GenDone) return false;
      for (const Var *c : i->captures)
        if (c == gen) return false;
      if (i->op == Op::GenYield) out.yields.push_back(i.get());
      if (i->op == Op::Ret) { // the end: right after gen.done
        size_t k = b->insts.size() - 1;
        if (k == 0 || b->insts[k - 1]->op != Op::GenDone) return false;
      }
    }
    for (size_t k = 0; k < b->numSuccessors(); ++k) {
      BasicBlock *s = b->successor(k);
      if (dispatch.count(s)) return false;
      if (seen.insert(s).second) work.push_back(s);
    }
  }
  // A generator of a carried runtime source runs in place only on what the runtime exports of its module.
  return !out.yields.empty() && instructionCount(out.blocks) <= kMaxGenerator &&
         (!r.carried || selfContained(out.blocks));
}

// The FOR EACH loop a use of variable `v` at position `k` of block `b` belongs to: the nearest each.enter or each.leave
// of `v` before it on every path (up the dominator tree), when that is an each.enter; null otherwise (after a loop's
// end, or outside any).
const Instruction *owningEnter(const CFG &cfg, const Var *v, const BasicBlock *b, size_t k) {
  for (const BasicBlock *at = b; at; at = cfg.idom(at)) {
    for (size_t j = at == b ? k : at->insts.size(); j-- > 0;) {
      const Instruction &i = *at->insts[j];
      if (i.var != v) continue;
      if (i.op == Op::EachEnter) return &i;
      if (i.op == Op::EachLeave) return nullptr;
    }
  }
  return nullptr;
}

// The loop of a FOR EACH over an enumerator made by `gen.new`, ready to fuse.
struct EachLoop {
  Instruction *make = nullptr;   // gen.new
  Instruction *source = nullptr; // each.source
  Instruction *enter = nullptr, *len = nullptr, *move = nullptr;
  BasicBlock *header = nullptr, *body = nullptr, *end = nullptr;
  Var *var = nullptr;
};

bool fusibleLoop(const Function &f, const Loops &loops, Instruction *make, EachLoop &l) {
  l.make = make;
  // Its one use the loop's each.source; a body inlined around it that it was passed to only holds it (its
  // inline.enter, of a body that does not read its arguments as received), and is given NIL instead.
  for (Instruction *u : make->users()) {
    if (u->op == Op::InlineEnter && !u->blockFn->readsArgs) continue;
    if (l.source) return false;
    l.source = u;
  }
  if (!l.source) return false;
  const Constant *descend = l.source->op == Op::EachSource ? asBool(l.source->operand(1)) : nullptr;
  if (!descend || descend->logical || l.source->operand(0) != make) return false;
  for (Instruction *u : l.source->users()) {
    Instruction **slot = u->op == Op::EachEnter ? &l.enter
                         : u->op == Op::EnumLen ? &l.len
                         : u->op == Op::EachMove ? &l.move
                                                 : nullptr;
    if (!slot || *slot) return false; // another use (each.key, a second loop), or a second one
    *slot = u;
  }
  if (!l.enter || !l.len || !l.move || l.enter->operand(1) != l.source || l.move->operand(2) != l.source) return false;
  l.var = l.enter->var;
  const Constant *inCell = asBool(l.enter->operand(0));
  if (!plainVar(l.var) || l.var->param || !inCell || inCell->logical || l.move->var != l.var) return false;
  CFG cfg(f); // no loop over the same variable inside this one
  for (const BasicBlock *b : cfg.order())
    for (size_t k = 0; k < b->insts.size(); ++k)
      if (b->insts[k]->var == l.var && b->insts[k]->op == Op::EachEnter && b->insts[k].get() != l.enter &&
          owningEnter(cfg, l.var, b, k) == l.enter)
        return false;
  // The header's test: `index <= enum.len`, to the body or the end; a loop entered as often as its enumerator is
  // made (every loop around its each.enter goes round its gen.new too: an enumerator made once and gone through
  // again goes on where it was).
  l.header = l.len->parent;
  for (const Loop *around = loops.innermost(l.enter->parent); around; around = around->parent)
    if (!around->contains(make->parent)) return false;
  Instruction *t = l.header->terminator();
  const Instruction *cmp = t->op == Op::CondBr ? asInstruction(t->operand(0), Op::IntCmp) : nullptr;
  if (!cmp || cmp->pred != IntPred::Le || cmp->operand(1) != l.len) return false;
  l.body = t->targets[0].block;
  l.end = t->targets[1].block;
  if (l.body == l.header || l.end == l.header) return false;
  // What the header computes goes no further than the test and the move, which go with it; its arguments to the body
  // and the end are its parameters or values from before the loop, and an edge into it passes it, for each parameter,
  // a value of its own or that parameter itself (the value going on).
  for (auto &i : l.header->insts)
    for (const Instruction *u : i->users())
      if (u->parent != l.header && u != l.move) return false;
  for (const Target &tg : t->targets)
    for (const Value *a : tg.args)
      if (a->kind() == Value::Kind::Instruction && static_cast<const Instruction *>(a)->parent == l.header)
        return false;
  for (auto &[b, terms] : predecessorTerminators(f))
    if (b == l.header)
      for (const Instruction *term : terms)
        for (const Target &tg : term->targets)
          for (size_t k = 0; tg.block == l.header && k < tg.args.size(); ++k)
            if (const BlockParam *p = asParam(tg.args[k]); p && p->block == l.header && p->index != k) return false;
  return true;
}

void fuse(Function &f, const EachLoop &l, Function &r, const Body &body) {
  Builder builder(&f);
  std::vector<Value *> args(l.make->operands().begin(), l.make->operands().end());

  // The generator's variables (but its enumerator, slot 0): new ones of the function (bodyVars).
  VarMap vars = bodyVars(f, r, std::any_of(args.begin(), args.end(), isReference), {}, 1);
  Var *state = body.yields.size() > 1 ? f.addFrameVar("") : nullptr;

  // The values the loop's header carries, as each edge into it gives them.
  struct Edge {
    BasicBlock *from;
    std::vector<Value *> args;
    bool back;
  };
  std::vector<Edge> edges;
  CFG before(f);
  Loops loops(before);
  std::unordered_set<const Instruction *> within; // the loop's reads and writes of its variable
  for (const BasicBlock *b : before.order())
    for (size_t k = 0; k < b->insts.size(); ++k)
      if (b->insts[k]->var == l.var && b->insts[k]->access == Access::ThroughRef &&
          owningEnter(before, l.var, b, k) == l.enter)
        within.insert(b->insts[k].get());
  const Loop *loop = loops.innermost(l.header);
  for (auto &[b, terms] : predecessorTerminators(f))
    if (b == l.header)
      for (Instruction *t : terms)
        for (const Target &tg : t->targets)
          if (tg.block == l.header)
            edges.push_back({t->parent, tg.args, loop && loop->header == l.header && loop->contains(t->parent)});
  Instruction *test = l.header->terminator();
  std::vector<Value *> bodyArgs = test->targets[0].args, endArgs = test->targets[1].args;
  std::vector<BlockParam *> headerParams;
  for (auto &p : l.header->params) headerParams.push_back(p.get());

  // The body, copied.
  BlockCopy copy = copyBlocks(f, body.blocks, ".fused", [&](Value *v) -> Value * {
    const Constant *c = asConstant(v);
    return c ? copyConstant(f, *c) : v;
  });
  std::vector<std::pair<BasicBlock *, BasicBlock *>> yields; // the block of each YIELD (copied), its target's copy
  std::vector<uint32_t> states;
  std::vector<BasicBlock *> ends;
  useBodyVars(copy, vars);
  // In a BEGIN SEQUENCE (or a FOR EACH cleanup), the body unwinds through a block that leaves its frame, and its ways
  // out to the loop are blocks of their own that unwind where the loop does (unwindBody).
  BasicBlock *handler = l.header->unwind, *pad = unwindBody(f, r, copy, handler);
  auto wayOut = [&](BasicBlock *b) {
    if (!handler) return b;
    BasicBlock *out = f.addBlock(b->name + ".out");
    out->unwind = handler;
    builder.br(out);
    builder.setBlock(out);
    return out;
  };
  for (BasicBlock *orig : body.blocks) {
    BasicBlock *b = copy.blocks.at(orig);
    Instruction *t = b->terminator();
    if (t->op == Op::GenYield) {
      Value *element = t->operand(0);
      uint32_t k = t->index;
      BasicBlock *next = t->targets[0].block;
      SourceLoc loc = t->loc;
      b->takeTerminator();
      builder.setBlock(b);
      builder.setLoc(loc);
      BasicBlock *out = wayOut(b);
      builder.store(l.var, Access::Direct, element);
      if (state) builder.store(state, Access::Direct, builder.integer(k, 10));
      builder.inlineLeave();
      builder.br(l.body);
      yields.push_back({out, next});
      states.push_back(k);
    } else if (t->op == Op::Ret) {
      SourceLoc loc = t->loc;
      b->takeTerminator();
      eraseInstruction(b->insts.back().get()); // gen.done
      builder.setBlock(b);
      builder.setLoc(loc);
      BasicBlock *out = wayOut(b);
      leaveBody(builder, r, vars); // what the generator's frame held, let go as its end drops it
      builder.inlineLeave();
      builder.br(l.end);
      ends.push_back(out);
    }
  }

  // The start: the frame shown, the parameters given the arguments (NIL past the last), the rest NIL; the way back
  // from the loop's body: the frame shown again, on after the YIELD that entered it.
  BasicBlock *start = f.addBlock("gen.fused"), *resume = f.addBlock("gen.resume");
  start->unwind = resume->unwind = pad; // their ends have the generator's frame open
  builder.setBlock(start);
  builder.setLoc(l.len->loc); // where the loop steps it: the caller's line the generator's frame shows
  builder.inlineEnter(&r);
  enterBody(builder, r, vars, args, 1);
  builder.br(copy.blocks.at(body.start));
  builder.setBlock(resume);
  builder.inlineEnter(&r);
  if (!state) {
    builder.br(yields.front().second);
  } else {
    Instruction *s = builder.load(state, Access::Direct);
    auto unboxed = std::make_unique<Instruction>(Op::UnboxI64, Type::I64, resume);
    unboxed->addOperand(s);
    Value *k64 = unboxed.get();
    resume->insts.push_back(std::move(unboxed));
    for (size_t k = 0; k + 1 < yields.size(); ++k) {
      BasicBlock *next = f.addBlock("gen.resume");
      next->unwind = pad;
      builder.condBr(builder.intCmp(IntPred::Eq, k64, builder.i64(states[k])), yields[k].second, next);
      builder.setBlock(next);
    }
    builder.br(yields.back().second);
  }

  // The loop enters the start, and comes back to the generator, instead of its header.
  for (size_t k = headerParams.size(); k-- > 0;) {
    std::vector<Instruction *> branches;
    for (const Edge &e : edges) branches.push_back(e.from->terminator());
    l.header->removeArgs(k, branches);
  }
  for (const Edge &e : edges)
    for (Target &tg : e.from->terminator()->targets)
      if (tg.block == l.header) tg.block = e.back ? resume : start;

  // The variable: saved at the loop's start, read and written in place within the loop, restored at its end.
  Instruction *saved = nullptr;
  {
    BasicBlock *b = l.enter->parent;
    size_t k = b->indexOf(l.enter);
    auto load = std::make_unique<Instruction>(Op::Load, Type::Item, b);
    load->loc = l.enter->loc;
    load->var = l.var;
    load->access = Access::Direct;
    saved = load.get();
    b->insts.insert(b->insts.begin() + (std::ptrdiff_t)k, std::move(load));
  }
  l.enter->replaceAllUsesWith(saved);
  for (auto &b : f.blocks)
    for (auto &ip : b->insts) {
      if (within.count(ip.get())) ip->access = Access::Direct;
      if (ip->op != Op::EachLeave || ip->operand(1) != saved) continue;
      auto store = std::make_unique<Instruction>(Op::Store, Type::Void, b.get());
      store->loc = ip->loc;
      store->var = l.var;
      store->access = Access::Direct;
      store->addOperand(ip->operand(1));
      ip = std::move(store); // the leave goes, its uses undone
    }
  eraseInstruction(l.move);
  eraseInstruction(l.enter);

  // The values the header carried, joined again through the generator's blocks: at its yields (the body's arguments),
  // its end (the end's), and wherever the loop used them.
  CFG now(f);
  Predecessors preds;
  for (auto &[b, terms] : predecessorTerminators(f))
    for (Instruction *t : terms)
      if (now.reachable(t->parent)) preds[b].push_back(t);
  std::vector<BlockParam *> placed;
  std::vector<SSAUpdater> carried;
  carried.reserve(headerParams.size());
  for (size_t k = 0; k < headerParams.size(); ++k) {
    carried.emplace_back(f, headerParams[k]->type(), preds, placed);
    for (const Edge &e : edges)
      if (e.args[k] != headerParams[k]) carried.back().addAvailable(e.from, e.args[k]); // else the value goes on
  }
  auto reaching = [&](Value *a, BasicBlock *at) -> Value * {
    for (size_t k = 0; k < headerParams.size(); ++k)
      if (a == headerParams[k]) return carried[k].atEnd(at);
    return a;
  };
  for (auto [b, next] : yields)
    for (Value *a : bodyArgs) b->terminator()->addTargetArg(0, reaching(a, b));
  for (BasicBlock *b : ends)
    for (Value *a : endArgs) b->terminator()->addTargetArg(0, reaching(a, b));
  for (size_t k = 0; k < headerParams.size(); ++k) {
    std::vector<Instruction *> users;
    for (Instruction *u : headerParams[k]->users())
      if (u->parent != l.header) users.push_back(u);
    std::sort(users.begin(), users.end());
    users.erase(std::unique(users.begin(), users.end()), users.end());
    for (Instruction *u : users) carried[k].rewriteUse(u, headerParams[k]);
  }
  removeTrivialParams(placed, preds);
  removeUnreachableBlocks(f); // the header, with the test and the length
  eraseInstruction(l.source);
  Builder nils(&f);
  for (Instruction *u : std::vector<Instruction *>(l.make->users())) u->replaceUses(l.make, nils.nil());
  eraseInstruction(l.make);
}

} // namespace

bool fuseGenerators(Function &f) {
  bool changed = false;
  for (bool again = true; again && instructionCount(f) <= kMaxFunction;) {
    again = false;
    CFG cfg(f);
    Loops loops(cfg);
    for (auto &b : f.blocks) {
      for (auto &i : b->insts) {
        if (i->op != Op::GenNew || !i->callee.function || i->callee.function == &f || !cfg.reachable(b.get())) continue;
        EachLoop l;
        Body body;
        if (!fusibleLoop(f, loops, i.get(), l) || !resumableBody(*i->callee.function, body)) continue;
        fuse(f, l, *i->callee.function, body);
        again = changed = true;
        break;
      }
      if (again) break;
    }
  }
  return changed;
}

} // namespace xb::xir
