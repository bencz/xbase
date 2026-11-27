// Classes to functions (see Classes.h).
#include "sema/Classes.h"

#include "xbrt.h" // xb_type_class_name

#include <algorithm>
#include <set>
#include <unordered_map>

namespace xb {

namespace {

// The function name of a member: "CLS:MESSAGE", what the call stack shows.
std::string memberName(const ClassDecl &c, const ClassMethodDecl &m) { return c.name + ":" + m.message(); }

// The parameters of a method: SELF, then those written.
void withSelf(FuncDecl &f, SourceLoc loc) { f.params.insert(f.params.begin(), Param{loc, "SELF", {}}); }

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

// The first class of the module, `c` or one it inherits from, that declares instance variables; null for none.
const ClassDecl *withVariables(const ClassDecl &c, const std::unordered_map<std::string, ClassDecl *> &byName,
                               std::set<const ClassDecl *> &seen) {
  if (!seen.insert(&c).second) return nullptr;
  if (!c.vars.empty()) return &c;
  for (const std::string &p : c.parents)
    if (auto it = byName.find(p); it != byName.end())
      if (const ClassDecl *v = withVariables(*it->second, byName, seen)) return v;
  return nullptr;
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
      bool known = false;
      for (uint32_t t = 0; t < XB_T_COUNT_; ++t) known |= xb_type_class_name(t) && c->name == xb_type_class_name(t);
      if (!known)
        diag_.error(c->loc, "SCALAR CLASS " + c->name + ": not the name of a type as ClassName() gives it");
      if (!c->vars.empty()) diag_.error(c->vars[0].loc, "a SCALAR CLASS has no instance variables");
    } else if (functions.count(c->name)) {
      diag_.error(c->loc,
                  std::string(c->isInterface ? "interface " : "class ") + c->name + " has the name of a function");
    }
    std::set<std::string> messages; // a variable answers NAME and _NAME
    for (const auto *vars : {&c->vars, &c->classVars})
      for (const ClassVarDecl &v : *vars) {
        if (!messages.insert(v.name).second)
          diag_.error(v.loc, "'" + v.name + "' is declared twice in class " + c->name);
        messages.insert("_" + v.name);
      }
    bool destructor = false;
    if (c->isAbstract && c->isFinal) diag_.error(c->loc, "class " + c->name + " is ABSTRACT or FINAL, not both");
    for (const ClassMethodDecl &d : c->methods) {
      if (!messages.insert(d.message()).second)
        diag_.error(d.loc, "'" + d.name + "' is declared twice in " + (c->isInterface ? "interface " : "class ") +
                               c->name);
      if (d.destructor && destructor) diag_.error(d.loc, "class " + c->name + " has a second DESTRUCTOR");
      destructor = destructor || d.destructor;
      bool abstract = (d.flags & XB_MEMBER_ABSTRACT) != 0;
      if (abstract && (d.flags & XB_MEMBER_FINAL))
        diag_.error(d.loc, "'" + d.name + "' is ABSTRACT or FINAL, not both");
      if (abstract && d.inlineBody && !c->isInterface) // an interface's: the parser's error
        diag_.error(d.loc, "'" + d.name + "' is ABSTRACT: it has no body (INLINE)");
      if (abstract && !c->isAbstract && !c->isInterface)
        diag_.error(d.loc, "'" + d.name + "' is ABSTRACT, so class " + c->name + " must be an ABSTRACT CLASS");
    }
  }
  for (auto &c : m.classes) {
    std::set<const ClassDecl *> seen;
    if (inherits(*c, *c, byName, seen)) {
      diag_.error(c->loc, std::string(c->isInterface ? "interface " : "class ") + c->name + " inherits from itself");
      continue;
    }
    for (const std::string &p : c->isInterface ? c->parents : c->interfaces) // the module's own: an interface each
      if (auto it = byName.find(p); it != byName.end() && !it->second->isInterface)
        diag_.error(c->loc, std::string(c->isInterface ? "interface " + c->name + " inherits from " : "IMPLEMENTS ") +
                                p + ", which is a class");
    seen.clear();
    // A value of a type has no area for variables: nor its class's own nor inherited ones (a class of another module
    // the runtime checks when the class first answers).
    if (const ClassDecl *v = c->scalar && c->vars.empty() ? withVariables(*c, byName, seen) : nullptr)
      diag_.error(c->loc, "a SCALAR CLASS has no instance variables: " + c->name + " inherits those of " + v->name);
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
    if (decl->flags & XB_MEMBER_ABSTRACT) {
      diag_.error(f->loc, "'" + f->name + "' of " + (cls->isInterface ? "interface " : "class ") + cls->name +
                              " is ABSTRACT: it has no body");
      continue;
    }
    if (decl->async != f->async)
      diag_.error(f->loc, "'" + f->name + "' of class " + cls->name + (decl->async ? " is" : " is not") +
                              " declared ASYNC: its implementation must match");
    f->name = memberName(*cls, *decl);
    f->isStatic = true;
    f->ofClass = cls;
    withSelf(*f, f->loc);
    decl->impl = f.get();
  }
  for (auto &c : m.classes) {
    if (c->isInterface) continue; // messages alone: no bodies, no objects
    for (ClassMethodDecl &d : c->methods) {
      if ((d.flags & XB_MEMBER_ABSTRACT) && !d.impl) { // its body raises the error: reached through Super only
        auto f = std::make_unique<FuncDecl>();
        f->loc = d.loc;
        f->name = memberName(*c, d);
        f->isStatic = true;
        f->member = d.kind;
        f->ofClass = c.get();
        f->params = d.params;
        f->variadic = d.variadic;
        f->async = d.async; // as an implementation's would be
        withSelf(*f, d.loc);
        auto call = std::make_unique<CallExpr>(d.loc, "__ABSTRACTMETHOD");
        call->args.push_back(std::make_unique<StringExpr>(d.loc, f->name));
        f->body.push_back(std::make_unique<ReturnStmt>(d.loc, std::move(call)));
        d.impl = f.get();
        made.push_back(std::move(f));
      } else if (d.inlineBody && !d.impl) { // INLINE: a function returning its expression
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
