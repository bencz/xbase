// Call resolution against the module's functions and EXTERN FUNCTION declarations.
#include "sema/Calls.h"

#include "common/Text.h"
#include "parser/Walk.h"

#include <utility>

namespace xb {

void CallResolver::run(Module &m) {
  for (const ExternDecl &d : m.externs) {
    auto [it, fresh] = externs_.emplace(d.name, &d);
    if (!fresh) {
      const ExternDecl &p = *it->second;
      if (p.minArgs != d.minArgs || p.maxArgs != d.maxArgs || p.native != d.native || p.reserved != d.reserved ||
          p.checked != d.checked || p.fold != d.fold || p.args != d.args || p.params != d.params ||
          p.abbrev != d.abbrev || p.resultTypes != d.resultTypes || p.byRef != d.byRef || p.restByRef != d.restByRef ||
          p.library != d.library) {
        diag_.error(d.loc, "EXTERN FUNCTION '" + d.name + "' conflicts with an earlier declaration");
        diag_.note(p.loc, "previous declaration is here");
      }
      continue;
    }
    if (d.abbrev) abbreviable_.push_back(&d);
    if (d.fold && !foldable_(d.name))
      diag_.error(d.loc, "FOLD: the compiler cannot evaluate '" + d.name + "'");
  }
  for (auto &f : m.funcs) {
    const ExternDecl *d = findExtern(f->name, f->loc);
    if (d && d->reserved && !runtimeLibrary_)
      diag_.error(f->loc, "redefinition of predefined function " + d->name + " as '" + f->name + "'");
    if (f->startup == FuncDecl::Startup::None) funcs_.insert(f->name); // INIT and EXIT names are their own
  }
  // EXTERNAL names: the module's own functions and the runtime's (all of them are linked in) need nothing; a name
  // given twice is kept once.
  std::vector<Request> requests;
  std::unordered_set<std::string> requested;
  for (Request &r : m.requests) {
    if (funcs_.count(r.name)) continue;
    const ExternDecl *d = findExtern(r.name, r.loc);
    if (d && d->native) continue;
    if (d) {
      r.name = d->name;
      r.library = d->library;
    }
    if (requested.insert(r.name).second) requests.push_back(std::move(r));
  }
  m.requests = std::move(requests);
  for (LocalDecl &d : m.fileStatics)
    if (d.init) expr(*d.init);
  for (auto &f : m.funcs) block(f->body);
}

// The declaration a name designates: the exact name, or an abbreviation of a
// declaration that allows one (at least `abbrev` letters), as keywords are.
// An abbreviation that fits several declarations is reported at `loc`.
const ExternDecl *CallResolver::findExtern(const std::string &name, SourceLoc loc) const {
  auto it = externs_.find(name);
  if (it != externs_.end()) return it->second;
  const ExternDecl *found = nullptr;
  for (const ExternDecl *d : abbreviable_) {
    if (!keywordMatches(name, d->name, d->abbrev)) continue;
    if (found) {
      diag_.error(loc, "'" + name + "' abbreviates both " + found->name + " and " + d->name);
      return found;
    }
    found = d;
  }
  return found;
}

void CallResolver::call(CallExpr &c) {
  const ExternDecl *d = findExtern(c.name, c.loc);
  if (!c.declaredOnly && funcs_.count(c.name)) { // the module's own functions come first
    if (d && d->name == c.name && !c.reference) checkArgs(c, *d); // a CHECKED name keeps its count when redefined
    return;
  }
  if (!d) { // as in Clipper, the linker finds it in another module
    c.external = true;
    return;
  }
  c.name = d->name;
  c.callee = d;
  if (c.reference) return; // @name(): no call, no arguments to check
  checkArgs(c, *d);
  // A native function that does not take a reference at that position
  // receives the value (`...` comes last: every other argument's position is
  // known).
  for (size_t k = 0; k < c.args.size(); ++k) {
    if (c.args[k]->kind != ExprKind::Ref || d->takesRef(k)) continue;
    if (static_cast<RefExpr &>(*c.args[k]).target->kind == ExprKind::Call) continue; // @name(): a value
    ExprPtr target = std::move(static_cast<RefExpr &>(*c.args[k]).target);
    c.args[k] = std::move(target);
  }
}

// With `...` as the last argument, only the others are known: too many of them
// is an error, too few may not be.
void CallResolver::checkArgs(const CallExpr &c, const ExternDecl &d) {
  bool spread = !c.args.empty() && c.args.back()->kind == ExprKind::Spread;
  size_t n = c.args.size() - (spread ? 1 : 0);
  if (d.checked && ((!spread && n < d.minArgs) || (d.maxArgs != ExternDecl::kUnlimited && n > d.maxArgs))) {
    std::string expected = d.minArgs == d.maxArgs ? std::to_string(d.minArgs)
                           : d.maxArgs == ExternDecl::kUnlimited
                               ? "at least " + std::to_string(d.minArgs)
                               : "from " + std::to_string(d.minArgs) + " to " + std::to_string(d.maxArgs);
    diag_.error(c.loc, "incorrect number of arguments in " + d.name + ": passed " + std::to_string(n) +
                           ", expected " + expected);
  }
}

void CallResolver::expr(Expr &e) {
  forEachChildExpr(e, [&](ExprPtr &c) { expr(*c); });
  forEachChildBlock(e, [&](Block &x) { block(x); });
  if (e.kind == ExprKind::Call) call(static_cast<CallExpr &>(e));
}

void CallResolver::block(Block &b) {
  for (auto &st : b) forEachPart(*st, [&](ExprPtr &e) { expr(*e); }, [&](Block &x) { block(x); });
}

} // namespace xb
