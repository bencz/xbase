#pragma once

#include "common/Diagnostics.h"

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
  Ref,        // @var, @a[i]: a call argument passed by reference
  List,       // ( a, b, ... ): evaluated in order, the value of the last
  ArrayLit,   // { a, b, ... }
  Index,      // base[ index ]
  Block,      // {| params | exprs }
  Send,       // object:message, object:message( args )
  HashLit,    // { k => v, ... }, {=>}
  Spread,     // ... in the arguments of a call or send, or in an array literal: the function's own arguments past
              // its parameters
};

enum class BinOp : uint8_t {
  Add, Sub, Mul, Div, Mod, Pow, Eq, ExactEq, NotEq, Lt, Le, Gt, Ge, In,
  BitAnd, BitOr, BitXor, Shl, Shr, // & | ^^ << >> (xbase's own)
};
enum class UnOp : uint8_t { Neg, Plus, Not, BitNot }; // BitNot: ~ (xbase's own)

struct Param {
  SourceLoc loc;
  std::string name;
};

struct Expr {
  ExprKind kind;
  bool parenthesized = false; // written as ( expr )
  SourceLoc loc;
  explicit Expr(ExprKind k, SourceLoc l) : kind(k), loc(l) {}
  virtual ~Expr() = default;
};
using ExprPtr = std::unique_ptr<Expr>;

struct NilExpr : Expr {
  explicit NilExpr(SourceLoc l) : Expr(ExprKind::Nil, l) {}
};

struct SpreadExpr : Expr {
  explicit SpreadExpr(SourceLoc l) : Expr(ExprKind::Spread, l) {}
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

// Where a variable lives, set by Sema. A LOCAL or parameter of a function
// that a codeblock uses or that is passed by reference is "boxed": shared, it
// moves into a cell when that happens; the function's `boxed` table says
// which.
enum class VarWhere : uint8_t {
  Frame,     // slot of the current function's or codeblock's frame
  Capture,   // captured cell `slot` of the current codeblock
  Static,    // item `slot` of the module's STATICs
};

struct VarExpr : Expr {
  std::string name;              // upper-cased
  VarWhere where = VarWhere::Frame;
  uint32_t slot = 0;
  VarExpr(SourceLoc l, std::string n) : Expr(ExprKind::Var, l), name(std::move(n)) {}
};

struct ExternDecl;

struct CallExpr : Expr {
  std::string name;              // upper-cased; the full name once resolved
  std::vector<ExprPtr> args;
  const ExternDecl *callee = nullptr; // the declaration, when the call goes to an EXTERN FUNCTION
  bool declaredOnly = false;     // always the declared function, even if the module defines one (LOCAL a[n])
  bool external = false;         // neither defined nor declared: another module's function, found when linking
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
  ExprPtr target;                // an lvalue (isLvalue)
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

struct ArrayLitExpr : Expr {
  std::vector<ExprPtr> elems;
  explicit ArrayLitExpr(SourceLoc l) : Expr(ExprKind::ArrayLit, l) {}
};

struct HashLitExpr : Expr {     // evaluated key, value, key, value, ...
  std::vector<ExprPtr> keys, values;
  explicit HashLitExpr(SourceLoc l) : Expr(ExprKind::HashLit, l) {}
};

struct IndexExpr : Expr { // a[i, j] is Index( Index( a, i ), j )
  ExprPtr base, index;
  IndexExpr(SourceLoc l, ExprPtr b, ExprPtr i) : Expr(ExprKind::Index, l), base(std::move(b)), index(std::move(i)) {}
};

// A message sent to an object. Written without arguments, it reads an
// instance variable (or calls a method of that name); assigning it sends
// "_" + message with the value.
struct SendExpr : Expr {
  ExprPtr object;
  std::string message;           // upper-cased
  std::vector<ExprPtr> args;
  bool withArgs = false;         // written object:message( ... )
  SendExpr(SourceLoc l, ExprPtr o, std::string m)
      : Expr(ExprKind::Send, l), object(std::move(o)), message(std::move(m)) {}
};

// What can be assigned or incremented: a variable, an element, or an instance
// variable (a message written without arguments).
inline bool isLvalue(const Expr &e) {
  return e.kind == ExprKind::Var || e.kind == ExprKind::Index ||
         (e.kind == ExprKind::Send && !static_cast<const SendExpr &>(e).withArgs);
}

// A call argument passed by reference: the callee's parameter designates the
// variable or element, so assigning the parameter assigns it.
struct RefExpr : Expr {
  ExprPtr target;                // a VarExpr or an IndexExpr
  RefExpr(SourceLoc l, ExprPtr t) : Expr(ExprKind::Ref, l), target(std::move(t)) {}
};

struct ListExpr : Expr {
  std::vector<ExprPtr> exprs;    // at least two
  explicit ListExpr(SourceLoc l) : Expr(ExprKind::List, l) {}
};

struct BlockExpr : Expr {
  std::vector<Param> params;
  std::vector<ExprPtr> body;     // evaluated in order; the last one is the result
  // Set by Sema:
  uint32_t numLocals = 0;        // frame slots (the parameters)
  struct Capture {
    bool fromParentCapture;      // the parent's captured cell `index`, else the parent's frame slot `index`
    uint32_t index;
    bool operator==(const Capture &) const = default;
  };
  std::vector<Capture> captures; // cells this block holds, in VarExpr::slot order
  bool readsArgs = false;        // calls an ARGS function: its frame keeps the arguments it received
  explicit BlockExpr(SourceLoc l) : Expr(ExprKind::Block, l) {}
};

struct IIfExpr : Expr {
  ExprPtr cond, then, otherwise;
  IIfExpr(SourceLoc l, ExprPtr c, ExprPtr a, ExprPtr b)
      : Expr(ExprKind::IIf, l), cond(std::move(c)), then(std::move(a)), otherwise(std::move(b)) {}
};

enum class StmtKind : uint8_t { Expr, Local, Static, Return, If, While, For, ForEach, Switch, Sequence, Exit, Loop };

struct Stmt {
  StmtKind kind;
  SourceLoc loc;
  uint32_t line = 0;             // where it (its first line, for a construct) ends: the line the call stack shows
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

struct LocalStmt : Stmt {      // LOCAL, or STATIC (kind Static: `slot` indexes the module's statics)
  std::vector<LocalDecl> decls;
  explicit LocalStmt(SourceLoc l, StmtKind k = StmtKind::Local) : Stmt(k, l) {}
};

struct ReturnStmt : Stmt {
  ExprPtr value;                 // may be null
  ReturnStmt(SourceLoc l, ExprPtr v) : Stmt(StmtKind::Return, l), value(std::move(v)) {}
};

struct IfStmt : Stmt {
  struct Arm {
    ExprPtr cond;
    Block body;
    uint32_t line = 0;           // where the IF, ELSEIF or CASE line ends (see Stmt::line)
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

// FOR EACH v1 [, v2...] IN e1 [, e2...] [DESCEND]: each variable takes the
// elements of its collection in turn; the loop ends with the shortest one.
// FOR EACH k => v IN e: k also takes each element's key (a hash's) or position.
struct ForEachStmt : Stmt {
  ExprPtr key;                   // a VarExpr, or null
  std::vector<ExprPtr> vars;     // VarExprs
  std::vector<ExprPtr> collections;
  bool descend = false;
  Block body;
  explicit ForEachStmt(SourceLoc l) : Stmt(StmtKind::ForEach, l) {}
};

// SWITCH value: control enters at the first CASE equal to the value (or at
// OTHERWISE) and falls through the following ones until EXIT.
struct SwitchStmt : Stmt {
  struct Case {
    ExprPtr value;               // a constant; null for OTHERWISE
    Block body;
  };
  ExprPtr value;
  std::vector<Case> cases;       // OTHERWISE, if present, is last
  explicit SwitchStmt(SourceLoc l, ExprPtr v) : Stmt(StmtKind::Switch, l), value(std::move(v)) {}
};

// BEGIN SEQUENCE body [RECOVER [USING var] recover] END: a BREAK in the body
// (in what it calls, too) continues after RECOVER, the BREAK value in `var`;
// without RECOVER, after END.
struct SequenceStmt : Stmt {
  Block body;
  bool hasRecover = false;
  ExprPtr usingVar;              // a VarExpr, or null
  Block recover;
  explicit SequenceStmt(SourceLoc l) : Stmt(StmtKind::Sequence, l) {}
};

struct JumpStmt : Stmt { // EXIT / LOOP
  explicit JumpStmt(SourceLoc l, StmtKind k) : Stmt(k, l) {}
};

struct FuncDecl {
  SourceLoc loc;
  std::string name;              // upper-cased
  bool isProcedure = false;
  bool isStatic = false;         // STATIC FUNCTION: module-private (INIT and EXIT procedures too)
  enum class Startup : uint8_t { None, Init, Exit };
  Startup startup = Startup::None; // INIT PROCEDURE: runs before the start function; EXIT: after it
  bool implicit = false;         // code before the first FUNCTION/PROCEDURE
  std::vector<Param> params;
  bool variadic = false;         // `...` after the parameters: the body may pass on the arguments past them
  Block body;
  uint32_t numLocals = 0;        // params + LOCALs, set by Sema
  std::vector<bool> boxed;       // per slot, set by Sema: captured by a codeblock or passed by reference (shared)
  bool readsArgs = false;        // set by Sema: calls an ARGS function, so its frame keeps the arguments received
};

// EXTERN FUNCTION name( params ) attributes: a function the program calls but
// does not define (the runtime's are declared in include/rtl.ch).
struct ExternDecl {
  SourceLoc loc;
  std::string name;              // upper-cased
  uint32_t minArgs = 0;          // required parameters
  uint32_t maxArgs = 0;          // all parameters; kUnlimited with `...`
  bool reserved = false;         // RESERVED: the program cannot define it
  bool checked = false;          // CHECKED: argument count checked at compile time
  bool fold = false;             // FOLD: evaluated at compile time on constant arguments
  bool native = false;           // NATIVE: written in C to the xBase convention (else another module's function)
  bool args = false;             // ARGS: reads the arguments the function calling it received (PCount)
  bool params = false;           // PARAMS: ARGS, and reads and writes that function's parameters (PValue)
  uint32_t abbrev = 0;           // ABBREV n: shortest accepted abbreviation (0: none)
  std::vector<bool> byRef;       // NATIVE: per parameter, `@name`: receives a reference passed to it
  bool restByRef = false;        // NATIVE: `@...`: so do the arguments past the named parameters
  static constexpr uint32_t kUnlimited = UINT32_MAX;
  // Whether argument `k` written `@x` reaches the function as a reference. A
  // native function otherwise receives the value; an xBase function always
  // receives the reference.
  bool takesRef(size_t k) const { return !native || (k < byRef.size() ? byRef[k] : restByRef); }
  // The positions that take references as a mask (xb_refs_take in xbrt.h: bit 31 is the `@...`).
  uint32_t refMask() const {
    uint32_t m = !native || restByRef ? 1u << 31 : 0;
    for (uint32_t k = 0; k < 31; ++k)
      if (takesRef(k)) m |= 1u << k;
    return m;
  }
};

struct Module {
  std::string name;              // upper-cased source stem
  std::vector<std::unique_ptr<FuncDecl>> funcs;
  std::vector<LocalDecl> fileStatics; // STATICs before the first function: visible in the whole file
  std::vector<LocalDecl *> statics;   // every STATIC of the module, in slot order (set by Sema)
  std::vector<ExternDecl> externs;
};

} // namespace xb
