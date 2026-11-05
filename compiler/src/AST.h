#pragma once

#include "Diagnostics.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace xb {

enum class ExprKind : uint8_t {
  Nil, Logical, Number, String,
  Var,        // identifier reference
  Call,       // name( args )
  Unary,      // - + .NOT.
  Binary,     // arithmetic / relational
  And, Or,    // short-circuit
  Assign,     // := and compound assignment
  IncDec,     // ++x x++ --x x--
  IIf,        // IIF( c, a, b ) / IF( c, a, b )
};

enum class BinOp : uint8_t { Add, Sub, Mul, Div, Mod, Pow, Eq, ExactEq, NotEq, Lt, Le, Gt, Ge, In };
enum class UnOp : uint8_t { Neg, Plus, Not };

struct Expr {
  ExprKind kind;
  SourceLoc loc;
  explicit Expr(ExprKind k, SourceLoc l) : kind(k), loc(l) {}
  virtual ~Expr() = default;
};
using ExprPtr = std::unique_ptr<Expr>;

struct NilExpr : Expr {
  explicit NilExpr(SourceLoc l) : Expr(ExprKind::Nil, l) {}
};

struct LogicalExpr : Expr {
  bool value;
  LogicalExpr(SourceLoc l, bool v) : Expr(ExprKind::Logical, l), value(v) {}
};

struct NumberExpr : Expr {
  bool isInt;
  int64_t ival;
  double dval;
  uint16_t dec;
  uint16_t width;                // display width of the integer part
  NumberExpr(SourceLoc l, bool integral, int64_t i, double d, uint16_t decimals, uint16_t displayWidth)
      : Expr(ExprKind::Number, l), isInt(integral), ival(i), dval(d), dec(decimals), width(displayWidth) {}
};

struct StringExpr : Expr {
  std::string value;
  StringExpr(SourceLoc l, std::string v) : Expr(ExprKind::String, l), value(std::move(v)) {}
};

struct VarExpr : Expr {
  std::string name;              // upper-cased
  uint32_t slot = 0;
  VarExpr(SourceLoc l, std::string n) : Expr(ExprKind::Var, l), name(std::move(n)) {}
};

struct CallExpr : Expr {
  std::string name;              // upper-cased
  std::vector<ExprPtr> args;
  CallExpr(SourceLoc l, std::string n) : Expr(ExprKind::Call, l), name(std::move(n)) {}
};

struct UnaryExpr : Expr {
  UnOp op;
  ExprPtr operand;
  UnaryExpr(SourceLoc l, UnOp o, ExprPtr e) : Expr(ExprKind::Unary, l), op(o), operand(std::move(e)) {}
};

struct BinaryExpr : Expr {
  BinOp op;
  ExprPtr lhs, rhs;
  BinaryExpr(SourceLoc l, BinOp o, ExprPtr a, ExprPtr b)
      : Expr(ExprKind::Binary, l), op(o), lhs(std::move(a)), rhs(std::move(b)) {}
};

struct LogicExpr : Expr { // And / Or
  ExprPtr lhs, rhs;
  LogicExpr(SourceLoc l, ExprKind k, ExprPtr a, ExprPtr b) : Expr(k, l), lhs(std::move(a)), rhs(std::move(b)) {}
};

struct AssignExpr : Expr {
  bool compound = false;         // x op= y
  BinOp op = BinOp::Add;         // valid when compound
  ExprPtr target;                // lvalue (VarExpr for now)
  ExprPtr value;
  AssignExpr(SourceLoc l, ExprPtr t, ExprPtr v)
      : Expr(ExprKind::Assign, l), target(std::move(t)), value(std::move(v)) {}
};

struct IncDecExpr : Expr {
  bool increment;
  bool prefix;
  ExprPtr target;
  IncDecExpr(SourceLoc l, bool inc, bool pre, ExprPtr t)
      : Expr(ExprKind::IncDec, l), increment(inc), prefix(pre), target(std::move(t)) {}
};

struct IIfExpr : Expr {
  ExprPtr cond, then, otherwise;
  IIfExpr(SourceLoc l, ExprPtr c, ExprPtr a, ExprPtr b)
      : Expr(ExprKind::IIf, l), cond(std::move(c)), then(std::move(a)), otherwise(std::move(b)) {}
};

enum class StmtKind : uint8_t { Expr, Local, Return, If, While, For, Exit, Loop };

struct Stmt {
  StmtKind kind;
  SourceLoc loc;
  Stmt(StmtKind k, SourceLoc l) : kind(k), loc(l) {}
  virtual ~Stmt() = default;
};
using StmtPtr = std::unique_ptr<Stmt>;
using Block = std::vector<StmtPtr>;

struct ExprStmt : Stmt {
  ExprPtr expr;
  ExprStmt(SourceLoc l, ExprPtr e) : Stmt(StmtKind::Expr, l), expr(std::move(e)) {}
};

struct LocalDecl {
  SourceLoc loc;
  std::string name;
  ExprPtr init;                  // may be null
  uint32_t slot = 0;             // set by Sema
};

struct LocalStmt : Stmt {
  std::vector<LocalDecl> decls;
  explicit LocalStmt(SourceLoc l) : Stmt(StmtKind::Local, l) {}
};

struct ReturnStmt : Stmt {
  ExprPtr value;                 // may be null
  ReturnStmt(SourceLoc l, ExprPtr v) : Stmt(StmtKind::Return, l), value(std::move(v)) {}
};

struct IfStmt : Stmt {
  struct Arm {
    ExprPtr cond;
    Block body;
  };
  std::vector<Arm> arms;         // IF + ELSEIFs
  Block elseBody;
  explicit IfStmt(SourceLoc l) : Stmt(StmtKind::If, l) {}
};

struct WhileStmt : Stmt {
  ExprPtr cond;
  Block body;
  WhileStmt(SourceLoc l, ExprPtr c) : Stmt(StmtKind::While, l), cond(std::move(c)) {}
};

struct ForStmt : Stmt {
  ExprPtr var;                   // VarExpr
  ExprPtr start, limit, step;    // step may be null (= 1)
  Block body;
  explicit ForStmt(SourceLoc l) : Stmt(StmtKind::For, l) {}
};

struct JumpStmt : Stmt { // EXIT / LOOP
  explicit JumpStmt(SourceLoc l, StmtKind k) : Stmt(k, l) {}
};

struct Param {
  SourceLoc loc;
  std::string name;
};

struct FuncDecl {
  SourceLoc loc;
  std::string name;              // upper-cased
  bool isProcedure = false;
  bool isStatic = false;         // STATIC FUNCTION: module-private
  bool implicit = false;         // code before the first FUNCTION/PROCEDURE
  std::vector<Param> params;
  Block body;
  uint32_t numLocals = 0;        // params + LOCALs, set by Sema
};

struct Module {
  std::string name;              // upper-cased source stem
  std::vector<std::unique_ptr<FuncDecl>> funcs;
};

} // namespace xb
