#pragma once

#include "parser/AST.h"
#include "xbrt.h" // the folding rules

#include <optional>
#include <string>
#include <vector>

namespace xb {

// Decimal count of a number whose decimals are "the SET DECIMALS value at run
// time" (the result of folding `/`, `%` on doubles, or `^`; XB_DEC_AT_RUN of the folding rules in xbrt.h). Never
// reaches the runtime: code generation materializes such numbers with a runtime call.
constexpr uint16_t kDefaultDecimals = XB_DEC_AT_RUN;

// The result of `x op y` for two strings and a comparison for equality (=, ==, !=) or `$`, when it does not depend on
// SET EXACT at run time: == compares the bytes; = and != compare the bytes of strings of one length (SET EXACT changes
// only how a longer string compares with a shorter one); `$` finds x in y (an empty x nowhere).
std::optional<bool> stringTest(BinOp op, const std::string &x, const std::string &y);
// `x op y` for two strings and `+` or `-` (x's trailing spaces moved to the end), or nothing for another operator.
std::optional<std::string> stringJoin(BinOp op, const std::string &x, const std::string &y);

// `a op b` for two numbers and an arithmetic or bit operator, as a number literal, or null when it is left to run
// time (a division by zero, and so on). Without `runtime`, as literals fold (an exact division of two integers is an
// integer, a quotient's decimals are SET DECIMALS at run time: kDefaultDecimals); with it, as the run-time operator
// makes it from two values (an integer is a computed one, of width 0; /, %, ^ and an integer product that overflows,
// whose decimals are SET DECIMALS, and a product with more decimals than a literal holds, give null).
ExprPtr foldNumbers(SourceLoc loc, BinOp op, const NumberExpr &a, const NumberExpr &b, bool runtime);
// `op n` for a number and `-` or `~`, as a number literal (with `runtime`, as the run-time operator makes it: an
// integer is a computed one), or null for another operator.
ExprPtr foldNumber(SourceLoc loc, UnOp op, const NumberExpr &n, bool runtime);
// `a op b` for two logicals and a comparison operator (.F. before .T.), or nothing for another operator.
std::optional<bool> compareLogicals(BinOp op, bool a, bool b);
// `a op b` for constants of which one at least is NIL (aNil, bNil) and the other no object: = and == of two NILs,
// != of NIL and another value (xb_fold_nil), or nothing for another operator (an error at run time).
std::optional<bool> compareNil(BinOp op, bool aNil, bool bNil);
// `a op b` for two numbers and a comparison operator (as xb_compare orders them), or nothing for another operator.
std::optional<bool> compareNumbers(BinOp op, const NumberExpr &a, const NumberExpr &b);

// A call of runtime function `name` (one declared FOLD) on literal arguments, as the function computes it, or null
// when it is left to run time (no evaluator, arguments it does not take, errors included). With `literal`, on the
// syntax tree: only a call the language evaluates when compiling (Len, Chr, Asc, At, Upper of the text
// xb_upper_folds accepts, Empty, the bit functions), whose result is a literal; without it, on xIR constants: every
// call that has an evaluator, giving the value the function returns at run time.
ExprPtr evaluateCall(const std::string &name, SourceLoc loc, const std::vector<ExprPtr> &args, bool literal);

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
  void fold(ExprPtr &e);
  ExprPtr foldBinary(BinaryExpr &b);
  ExprPtr foldUnary(UnaryExpr &u);
  ExprPtr foldLogic(LogicExpr &l, bool isAnd);
  ExprPtr foldCall(CallExpr &c);
};

} // namespace xb
