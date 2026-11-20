// A call of a function of the module whose arguments hold numbers (or logicals, or dates), or objects of one class of
// the module, goes to a copy of the function that knows it: its parameters read with those types, so that what it
// computes from them stays in registers, or of that class, so that its sends to them resolve (a method's to Self), and
// its own calls with such arguments go to the copy too (a recursive function then knows its result).
#include "xir/passes/Passes.h"

#include "xir/Builder.h"
#include "xir/ClassModel.h"
#include "xir/Transform.h"
#include "xir/Types.h"

#include <algorithm>
#include <bit>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace xb::xir {

namespace {

// The classes of argument types a copy is made for: those without heap references whose operations the code makes
// inline, the integers alone first (the copy then computes with integers only, where their ranges allow). A parameter
// of any other type stays of any type.
constexpr TypeSet kClasses[] = {kIntType, kNumberTypes, typeBit(XB_T_LOGICAL), typeBit(XB_T_DATE)};

// At most this many copies of a function of each kind (those that know every parameter, its types or its object's
// class, and those that leave some parameter of any type), each of at most this many instructions.
constexpr size_t kMaxCopies = 4;
constexpr size_t kMaxInstructions = 2000;

// Rounds of refineCopies at most: each one grows some copy's return types or moves some call, which the number of
// copies and of types bound; past them the copies stay as the pipeline left them.
constexpr unsigned kMaxRefineRounds = 32;
// Passes of refineCopies over one function in a round at most: each moves some call to a copy for more types.
constexpr unsigned kMaxRefinePasses = 32;

// What a copy is made for: each parameter's class of types (kClasses, or any), then the class of the object it holds
// (kNoClass for any value).
using Wanted = std::vector<uint32_t>;

using Origins = std::unordered_map<const Instruction *, const Instruction *>;

TypeSet classOf(TypeSet t) {
  for (TypeSet c : kClasses)
    if (t && (t & ~c) == 0) return c;
  return kAnyType;
}

// A function whose copy behaves as it does: a FUNCTION, PROCEDURE or method of the program, not one that reads
// its arguments as received (PCount(), PValue(), `...`), nor a generator or an ASYNC function (made by a resume
// function), nor one that makes codeblocks (which would share their function with the copy) or recovers from errors.
bool copyable(const Function &g) {
  if (g.kind != Function::Kind::Program || g.startup || g.byRefBody || g.readsArgs || g.readsParams ||
      g.usesRest || g.ownsPrivates || g.numParams == 0 || instructionCount(g) > kMaxInstructions)
    return false;
  for (auto &b : g.blocks) {
    if (b->unwind) return false;
    for (auto &i : b->insts)
      if (i->op == Op::MakeBlock || i->op == Op::GenNew || i->op == Op::AgenNew || i->op == Op::AsyncNew ||
          isSuspension(i->op) || i->op == Op::ArgsRest)
        return false;
  }
  return true;
}

// A call that may go to a copy of `g` (its callee, or the function its callee is a copy of): no `...`, no reference.
bool mayCallCopy(const Instruction &i, const Function &g) {
  return i.op == Op::Call && !i.hasRest() && copyable(g) &&
         std::none_of(i.operands().begin(), i.operands().end(), isReference);
}

// What a call of `g` wants a copy for; empty when it knows nothing of any parameter. An argument with no type yet (a
// call of a copy whose return types refineCopies is settling, or of one that never returns) takes the first class: no
// value of it reaches the call.
Wanted wantedFor(const Instruction &call, const Function &g, const TypeInfo &types, const ClassInfo *classes) {
  Wanted wanted(2 * g.numParams, kAnyType);
  for (uint32_t k = 0; k < g.numParams; ++k) wanted[g.numParams + k] = (uint32_t)kNoClass;
  bool any = false;
  for (uint32_t k = 0; k < g.numParams && k < call.operands().size(); ++k) {
    TypeSet t = types.of(call.operand(k));
    wanted[k] = t == 0 ? kClasses[0] : classOf(t);
    int32_t c = classes ? classes->of(call.operand(k)) : kNoClass;
    wanted[g.numParams + k] = (uint32_t)c;
    any |= wanted[k] != kAnyType || c != kNoClass;
  }
  return any ? wanted : Wanted{};
}

// Whether a copy for `wanted` knows every parameter: its types or its object's class.
bool knowsAll(const Wanted &wanted) {
  size_t n = wanted.size() / 2;
  for (size_t k = 0; k < n; ++k)
    if (wanted[k] == kAnyType && (int32_t)wanted[n + k] == kNoClass) return false;
  return true;
}

// Whether copy `c` takes every argument of a call that wants `wanted`.
bool covers(const Function &c, const Wanted &wanted) {
  size_t n = wanted.size() / 2;
  for (size_t k = 0; k < n; ++k)
    if ((wanted[k] & ~c.paramTypes[k]) != 0 ||
        (c.paramClasses[k] != kNoClass && c.paramClasses[k] != (int32_t)wanted[n + k]))
      return false;
  return true;
}

// Where a call of `g` that wants `wanted` goes: the copy made for it; else a new one (`make`) while `g` has fewer than
// kMaxCopies of its kind; else the copy made already that takes its arguments with the fewest parameter types (the
// first made of those); else (null) `g` itself.
struct Choice {
  Function *copy = nullptr;
  bool make = false;
};

Choice chooseCopy(const Module &m, const Function &g, const Wanted &wanted, const TypedCopies &copies) {
  if (auto it = copies.find({&g, wanted}); it != copies.end()) return {it->second, false};
  bool all = knowsAll(wanted);
  size_t kind = 0;
  for (auto &[key, c] : copies) kind += key.first == &g && knowsAll(key.second) == all;
  if (kind < kMaxCopies) return {nullptr, true};
  std::unordered_set<const Function *> ofG;
  for (auto &[key, c] : copies)
    if (key.first == &g && covers(*c, wanted)) ofG.insert(c);
  Function *best = nullptr;
  size_t bestTypes = 0;
  for (auto &f : m.functions) { // in the order they were made
    if (!ofG.count(f.get())) continue;
    size_t types = 0;
    for (uint32_t t : f->paramTypes) types += (size_t)std::popcount(t);
    if (best && types >= bestTypes) continue;
    best = f.get();
    bestTypes = types;
  }
  return {best, false};
}

// The copy of `g` whose parameters hold `types` and objects of `classes`: shown on the call stack as `g`, reached only
// from the module. `origins`, when given, gets each call of the copy with the call of `g` it copies.
Function *makeCopy(Module &m, const Function &g, const std::vector<TypeSet> &types,
                   const std::vector<int32_t> &classes, size_t number, Origins *origins) {
  std::string name = g.name + ".typed" + std::to_string(number);
  m.functions.push_back(std::make_unique<Function>(Function::Kind::Program, name));
  Function &c = *m.functions.back();
  c.stackName = g.stackName;
  c.internal = true;
  c.numParams = g.numParams;
  c.runtime = g.runtime;
  c.callsPrivates = g.callsPrivates;
  c.hidden = g.hidden;
  c.method = g.method;
  // A parameter some instruction writes (or passes on by reference) keeps any type and class.
  c.paramTypes = types;
  c.paramClasses = classes;
  for (auto &b : g.blocks)
    for (auto &i : b->insts)
      if (isParameter(i->var) && i->var->index < types.size() && i->op != Op::Load) {
        c.paramTypes[i->var->index] = kAnyType;
        c.paramClasses[i->var->index] = kNoClass;
      }
  std::map<const Var *, Var *> vars;
  for (auto &v : g.vars) {
    Var *nv = c.addVar(v->kind, v->index, v->name);
    nv->shared = v->shared;
    nv->param = v->param;
    vars[v.get()] = nv;
  }
  std::vector<BasicBlock *> blocks;
  for (auto &b : g.blocks) blocks.push_back(b.get());
  BlockCopy copy = copyBlocks(c, blocks, "", [&](Value *v) -> Value * {
    const Constant *k = asConstant(v);
    return k ? copyConstant(c, *k) : v;
  });
  for (auto [orig, made] : copy.pairs) {
    if (made->kind() != Value::Kind::Instruction) continue;
    auto *i = static_cast<Instruction *>(made);
    auto varOf = [&](Var *v) { // the module's STATICs stay
      auto it = vars.find(v);
      return it == vars.end() ? v : it->second;
    };
    if (i->var) i->var = varOf(i->var);
    for (Var *&v : i->captures) v = varOf(v);
    if (i->op == Op::FrameRelease && i->blockFn == &g) i->blockFn = &c; // a parameter of the copy's own
    if (origins && i->op == Op::Call) (*origins)[i] = static_cast<const Instruction *>(orig);
  }
  dropReferenceTest(c); // its calls pass no reference
  return &c;
}

// Makes the copy of `g` for `wanted`, recorded in `copies`.
Function *newCopy(Module &m, const Function &g, const Wanted &wanted, TypedCopies &copies, Origins *origins) {
  auto made = static_cast<size_t>(
      std::count_if(copies.begin(), copies.end(), [&](const auto &e) { return e.first.first == &g; }));
  std::vector<TypeSet> types(wanted.begin(), wanted.begin() + g.numParams);
  std::vector<int32_t> classes;
  for (size_t k = g.numParams; k < wanted.size(); ++k) classes.push_back((int32_t)wanted[k]);
  Function *copy = makeCopy(m, g, types, classes, made + 1, origins);
  copies[{&g, wanted}] = copy;
  return copy;
}

} // namespace

bool specializeCalls(Module &m, Function &f, TypedCopies &copies, const std::function<void(Function &)> &optimizeCopy) {
  bool changed = false;
  CFG cfg(f);
  TypeInfo types(cfg);
  std::optional<ClassModel> model;
  std::optional<ClassInfo> classes;
  if (!m.classes.empty()) {
    model.emplace(m);
    classes.emplace(f, cfg, *model);
  }
  // Each call to copy for, with what its copy's key holds: the parameters' types, then their classes.
  std::vector<std::pair<Instruction *, Wanted>> work;
  for (BasicBlock *b : cfg.order())
    for (auto &ip : b->insts) {
      Instruction &i = *ip;
      Function *g = i.callee.function;
      if (!g || !g->paramTypes.empty() || !mayCallCopy(i, *g)) continue;
      Wanted wanted = wantedFor(i, *g, types, classes ? &*classes : nullptr);
      if (!wanted.empty()) work.push_back({&i, wanted});
    }
  for (auto &[call, wanted] : work) {
    Function *g = call->callee.function;
    Choice choice = chooseCopy(m, *g, wanted, copies);
    Function *copy = choice.copy;
    if (choice.make) {
      copy = newCopy(m, *g, wanted, copies, nullptr);
      optimizeCopy(*copy);
    }
    if (!copy) continue;
    call->callee.function = copy;
    changed = true;
  }
  return changed;
}

bool refineCopies(Module &m, TypedCopies &copies) {
  if (copies.empty()) return false;
  std::unordered_map<const Function *, Function *> baseOf; // a copy's function
  for (auto &[key, c] : copies) baseOf[c] = const_cast<Function *>(key.first);
  auto base = [&](Function *g) {
    auto it = baseOf.find(g);
    return it == baseOf.end() ? g : it->second;
  };
  // What the code was optimized for: each call's callee, and each function's return types.
  std::unordered_map<const Instruction *, Function *> before;
  std::unordered_map<const Function *, TypeSet> assumed;
  for (auto &f : m.functions) {
    assumed[f.get()] = f->returnTypes;
    for (auto &b : f->blocks)
      for (auto &i : b->insts)
        if (i->callee.function && mayCallCopy(*i, *base(i->callee.function))) before[i.get()] = i->callee.function;
  }
  size_t functions = m.functions.size();
  Origins origins; // the calls of the copies made here, with those they copy
  // What a call went to before: right still where nothing better fits (the copy it went to takes its arguments, as the
  // types of its function's code found them then; the code may have lost them since, a parameter of a body run in place
  // in a function that keeps its variables in memory).
  auto previous = [&](const Instruction &i, Function *g) {
    auto o = origins.find(&i);
    auto was = before.find(o == origins.end() ? &i : o->second);
    return was == before.end() ? g : was->second;
  };
  std::vector<TypedCopies::key_type> madeHere;
  // A copy made here is tidied (dropReferenceTest folds) with the return types the code was optimized for, not with
  // those being settled.
  auto make = [&](Function &g, const Wanted &wanted) {
    std::vector<std::pair<Function *, uint32_t>> settling;
    for (auto &[key, c] : copies) {
      settling.push_back({c, c->returnTypes});
      auto a = assumed.find(c);
      c->returnTypes = a == assumed.end() ? kAnyType : a->second;
    }
    Function *copy = newCopy(m, g, wanted, copies, &origins);
    for (auto [c, t] : settling) c->returnTypes = t;
    copy->returnTypes = 0;
    baseOf[copy] = &g;
    madeHere.push_back({&g, wanted});
    return copy;
  };
  for (auto &[key, c] : copies) c->returnTypes = 0;
  bool settled = false;
  for (unsigned round = 0; round < kMaxRefineRounds && !settled; ++round) {
    bool changed = false;
    std::optional<ClassModel> model;
    if (!m.classes.empty()) model.emplace(m);
    for (size_t fi = 0; fi < m.functions.size(); ++fi) { // the copies made meanwhile too
      Function &f = *m.functions[fi];
      bool calls = false; // a call that may go to a copy, else nothing to do
      for (auto &b : f.blocks)
        for (auto &i : b->insts) calls = calls || (i->callee.function && mayCallCopy(*i, *base(i->callee.function)));
      // Again while a call moves: the types of what it returns now reach the calls its result goes to.
      for (unsigned pass = 0; calls && pass < kMaxRefinePasses; ++pass) {
        CFG cfg(f);
        TypeInfo types(cfg);
        std::optional<ClassInfo> classes;
        if (model) classes.emplace(f, cfg, *model);
        bool moved = false;
        for (BasicBlock *b : cfg.order())
          for (auto &ip : b->insts) {
            Instruction &i = *ip;
            if (!i.callee.function || !mayCallCopy(i, *base(i.callee.function))) continue;
            Function *g = base(i.callee.function), *target = previous(i, g);
            if (Wanted wanted = wantedFor(i, *g, types, classes ? &*classes : nullptr); !wanted.empty()) {
              Choice choice = chooseCopy(m, *g, wanted, copies);
              if (choice.make) target = make(*g, wanted);
              else if (choice.copy) target = choice.copy;
            }
            if (target == i.callee.function) continue;
            i.callee.function = target;
            moved = true;
          }
        if (!moved) break;
        changed = true;
      }
    }
    for (auto &c : m.functions) { // in the module's order: the rounds go the same way every time
      if (!baseOf.count(c.get())) continue;
      TypeSet r = c->returnTypes | (returnTypes(*c) & kAnyType);
      if (r == c->returnTypes) continue;
      c->returnTypes = r;
      changed = true;
    }
    settled = !changed;
  }
  // The code around each call was optimized for what its callee returned then (in a copy made here, the code around
  // the call it copies): what the call gives now must be among those types.
  bool sound = settled;
  for (auto &f : m.functions)
    for (auto &b : f->blocks)
      for (auto &i : b->insts) {
        auto o = origins.find(i.get());
        auto was = before.find(o == origins.end() ? i.get() : o->second);
        if (was == before.end()) continue;
        TypeSet now = i->callee.function->returnTypes & kAnyType;
        sound = sound && (now & ~(assumed.at(was->second) & kAnyType)) == 0;
      }
  if (sound) {
    bool moved = std::any_of(before.begin(), before.end(), [](const auto &e) {
      return e.first->callee.function != e.second;
    });
    return moved || m.functions.size() != functions;
  }
  // Else everything goes back as it was.
  for (auto &[call, callee] : before) const_cast<Instruction *>(call)->callee.function = callee;
  for (auto &[f, t] : assumed) const_cast<Function *>(f)->returnTypes = t;
  for (const auto &key : madeHere) copies.erase(key);
  m.functions.resize(functions);
  return false;
}

bool dropCopiedFunctions(Module &m, const TypedCopies &copies) {
  // The candidates: a STATIC FUNCTION copied, and its body versioned for references, which only it calls; a copy.
  std::unordered_set<const Function *> candidates;
  for (auto &[key, copy] : copies) {
    candidates.insert(copy);
    const Function *g = key.first;
    if (!g->internal || g->startup || g->method) continue;
    candidates.insert(g);
    if (const Instruction *test = versioning(*g))
      if (const Function *byRef = referenceBody(*test)) candidates.insert(byRef);
  }
  // What the other functions reach, through calls, references and codeblocks, from the roots: every function not a
  // candidate, the module's initialization and procedures, the classes' methods.
  std::unordered_set<const Function *> reached{m.init, firstProgramFunction(m)};
  std::vector<const Function *> work;
  auto reach = [&](const Function *g) {
    if (g && reached.insert(g).second) work.push_back(g);
  };
  for (auto &g : m.functions)
    if (!candidates.count(g.get())) reach(g.get());
  for (const Function *g : m.initProcs) reach(g);
  for (const Function *g : m.exitProcs) reach(g);
  for (auto &c : m.classes) {
    reach(c.init);
    reach(c.destructor);
    for (auto &method : c.methods) reach(method.fn);
  }
  for (const Function *root : {static_cast<const Function *>(m.init), firstProgramFunction(m)})
    if (root) work.push_back(root);
  while (!work.empty()) {
    const Function *g = work.back();
    work.pop_back();
    for (auto &b : g->blocks)
      for (auto &i : b->insts) {
        reach(i->callee.function);
        reach(i->blockFn);
      }
  }
  return std::erase_if(m.functions, [&](const std::unique_ptr<Function> &g) {
           return candidates.count(g.get()) && !reached.count(g.get());
         }) > 0;
}

// What the copy's own calls of itself give: the least fixed point, from no type, of the types its RETURNs give with
// those calls giving the types found so far.
void settleReturnTypes(Function &f) {
  TypeSet r = 0;
  for (unsigned round = 0; round < 64; ++round) {
    f.returnTypes = r;
    TypeSet next = returnTypes(f);
    if (next == r) return;
    r |= next;
  }
  f.returnTypes = kAnyType;
}

} // namespace xb::xir
