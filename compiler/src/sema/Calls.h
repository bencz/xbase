#pragma once

#include "parser/AST.h"
#include "common/Diagnostics.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace xb {

// Resolves every call of a module before the other passes: a call goes to a
// function the module defines, or to an EXTERN FUNCTION (include/rtl.ch for the
// runtime), whose name it may abbreviate when the declaration allows it, or
// else to a function of another module of the program, by name. It
// also applies the declarations' rules: RESERVED names cannot be defined, and
// CHECKED functions take the declared number of arguments. Calls are renamed
// to the full name and linked to their declaration (CallExpr::callee).
class CallResolver {
public:
  // `foldable` tells whether the compiler can evaluate a FOLD function.
  CallResolver(Diagnostics &diag, std::function<bool(const std::string &)> foldable)
      : diag_(diag), foldable_(std::move(foldable)) {}
  void run(Module &m);

private:
  const ExternDecl *findExtern(const std::string &name, SourceLoc loc) const;
  void call(CallExpr &c);
  void checkArgs(const CallExpr &c, const ExternDecl &d);
  void expr(Expr &e);
  void block(Block &b);

  Diagnostics &diag_;
  std::function<bool(const std::string &)> foldable_;
  std::unordered_set<std::string> funcs_;     // the module's functions
  std::unordered_map<std::string, const ExternDecl *> externs_;
  std::vector<const ExternDecl *> abbreviable_;
};

} // namespace xb
