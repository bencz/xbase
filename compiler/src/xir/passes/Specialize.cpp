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
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace xb::xir {

namespace {

// The classes of argument types a copy is made for: those without heap references whose operations the code makes
// inline, the integers alone first (the copy then computes with integers only, where their ranges allow). A parameter
// of any other type stays of any type.
constexpr TypeSet kClasses[] = {kIntType, kNumberTypes, typeBit(XB_T_LOGICAL), typeBit(XB_T_DATE)};

// At most this many copies of a function, each of at most this many instructions.
constexpr size_t kMaxCopies = 4;
constexpr size_t kMaxInstructions = 2000;

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

// The copy of `g` whose parameters hold `types` and objects of `classes`: shown on the call stack as `g`, reached only
// from the module.
Function *makeCopy(Module &m, const Function &g, const std::vector<TypeSet> &types,
                   const std::vector<int32_t> &classes, size_t number) {
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
  }
  dropReferenceTest(c); // its calls pass no reference
  return &c;
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
  std::vector<std::pair<Instruction *, std::vector<uint32_t>>> work;
  for (BasicBlock *b : cfg.order())
    for (auto &ip : b->insts) {
      Instruction &i = *ip;
      Function *g = i.callee.function;
      if (i.op != Op::Call || !g || i.hasRest() || !g->paramTypes.empty() || !copyable(*g)) continue;
      if (std::any_of(i.operands().begin(), i.operands().end(), isReference)) continue;
      std::vector<uint32_t> wanted(2 * g->numParams, kAnyType);
      for (uint32_t k = 0; k < g->numParams; ++k) wanted[g->numParams + k] = (uint32_t)kNoClass;
      bool any = false;
      for (uint32_t k = 0; k < g->numParams && k < i.operands().size(); ++k) {
        wanted[k] = classOf(types.of(i.operand(k)));
        int32_t c = classes ? classes->of(i.operand(k)) : kNoClass;
        wanted[g->numParams + k] = (uint32_t)c;
        any |= wanted[k] != kAnyType || c != kNoClass;
      }
      if (any) work.push_back({&i, wanted});
    }
  for (auto &[call, wanted] : work) {
    Function *g = call->callee.function;
    auto key = std::make_pair(static_cast<const Function *>(g), wanted);
    auto it = copies.find(key);
    Function *copy = nullptr;
    if (it != copies.end()) {
      copy = it->second;
    } else {
      auto made = static_cast<size_t>(
          std::count_if(copies.begin(), copies.end(), [&](const auto &e) { return e.first.first == g; }));
      if (made >= kMaxCopies) continue;
      std::vector<TypeSet> typesOf(wanted.begin(), wanted.begin() + g->numParams);
      std::vector<int32_t> classesOf;
      for (size_t k = g->numParams; k < wanted.size(); ++k) classesOf.push_back((int32_t)wanted[k]);
      copy = makeCopy(m, *g, typesOf, classesOf, made + 1);
      copies[key] = copy;
      optimizeCopy(*copy);
    }
    call->callee.function = copy;
    changed = true;
  }
  return changed;
}

bool dropCopiedFunctions(Module &m, const TypedCopies &copies) {
  // The candidates: a STATIC FUNCTION copied, and its body versioned for references, which only it calls.
  std::unordered_set<const Function *> candidates;
  for (auto &[key, copy] : copies) {
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
