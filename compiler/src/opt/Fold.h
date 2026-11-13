#pragma once

#include "parser/AST.h"

#include <optional>
#include <string>

namespace xb {

// Decimal count of a number whose decimals are "the SET DECIMALS value at run
// time" (the result of folding `/`, `%` on doubles, or `^`). Never reaches the
// runtime: code generation materializes such numbers with a runtime call.
constexpr uint16_t kDefaultDecimals = 255;

// The result of `x op y` for two strings and a comparison for equality (=, ==, !=), when it does not depend on SET
// EXACT at run time: == compares the bytes; = and != compare the bytes of strings of one length (SET EXACT changes
// only how a longer string compares with a shorter one).
std::optional<bool> stringEquality(BinOp op, const std::string &x, const std::string &y);

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
