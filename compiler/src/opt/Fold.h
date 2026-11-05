#pragma once

#include "parser/AST.h"

#include <string>

namespace xb {

// Decimal count of a number whose decimals are "the SET DECIMALS value at run
// time" (the result of folding `/`, `%` on doubles, or `^`). Never reaches the
// runtime: code generation materializes such numbers with a runtime call.
constexpr uint16_t kDefaultDecimals = 255;

// Compile-time evaluation of expressions on literals (constant folding). It
// runs after call resolution and before Sema; calls are evaluated only for
// functions declared FOLD (see sema/Calls.h).
// Folding is part of the language semantics, not only an optimization: an
// exact integer division of literals is an integer, while the run-time `/`
// always produces a double.
class Folder {
public:
  void run(Module &m);
  static bool canEvaluate(const std::string &name); // has an evaluator for a FOLD function

private:
  void block(Block &b);
  void stmt(Stmt &s);
  void fold(ExprPtr &e);
  ExprPtr foldBinary(BinaryExpr &b);
  ExprPtr foldUnary(UnaryExpr &u);
  ExprPtr foldLogic(LogicExpr &l, bool isAnd);
  ExprPtr foldCall(CallExpr &c);
};

} // namespace xb
