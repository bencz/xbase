#pragma once

#include "parser/AST.h"
#include "xir/Ops.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// xIR: the mid-level IR between the syntax tree and LLVM (docs/xir.md).
// Functions are basic blocks in SSA form; blocks take parameters, branches
// pass arguments to them. Values are typed: `Item` is an xBase value, the other
// types are machine values (conditions, counters).
namespace xb::xir {

// Args: a list of items of a length known at run time (args.rest), only an operand of a call, a send or an array
// literal.
enum class Type : uint8_t { Void, Bool, I64, Item, Args };

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
  void replaceAllUsesWith(Value *with); // every operand and branch argument using this value now uses `with`

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

// The value as a constant, a machine boolean constant, or a block parameter;
// null when it is something else.
inline const Constant *asConstant(const Value *v) {
  return v->kind() == Value::Kind::Constant ? static_cast<const Constant *>(v) : nullptr;
}
inline const Constant *asBool(const Value *v) {
  const Constant *c = asConstant(v);
  return c && c->lit == Constant::Lit::Bool ? c : nullptr;
}
inline const BlockParam *asParam(const Value *v) {
  return v->kind() == Value::Kind::Param ? static_cast<const BlockParam *>(v) : nullptr;
}

// The block that defines the value (null for a constant).
const BasicBlock *definingBlock(const Value *v);

// A function of the program that reads no arguments as received (readsArgs, `...`) and is not the body versioned for
// references: calls of its module pass the parameters' items in registers to its direct entry (codegen).
bool hasDirectEntry(const Function &f);
// A call that goes to a direct entry: to such a function, without `...` (it then needs no argument slots).
bool isDirectCall(const Instruction &i);

// Whether two values are always the same: the same value, or equal constants
// (each use of a literal may be its own Constant).
bool sameValue(const Value *a, const Value *b);

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

// Reached directly in its frame slot (the access of a frame variable that is
// neither shared nor a parameter): only stores of the variable itself can
// change what a load of it reads, and nothing it holds lives in the heap.
inline bool isPlain(const Var &v, Access a) { return v.kind == Var::Kind::Frame && a == Access::Direct; }

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

  // The last operand is an args.rest: a list with `...` (the only place an args value may be).
  bool hasRest() const;
  // The operation's effects (Effect in Ops.h): its op's, and for a list with
  // `...`, those of reading references (an error, the error block's code).
  uint16_t effects() const;
  // The operation can raise an error (the function then returns: the error
  // is pending in the context).
  bool mayFail() const;
  // Copies the operation-specific data below (not operands nor targets).
  void copyData(const Instruction &from);

  const std::vector<Value *> &operands() const { return operands_; }
  Value *operand(size_t i) const { return operands_[i]; }
  void addOperand(Value *v);
  void addTargetArg(size_t target, Value *v); // branch arguments are uses too
  void dropOperands();                         // operands and branch arguments
  void replaceUses(Value *from, Value *to);    // in operands and branch arguments
  void setTarget(size_t target, BasicBlock *to, std::vector<Value *> args);
  void removeTargetArg(size_t target, size_t k);

  // Operation-specific data.
  UnOp unop = UnOp::Neg;
  BinOp binop = BinOp::Add;
  IntPred pred = IntPred::Eq;
  double imm = 0.0;                   // NumDefaultDec
  Var *var = nullptr;                 // Load, Store, Barrier, Each*, RefVar
  Access access = Access::Direct;     // Load, Store, Barrier
  Callee callee;                      // Call
  std::string message;                // Send: upper-cased, "_NAME" to assign NAME
  // Call with `...` (an args.rest operand): the argument positions that receive the arguments passed on as they are,
  // references included (bit k: argument k, bit 31: argument 31 and after; xb_refs_take); the others, and those of
  // a send or an array literal, receive their values.
  uint32_t restRefs = 0;
  Function *blockFn = nullptr;        // MakeBlock
  std::vector<Var *> captures;        // MakeBlock: the cells the block receives, in its capture order
  std::vector<Target> targets;        // Br: one; CondBr: then, else (operand 0 is the condition)

private:
  static void removeUse(Value *v, Instruction *user); // one occurrence of `user` in v's users
  std::vector<Value *> operands_;
};

// The value as an instruction of operation `op`; null when it is something else.
inline const Instruction *asInstruction(const Value *v, Op op) {
  if (v->kind() != Value::Kind::Instruction) return nullptr;
  const auto *i = static_cast<const Instruction *>(v);
  return i->op == op ? i : nullptr;
}

class BasicBlock {
public:
  BasicBlock(Function *in, std::string label) : parent(in), name(std::move(label)) {}

  Function *parent;
  std::string name;
  std::vector<std::unique_ptr<BlockParam>> params;
  std::vector<std::unique_ptr<Instruction>> insts;
  // Where an operation of this block that fails or leaves a request to unwind
  // goes, with the request pending: a block without parameters inside the
  // function (a BEGIN SEQUENCE handler, a FOR EACH cleanup); null: the function
  // returns. It is the block's last successor in the control flow graph.
  BasicBlock *unwind = nullptr;

  BlockParam *addParam(Type t);
  // Removes argument k of `branches`, the terminators that branch here (see
  // predecessorTerminators in Analysis.h); then parameter k, once nothing uses
  // it. Parameters that only feed each other (a cycle through a loop) lose
  // all their arguments first, then go.
  void removeArgs(size_t k, const std::vector<Instruction *> &branches);
  void eraseParam(size_t k);
  void removeParam(size_t k, const std::vector<Instruction *> &branches) {
    removeArgs(k, branches);
    eraseParam(k);
  }
  Instruction *terminator() const;
  std::unique_ptr<Instruction> takeTerminator(); // removed with its uses undone
  size_t numSuccessors() const; // the targets of the terminator, then the unwind block, without allocating
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
  std::string stackName;          // what ProcName() and "Called from" show: the xBase name, "(b)NAME" for a
                                  // codeblock of NAME; empty for the module initialization
  bool internal = false;          // STATIC FUNCTION, codeblocks, module initialization, INIT and EXIT procedures
  bool startup = false;           // an INIT or EXIT procedure: the runtime runs it, nothing calls it
  uint32_t numParams = 0;
  bool readsArgs = false;         // its frame keeps the arguments received (PCount(), PValue())
  bool hidden = false;            // the call stack does not show it (a STATIC function or codeblock of the runtime)
  bool byRefBody = false;         // the body versioned for references (NAME.byref), reached by call.forward
  bool usesRest = false;          // has an args.rest (`...`)
  std::vector<std::unique_ptr<Var>> vars;      // frame variables (index = slot), then captures
  std::vector<std::unique_ptr<BasicBlock>> blocks;
  std::vector<std::unique_ptr<Constant>> constants;

  BasicBlock *entry() const { return blocks.front().get(); }
  BasicBlock *addBlock(const std::string &name);
  Var *addVar(Var::Kind k, uint32_t index, std::string name);
  void renumber() const;          // ids for values, in block order

private:
  std::unordered_map<std::string, unsigned> blockNames_; // uses of each block name, for unique names
};

class Module {
public:
  std::string name;
  std::vector<std::unique_ptr<Function>> functions; // program functions first, in source order
  std::vector<std::unique_ptr<Var>> statics;        // index = static slot
  Function *init = nullptr;                         // initializes the STATICs (null: none have initializers)
  std::vector<Function *> initProcs, exitProcs;     // INIT and EXIT procedures, in source order
  std::vector<std::string> requests;                // functions of other modules to link in (EXTERNAL)
};

} // namespace xb::xir
