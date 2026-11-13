// A STATIC FUNCTION is reached only from the module (no macro, no other module, no message reaches it): by its calls,
// and through its function references (`func.ref`), which may pass anything. When it has no reference and none of its
// calls passes a reference, nor `...` (whose arguments may be references), its parameters never hold one. Its
// versioning test (`params.ref`) is then false, and its body versioned for references (`NAME.byref`), which only that
// test leads to, is never run: it leaves the module, with the codeblocks it makes.
#include "xir/passes/Passes.h"

#include "xir/Builder.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace xb::xir {

bool dropReferenceBodies(Module &m) {
  // What may reach each function with a reference: a call passing one or `...`, forwarding its arguments, or any
  // other use of the function (`func.ref`: Eval(), Do() and :Exec() pass what they are given).
  std::unordered_map<const Function *, bool> calls; // callee: some call may pass a reference
  for (auto &g : m.functions)
    for (auto &b : g->blocks)
      for (auto &i : b->insts)
        if (const Function *callee = i->callee.function) {
          bool ref = i->op != Op::Call || i->hasRest() ||
                     std::any_of(i->operands().begin(), i->operands().end(), isReference);
          calls[callee] |= ref;
        }
  std::unordered_set<Function *> dropped;
  for (auto &fp : m.functions) {
    Function &f = *fp;
    if (f.kind != Function::Kind::Program || !f.internal || f.method || f.startup || f.byRefBody) continue;
    Instruction *test = versioning(f);
    if (!test || calls[&f]) continue;
    Function *byRef = referenceBody(*test);
    test->operand(0)->replaceAllUsesWith(Builder(&f).boolean(false));
    foldConstants(f);
    simplifyCFG(f);
    removeDeadCode(f);
    dropped.insert(byRef); // only the test of `f` called it
  }
  if (dropped.empty()) return false;
  // The codeblocks those bodies make (each made by one `block` instruction, of its creator) go with them.
  std::vector<Function *> work(dropped.begin(), dropped.end());
  while (!work.empty()) {
    Function *g = work.back();
    work.pop_back();
    for (auto &b : g->blocks)
      for (auto &i : b->insts)
        if (i->op == Op::MakeBlock && i->blockFn && dropped.insert(i->blockFn).second) work.push_back(i->blockFn);
  }
  std::erase_if(m.functions, [&](const std::unique_ptr<Function> &f) { return dropped.count(f.get()) != 0; });
  return true;
}

} // namespace xb::xir
