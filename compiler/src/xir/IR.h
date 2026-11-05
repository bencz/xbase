#pragma once

#include "parser/AST.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// xIR: the mid-level IR between the syntax tree and LLVM (docs/xir.md).
// Functions are basic blocks in SSA form; blocks take parameters, branches
// pass arguments to them. Values are typed: `Item` is an xBase value, the other
// types are machine values (conditions, counters).
namespace xb::xir {

enum class Type : uint8_t { Void, Bool, I64, Item };

const char *typeName(Type t);

class Instruction;
class BasicBlock;
class Function;
class Module;

// Something an instruction can use: a constant, a block parameter or an
// instruction's result. Every value records its users (an instruction appears
// once per operand that uses the value).
class Value {
public:
  enum class Kind : uint8_t { Constant, Param, Instruction };

  Value(Kind k, Type t) : kind_(k), type_(t) {}
  virtual ~Value() = default;
  Value(const Value &) = delete;
  Value &operator=(const Value &) = delete;

  Kind kind() const { return kind_; }
  Type type() const { return type_; }
  const std::vector<Instruction *> &users() const { return users_; }
  bool hasOneUse() const { return users_.size() == 1; }

  // Numbering for the printer and for analyses (set by Function::renumber).
  mutable uint32_t id = 0;

private:
  friend class Instruction;
  Kind kind_;
  Type type_;
  std::vector<Instruction *> users_;
};

// Constants: xBase literals (Item) and machine constants (Bool, I64).
class Constant : public Value {
public:
  enum class Lit : uint8_t { Nil, Logical, Integer, Double, String, Bool, I64 };

  Lit lit;
  bool logical = false;      // Logical, Bool
  int64_t ival = 0;          // Integer, I64
  double dval = 0.0;         // Double
  uint16_t width = 0;        // Integer, Double: display width
  uint16_t dec = 0;          // Double: decimals
  std::string str;           // String

  Constant(Lit l, Type t) : Value(Kind::Constant, t), lit(l) {}
};

class BlockParam : public Value {
public:
  BlockParam(BasicBlock *b, Type t, uint32_t i) : Value(Kind::Param, t), block(b), index(i) {}
  BasicBlock *block;
  uint32_t index;
};

// A memory place of a function: a frame variable (parameter or LOCAL), a
// variable a codeblock captured (reached through the block's cells), or a
// STATIC of the module. A frame variable or a STATIC moves into a cell when
// it is first shared (captured, passed by reference); its own item then holds
// the cell.
struct Var {
  enum class Kind : uint8_t { Frame, Capture, Static };
  Kind kind;
  uint32_t index;            // frame slot, capture index, or static index
  bool shared = false;       // Frame: captured by a codeblock or passed by reference somewhere
  bool param = false;        // Frame: a parameter (its caller may have passed a reference)
  std::string name;          // for the printer
};

// How a load or store reaches a variable's item.
enum class Access : uint8_t {
  Direct,     // the variable's own item (or its cell's)
  ThroughRef, // a FOR EACH variable of a loop in progress: the element its reference designates
  MaybeRef,   // a STATIC, captured variable or parameter: through the cell or element it designates if it
              // holds one (tested at run time; a parameter may have been passed by reference)
};

// Its own item may hold a cell: a STATIC, or a frame variable that is shared
// or a parameter. Others are never shared and never hold a reference.
inline bool mayBeInCell(const Var &v) {
  return v.kind == Var::Kind::Static || (v.kind == Var::Kind::Frame && (v.shared || v.param));
}

// A frame variable shared with codeblocks or callees: its own item usually
// holds its cell.
inline bool usuallyInCell(const Var &v) { return v.kind == Var::Kind::Frame && v.shared; }

// Reached directly in its frame slot (the access of a frame variable that is
// neither shared nor a parameter): only stores of the variable itself can
// change what a load of it reads, and nothing it holds lives in the heap.
inline bool isPlain(const Var &v, Access a) { return v.kind == Var::Kind::Frame && a == Access::Direct; }

enum class Op : uint8_t {
  // values
  NumDefaultDec,  // f64 immediate -> Item with SET DECIMALS decimals
  Load,           // var, access -> Item
  Store,          // var, access; value
  Barrier,        // var, access: write barrier after a store
  Unary,          // unop; a -> Item                        [may fail]
  Binary,         // binop; a, b -> Item                    [may fail]
  Truth,          // a -> Bool: .T./.F., anything else fails [may fail]
  FromBool,       // Bool -> Item: .T. or .F.
  Call,           // callee; args... -> Item                [may fail]
  ArrayLit,       // elems... -> Item
  Index,          // base, index -> Item                    [may fail]
  IndexSet,       // base, index, value                     [may fail]
  MakeBlock,      // block function, captured vars (moved into cells) -> Item
  ForTest,        // counter, limit, step -> Bool           [may fail]
  EnumLen,        // collection -> I64                      [may fail]
  EachInCell,     // var -> Bool: it is shared, in a cell the loop works inside (moved into one if it holds an
                  // element passed to it by reference)
  EachEnter,      // var; Bool in_cell, collection -> Item: saves the variable, installs the loop's reference
  EachMove,       // var; Bool in_cell, I64 index: the loop's reference at an element         [may fail]
  EachLeave,      // var; Bool in_cell, saved value: restores the variable
  RefVar,         // var -> Item: a reference to the variable, for an argument passed by reference
  RefIndex,       // base, index -> Item: a reference to an element (an argument @a[i]) [may fail]
  SwitchCheck,    // value                                  [may fail]
  SwitchMatch,    // value, case constant -> Bool
  IntAdd,         // I64, I64 -> I64
  IntSub,         // I64, I64 -> I64
  IntCmp,         // pred; I64, I64 -> Bool
  BoolAnd,        // Bool, Bool -> Bool
  BoolNot,        // Bool -> Bool
  // terminators
  Br,             // target(args)
  CondBr,         // Bool; then(args), else(args)
  Ret,            // [Item]
};

const char *opName(Op op);
bool isTerminator(Op op);

enum class IntPred : uint8_t { Eq, Ne, Lt, Le, Gt, Ge };

// The function a Call goes to: one of the module's (an xIR function) or an
// external one (a symbol of the runtime or of another module).
struct Callee {
  Function *function = nullptr; // the module's own
  std::string name;             // upper-cased xBase name
  bool native = false;          // external: written in C (runtime prefix), else another module's
};

// A branch destination with its arguments.
struct Target {
  BasicBlock *block = nullptr;
  std::vector<Value *> args;
};

class Instruction : public Value {
public:
  Instruction(Op o, Type t, BasicBlock *in) : Value(Kind::Instruction, t), op(o), parent(in) {}
  ~Instruction() override;

  const Op op;
  BasicBlock *parent;
  SourceLoc loc;

  // The operation can raise an error (and then leaves through `unwind`).
  bool mayFail() const;

  const std::vector<Value *> &operands() const { return operands_; }
  Value *operand(size_t i) const { return operands_[i]; }
  void addOperand(Value *v);
  void addTargetArg(size_t target, Value *v); // branch arguments are uses too
  void dropOperands();                         // operands and branch arguments

  // Operation-specific data.
  UnOp unop = UnOp::Neg;
  BinOp binop = BinOp::Add;
  IntPred pred = IntPred::Eq;
  double imm = 0.0;                   // NumDefaultDec
  Var *var = nullptr;                 // Load, Store, Barrier, Each*, RefVar
  Access access = Access::Direct;     // Load, Store, Barrier
  Callee callee;                      // Call
  Function *blockFn = nullptr;        // MakeBlock
  std::vector<Var *> captures;        // MakeBlock: the cells the block receives, in its capture order
  BasicBlock *unwind = nullptr;       // operations that may fail: where to go (null: the function's exit)
  std::vector<Target> targets;        // Br: one; CondBr: then, else (operand 0 is the condition)

private:
  std::vector<Value *> operands_;
};

class BasicBlock {
public:
  BasicBlock(Function *in, std::string label) : parent(in), name(std::move(label)) {}

  Function *parent;
  std::string name;
  std::vector<std::unique_ptr<BlockParam>> params;
  std::vector<std::unique_ptr<Instruction>> insts;

  BlockParam *addParam(Type t);
  Instruction *terminator() const;
  size_t numSuccessors() const; // the targets of the terminator, without allocating
  BasicBlock *successor(size_t k) const;
};

// A function of the module: a FUNCTION/PROCEDURE, a codeblock, or the
// module's initialization of its STATICs.
class Function {
public:
  enum class Kind : uint8_t { Program, Block, ModuleInit };

  Function(Kind k, std::string fnName) : kind(k), name(std::move(fnName)) {}
  ~Function();
  Function(const Function &) = delete;
  Function &operator=(const Function &) = delete;

  Kind kind;
  std::string name;               // Program: upper-cased xBase name; others: a symbol suffix
  bool internal = false;          // STATIC FUNCTION, codeblocks, module initialization
  uint32_t numParams = 0;
  std::vector<std::unique_ptr<Var>> vars;      // frame variables (index = slot), then captures
  std::vector<std::unique_ptr<BasicBlock>> blocks;
  std::vector<std::unique_ptr<Constant>> constants;

  BasicBlock *entry() const { return blocks.front().get(); }
  BasicBlock *addBlock(const std::string &name);
  Var *addVar(Var::Kind k, uint32_t index, std::string name);
  uint32_t numFrameVars() const;
  void renumber() const;          // ids for values, in block order
};

class Module {
public:
  std::string name;
  std::vector<std::unique_ptr<Function>> functions; // program functions first, in source order
  std::vector<std::unique_ptr<Var>> statics;        // index = static slot
  Function *init = nullptr;                         // initializes the STATICs (null: none have initializers)
};

} // namespace xb::xir
