#pragma once

#include "parser/AST.h"

#include <string>
#include <unordered_set>

namespace xb {

// Decimal count of a number whose decimals are "the SET DECIMALS value at run
// time" (the result of folding `/`, `%` on doubles, or `^`). Never reaches the
// runtime: code generation materializes such numbers with a runtime call.
constexpr uint16_t kDefaultDecimals = 255;

// Compile-time evaluation of expressions on literals (constant folding).
// Folding is part of the language semantics, not only an optimization: an
// exact integer division of literals is an integer, while the run-time `/`
// always produces a double.
class Folder {
public:
  void run(Module &m);

private:
  void block(Block &b);
  void stmt(Stmt &s);
  void fold(ExprPtr &e);
  ExprPtr foldBinary(BinaryExpr &b);
  ExprPtr foldUnary(UnaryExpr &u);
  ExprPtr foldLogic(LogicExpr &l, bool isAnd);
  ExprPtr foldCall(CallExpr &c);

  std::unordered_set<std::string> moduleFuncs_; // user functions shadow runtime ones
};

} // namespace xb
