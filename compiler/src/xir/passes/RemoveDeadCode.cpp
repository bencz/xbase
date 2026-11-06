// Dead code elimination: results nobody uses, of operations that do nothing
// else, and block parameters nobody uses.
#include "xir/passes/Passes.h"

#include "xir/Analysis.h"

#include <unordered_set>
#include <vector>

namespace xb::xir {

namespace {

// Removing it changes nothing but its result (see Effect): it cannot fail and
// writes nothing. Reading, depending on settings and allocating are harmless.
bool removable(const Instruction &i) {
  return !i.mayFail() && !(opInfo(i.op).effects & (WritesVar | WritesHeap | WritesState | RunsCode | Terminator));
}

} // namespace

bool removeDeadCode(Function &f) {
  std::vector<Instruction *> work;
  std::unordered_set<const Instruction *> dead;
  auto consider = [&](Value *v) {
    if (v->kind() != Value::Kind::Instruction || !v->users().empty()) return;
    auto *i = static_cast<Instruction *>(v);
    if (removable(*i) && dead.insert(i).second) work.push_back(i);
  };
  for (auto &bb : f.blocks)
    for (auto &i : bb->insts) consider(i.get());
  Predecessors preds = predecessorTerminators(f);
  bool params = false, removedParams = false;
  do { // dead instructions free their operands; dead parameters free their arguments
    while (!work.empty()) {
      Instruction *i = work.back();
      work.pop_back();
      std::vector<Value *> ops = i->operands();
      i->dropOperands();
      for (Value *v : ops) consider(v);
    }
    params = false;
    for (auto &bb : f.blocks)
      for (size_t k = bb->params.size(); k-- > 0;) {
        if (!bb->params[k]->users().empty()) continue;
        std::vector<Value *> args;
        for (Instruction *t : preds[bb.get()])
          for (const Target &tg : t->targets)
            if (tg.block == bb.get()) args.push_back(tg.args[k]);
        bb->removeParam(k, preds[bb.get()]);
        for (Value *v : args) consider(v);
        params = removedParams = true;
      }
  } while (!work.empty() || params);
  bool changed = !dead.empty();
  for (auto &bb : f.blocks)
    std::erase_if(bb->insts, [&](const std::unique_ptr<Instruction> &i) { return dead.count(i.get()) != 0; });
  return changed || removedParams;
}

} // namespace xb::xir
