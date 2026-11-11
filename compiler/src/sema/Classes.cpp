// Classes to functions (see Classes.h).
#include "sema/Classes.h"

#include "xbrt.h" // XB_TYPE_NAME_*

#include <algorithm>
#include <iterator>
#include <set>
#include <unordered_map>

namespace xb {

namespace {

// The function name of a member: "CLS:MESSAGE", what the call stack shows.
std::string memberName(const ClassDecl &c, const ClassMethodDecl &m) { return c.name + ":" + m.message(); }

// The parameters of a method: SELF, then those written.
void withSelf(FuncDecl &f, SourceLoc loc) { f.params.insert(f.params.begin(), Param{loc, "SELF"}); }

// Whether an implementation of `kind` implements a declaration of `declared`
// (METHOD may implement an ACCESS, as in the other dialects).
bool implements(FuncDecl::Member kind, FuncDecl::Member declared) {
  return kind == declared || (kind == FuncDecl::Member::Method && declared == FuncDecl::Member::Access);
}

// Whether class `c` reaches `target` through the module's own classes it inherits from.
bool inherits(const ClassDecl &c, const ClassDecl &target, const std::unordered_map<std::string, ClassDecl *> &byName,
              std::set<const ClassDecl *> &seen) {
  for (const std::string &p : c.parents) {
    auto it = byName.find(p);
    if (it == byName.end() || !seen.insert(it->second).second) continue;
    if (it->second == &target || inherits(*it->second, target, byName, seen)) return true;
  }
  return false;
}

} // namespace

void ClassResolver::run(Module &m) {
  if (m.classes.empty() && std::none_of(m.funcs.begin(), m.funcs.end(), [](const auto &f) {
        return f->member != FuncDecl::Member::None;
      }))
    return;
  std::unordered_map<std::string, ClassDecl *> byName;
  std::set<std::string> functions;
  for (auto &f : m.funcs)
    if (f->member == FuncDecl::Member::None && f->startup == FuncDecl::Startup::None) functions.insert(f->name);
  for (auto &c : m.classes) {
    if (!byName.emplace(c->name, c.get()).second) diag_.error(c->loc, "class " + c->name + " is already defined");
    if (c->scalar) {
      static const char *const types[] = {XB_TYPE_NAME_NIL,   XB_TYPE_NAME_LOGICAL, XB_TYPE_NAME_NUMERIC,
                                          XB_TYPE_NAME_CHARACTER, XB_TYPE_NAME_ARRAY, XB_TYPE_NAME_BLOCK,
                                          XB_TYPE_NAME_DATE,  XB_TYPE_NAME_HASH};
      if (std::none_of(std::begin(types), std::end(types), [&](const char *t) { return c->name == t; }))
        diag_.error(c->loc, "SCALAR CLASS " + c->name + ": not the name of a type as ClassName() gives it");
      if (!c->vars.empty()) diag_.error(c->vars[0].loc, "a SCALAR CLASS has no instance variables");
    } else if (functions.count(c->name)) {
      diag_.error(c->loc, "class " + c->name + " has the name of a function");
    }
    std::set<std::string> messages; // a variable answers NAME and _NAME
    for (const auto *vars : {&c->vars, &c->classVars})
      for (const ClassVarDecl &v : *vars) {
        if (!messages.insert(v.name).second)
          diag_.error(v.loc, "'" + v.name + "' is declared twice in class " + c->name);
        messages.insert("_" + v.name);
      }
    bool destructor = false;
    for (const ClassMethodDecl &d : c->methods) {
      if (!messages.insert(d.message()).second)
        diag_.error(d.loc, "'" + d.name + "' is declared twice in class " + c->name);
      if (d.destructor && destructor) diag_.error(d.loc, "class " + c->name + " has a second DESTRUCTOR");
      destructor = destructor || d.destructor;
    }
  }
  for (auto &c : m.classes) {
    std::set<const ClassDecl *> seen;
    if (inherits(*c, *c, byName, seen)) diag_.error(c->loc, "class " + c->name + " inherits from itself");
  }
  // Implementations: the class named, else the last one declared before them (Parser).
  std::vector<std::unique_ptr<FuncDecl>> made;
  for (auto &f : m.funcs) {
    if (f->member == FuncDecl::Member::None) continue;
    auto found = byName.find(f->memberClass);
    if (found == byName.end()) {
      diag_.error(f->loc, "no class " + (f->memberClass.empty() ? std::string("before ") + f->name : f->memberClass) +
                              " in this module for this method");
      continue;
    }
    ClassDecl *cls = found->second;
    ClassMethodDecl *decl = nullptr;
    for (ClassMethodDecl &d : cls->methods)
      if (d.name == f->name && implements(f->member, d.kind)) decl = &d;
    if (!decl) {
      diag_.error(f->loc, "'" + f->name + "' is not declared in class " + cls->name);
      continue;
    }
    if (decl->impl || decl->inlineBody) {
      diag_.error(f->loc, "'" + f->name + "' of class " + cls->name + " is implemented twice");
      continue;
    }
    f->name = memberName(*cls, *decl);
    f->isStatic = true;
    f->ofClass = cls;
    withSelf(*f, f->loc);
    decl->impl = f.get();
  }
  for (auto &c : m.classes) {
    for (ClassMethodDecl &d : c->methods) {
      if (d.inlineBody && !d.impl) { // INLINE: a function returning its expression
        auto f = std::make_unique<FuncDecl>();
        f->loc = d.loc;
        f->name = memberName(*c, d);
        f->isStatic = true;
        f->member = d.kind;
        f->ofClass = c.get();
        f->params = d.params;
        f->variadic = d.variadic;
        withSelf(*f, d.loc);
        f->body.push_back(std::make_unique<ReturnStmt>(d.loc, std::move(d.inlineBody)));
        d.impl = f.get();
        made.push_back(std::move(f));
      } else if (!d.impl) {
        diag_.error(d.loc, "'" + d.name + "' of class " + c->name + " is declared but not implemented");
      }
    }
    if (!c->scalar) {
      auto classFn = std::make_unique<FuncDecl>(); // NAME(): a new object
      classFn->loc = c->loc;
      classFn->name = c->name;
      classFn->isStatic = c->isStatic;
      classFn->instantiates = c.get();
      made.push_back(std::move(classFn));
    }
    bool inits = false;
    for (const auto *vars : {&c->vars, &c->classVars})
      for (const ClassVarDecl &v : *vars) inits = inits || v.init;
    if (inits) { // the INIT values: the variables', then the class variables'
      auto f = std::make_unique<FuncDecl>();
      f->loc = c->loc;
      f->name = c->name + ":(INIT)";
      f->isStatic = true;
      auto values = std::make_unique<ArrayLitExpr>(c->loc);
      for (auto *vars : {&c->vars, &c->classVars})
        for (ClassVarDecl &v : *vars)
          values->elems.push_back(v.init ? std::move(v.init) : std::make_unique<NilExpr>(v.loc));
      f->body.push_back(std::make_unique<ReturnStmt>(c->loc, std::move(values)));
      c->initValues = f.get();
      made.push_back(std::move(f));
    }
  }
  for (auto &f : made) m.funcs.push_back(std::move(f));
}

} // namespace xb
