// value-captures: the variables of a function that its codeblocks may capture by value (FuncDecl::capturedByValue). A
// codeblock shares a variable it captures with the function: the variable moves into a cell, which both reach, so that
// each sees the other's assignments. A variable that nothing assigns after its declaration (its initializer runs before
// any block can capture it: a block captures only variables declared before it), nor passes by reference, nor loops
// with (nor writes through PValue()), holds one value as long as a block can see it: a block may receive a cell of its
// own holding that value, and the function keeps the variable in its frame, read directly. Only a function's own
// parameters and LOCALs (not a codeblock's, which Eval() may pass references; not the hidden ones of WITH OBJECT, USING
// and AWAIT FOR EACH, which their statements assign).
#include "opt/AstPasses.h"
#include "opt/VariableFacts.h"

namespace xb {

namespace {

// The slots of the function's parameters and of its LOCAL statements' variables (which come first, at the function's
// level), not the hidden ones; not the parameters of a function that may write them through PValue() (readsParams: a
// call of a PARAMS function, a macro).
std::vector<bool> declared(const FuncDecl &f) {
  std::vector<bool> out(f.numLocals);
  for (size_t k = 0; k < f.params.size() && k < out.size(); ++k) out[k] = !f.readsParams;
  for (auto &s : f.body)
    if (s->kind == StmtKind::Local)
      for (const LocalDecl &d : static_cast<const LocalStmt &>(*s).decls)
        if (d.slot < out.size()) out[d.slot] = true;
  return out;
}

} // namespace

bool markValueCaptures(Module &m) {
  bool changed = false;
  for (auto &f : m.funcs) {
    VariableFacts facts = variableFacts(*f);
    std::vector<bool> own = declared(*f), byValue(f->numLocals);
    bool any = false;
    for (uint32_t s = 0; s < f->numLocals; ++s) {
      byValue[s] = VariableFacts::has(facts.captured, s) && !VariableFacts::has(facts.written, s) && own[s];
      any |= byValue[s];
    }
    if (!any) byValue.clear();
    if (byValue != f->capturedByValue) {
      f->capturedByValue = std::move(byValue);
      changed = true;
    }
  }
  return changed;
}

} // namespace xb
