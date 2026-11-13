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

// `a op b` for two numbers and an arithmetic or bit operator, as a number literal, or null when it is left to run
// time (a division by zero, and so on). Without `runtime`, as literals fold (an exact division of two integers is an
// integer, a quotient's decimals are SET DECIMALS at run time: kDefaultDecimals); with it, as the run-time operator
// makes it from two values (an integer is a computed one, of width 0; /, %, ^ and an integer product that overflows,
// whose decimals are SET DECIMALS, and a product with more decimals than a literal holds, give null).
ExprPtr foldNumbers(SourceLoc loc, BinOp op, const NumberExpr &a, const NumberExpr &b, bool runtime);
// `op n` for a number and `-` or `~`, as a number literal (with `runtime`, as the run-time operator makes it: an
// integer is a computed one), or null for another operator.
ExprPtr foldNumber(SourceLoc loc, UnOp op, const NumberExpr &n, bool runtime);
// `a op b` for two numbers and a comparison operator (as xb_compare orders them), or nothing for another operator.
std::optional<bool> compareNumbers(BinOp op, const NumberExpr &a, const NumberExpr &b);

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
