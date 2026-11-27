#pragma once

#include <cstdint>

namespace xb::xir {

// The operations of xIR and what each one does (one row per operation in
// Ops.cpp): the catalog passes consult instead of switching on operations.
// The pieces of one join (JoinStep, JoinEnd), at most: join-strings splits a longer chain, so that its steps' operands
// and the array of their addresses stay small.
constexpr unsigned kMaxJoinPieces = 16;

enum class Op : uint8_t {
  // values
  NumDefaultDec,  // f64 immediate -> Item with SET DECIMALS decimals
  Load,           // var, access -> Item
  Store,          // var, access; value
  Barrier,        // var, access: write barrier after a store
  Append,         // var, access; value: var := var + value, a string growing in place [may fail]
  Unary,          // unop; a -> Item                        [may fail]
  Binary,         // binop; a, b -> Item                    [may fail]
  JoinStep,       // pieces 0 and 1, or the step before and pieces 0 to k -> Item: a `+` of a chain (join-strings,
                  // xb_op_join): while the pieces are strings the pending mark, else the sum so far [may fail]
  JoinEnd,        // the step before, every piece -> Item: the chain's last `+`, its strings joined in one [may fail]
  Truth,          // a -> Bool: .T./.F., anything else fails [may fail]; index 1: the operand of a .NOT., whose
                  // class's .NOT. answers for an object (xb_cond_not)
  FromBool,       // Bool -> Item: .T. or .F.
  Call,           // callee; args... -> Item                [may fail]
  Send,           // message; object, args... -> Item       [may fail]
  SendMacro,      // text, object, args... -> Item: the message the text names (index 1: assigns) [may fail]
  ArrayLit,       // elems... -> Item
  HashLit,        // key, value, key, value... -> Item       [may fail]
  Index,          // base, index -> Item                    [may fail]
  TableIndex,     // var (a table: Var::items, a STATIC's or a frame variable's); index -> Item: its element [may fail]
  TableLen,       // var (a frame variable's table) -> Item: its length (its call's array's, once it has one: by the
                  // callee, the runtime's Len)
  IndexSet,       // base, index, value                     [may fail]
  MakeBlock,      // block function, captured vars (moved into cells) -> Item
  ForTest,        // counter, limit, step -> Bool           [may fail]
  EnumLen,        // collection -> I64                      [may fail]
  EachSource,     // collection, Bool descend -> Item: what the loop goes through: an object's enumerator (for
                  // DESCEND, its elements in an array), else the collection                       [may fail]
  EachInCell,     // var -> Bool: it is shared, in a cell the loop works inside (moved into one if it holds an
                  // element passed to it by reference)
  EachEnter,      // var; Bool in_cell, collection -> Item: saves the variable, installs the loop's reference
  EachMove,       // var; Bool in_cell, I64 index, collection (each.source's): the loop's reference at an element
                  //                                                                            [may fail]
  EachLeave,      // var; Bool in_cell, saved value: restores the variable
  EachKey,        // collection, I64 index -> Item: the key of a hash's entry, else the index      [may fail]
  RefVar,         // var -> Item: a reference to the variable, for an argument passed by reference
  RefIndex,       // base, index -> Item: a reference to an element (an argument @a[i]) [may fail]
  SwitchCheck,    // value -> Item: the value the CASEs match [may fail]
  SwitchMatch,    // value, case constant -> Bool
  ParamsRef,      // parameters (captures) -> Bool: one of them holds a reference its caller passed
  CallForward,    // callee -> Item: calls it with this function's own arguments, as received [may fail]
  ClassNew,       // -> Item: a new object of class `classIndex` (its INIT values run once)  [may fail]
  ObjectGet,      // object -> Item: its instance variable `index` (a send resolved for the object's known class)
  ObjectSet,      // object, value -> Item: assigns its instance variable `index`, giving the value
  FuncRef,        // callee -> Item: the function itself (@name(), a symbol: XB_T_SYMBOL)
  // Fields of work areas, by name (`message`): of the work area of `alias` ("" the current one), or of the one an
  // operand names (an alias or a number); a missing alias or field is an error the error block may retry.
  FieldGet,       // [area] -> Item: the field's value                              [may fail]
  FieldSet,       // [area,] value: writes the field                               [may fail]
  // Memory variables, by name (`message`): what the name designates is decided at run time, and any call may
  // create, change or end one, so they are read and written like the heap.
  MvGet,          // -> Item: the value of the variable visible; none is error 1003, retried or fatal [may fail]
  MvField,        // -> Item: an undeclared name read: the field of the current work area, else as MvGet [may fail]
  MvSet,          // value: assigns it; with none, a PRIVATE of the activation is created [may fail]
  MvRef,          // -> Item: a reference to it (an argument @name) [may fail]
  MvPrivate,      // PRIVATE name: a new variable, unless the activation has one
  MvPublic,       // PUBLIC name: a new variable, unless one is visible
  MvParam,        // PARAMETERS: a PRIVATE holding argument `index` of the function
  // The macro operator: the text (an item, the first operand) is compiled and run when the code runs; it may do
  // anything a call does, and create PRIVATEs of the activation.
  MacroGet,       // text -> Item: its value                                    [may fail]
  MacroSet,       // text, value: assigns what it designates                     [may fail]
  MacroCall,      // text, args... -> Item: calls the function it names           [may fail]
  MacroRef,       // text -> Item: a reference to the variable or element it designates (@&text) [may fail]
  MacroPrivate,   // text: PRIVATE &text                                         [may fail]
  MacroPublic,    // text: PUBLIC &text                                          [may fail]
  MacroText,      // string -> Item: `&name` in it replaced by the memory variable's text (a string literal)
  ArgsRest,       // -> Args: the arguments the function received past its parameters (`...`), right before the
                  // call, send or array literal that is its one use; read there, they may have changed before
                  // (PValue()), so it is never merged or moved
  SeqCatch,       // -> Bool: in an unwind block of BEGIN SEQUENCE, the request is a BREAK alone (then taken)
  SeqValue,       // -> Item: the BREAK value (RECOVER USING), moved out of the context
  SeqWith,        // Item -> Item: BEGIN SEQUENCE WITH: the handler becomes the error block; the one it replaces
  SeqRestore,     // Item: that error block back
  SeqSuspend,     // -> I64: in an unwind block, the requests to unwind, set aside (ALWAYS runs without them)
  SeqResume,      // I64, Item: those requests and the BREAK value back, unless ALWAYS made a request of its own
  SubstCheck,     // operation -> Bool: right after it, its result is not of the types it gives itself (an error's
                  // block substituted it); with a mask (`imm`), a guard: the value is not of those types (also of a
                  // block parameter, first in its block)
  SubstValue,     // operation -> Item: that value, at the start of the block the check leads to
  HasOperator,    // Item; message (XB_OPERATOR_*) -> Bool: the value is an object whose class overloads that operator
  ScalarPlain,    // Item; index: a mask of types -> Bool: the value is of one of those types, whose own class of the
                  // runtime answers its messages, no class of the program before it (xb_scalar_replaced): a method of
                  // that class may be called directly
  ObjectIs,       // Item; classIndex: a class of the module -> Bool: the value is an object of that class exactly,
                  // whose messages are resolved when compiling (speculate-sends)
  IntAdd,         // I64, I64 -> I64
  IntSub,         // I64, I64 -> I64
  IntCmp,         // pred; I64, I64 -> Bool
  BoolAnd,        // Bool, Bool -> Bool
  BoolNot,        // Bool -> Bool
  BoxI64,         // I64 -> Item: the number (a value a generator keeps across YIELD, in a frame variable)
  BoxBool,        // Bool -> Item: the logical
  UnboxI64,       // Item -> I64: the integer such an item holds
  UnboxBool,      // Item -> Bool: the logical such an item holds
  GenNew,         // callee: a generator's resume function; args -> Item: the enumerator, its arguments saved
  AgenNew,        // callee: an asynchronous generator's resume function; args -> Item: its enumerator, as gen.new
  GenRestore,     // var: the enumerator (parameter 0) -> I64: its frame back from the enumerator; the state to resume
                  // in (0 at the start)
  GenDone,        // var: the generator ended: the enumerator has no more elements
  AsyncNew,       // callee: an ASYNC function's resume function; args -> Item: its task, once its first step ran here
  AsyncRestore,   // var: the task (parameter 0) -> I64: its frame back from the task (the arguments at the first step);
                  // the point to resume at (0 at the start)
  AwaitReady,     // Item -> Bool: AWAIT goes on at once (no task, or one that ended)
  AwaitValue,     // Item -> Item: what AWAIT gives once the task ended: its result (its failure raised)
  AwaitWait,      // Item -> Item: AWAIT outside an ASYNC function (the start procedure): waits, then as await.value
  AwaitFuse,      // [callee]: an ASYNC function's entry (none: a send's method): right before AWAIT's call of it, asks
                  // its first step for its result itself when it ends without suspending (no task made)
  AwaitFused,     // -> Bool: right after that call: its value is that result (else a task), the request ended
  // A function's body inlined in another (passes/Inline.cpp) shows on the call stack as its own call would:
  // inline.enter makes a frame without slots (that of `blockFn`, its line the inlined code's) the innermost of the
  // context, inline.leave makes its caller's again. Between them, the inlined code's values live in the caller's frame.
  // inline.enter's operands, when `blockFn` reads the arguments it received, are those arguments (the frame's argv).
  InlineEnter,
  InlineLeave,
  // Right after `x := NIL` to a local or a parameter (the program lets go of what it held): sets to NIL the frame slots
  // of the values dead there that no live value holds, so that what only they held can be collected (a dead value's
  // slot otherwise keeps it until another value takes the slot).
  FrameRelease,
  // terminators
  Br,             // target(args)
  CondBr,         // Bool; then(args), else(args)
  GenYield,       // var, index: Item; resume: the element, the frame saved in the enumerator; the function returns .T.,
                  // and state `index` goes on at the target
  AsyncSuspend,   // var, index: Item; resume: the task awaited, the frame saved in the task; the step returns, and
                  // point `index` goes on at the target once the awaited task ended
  AsyncYield,     // var, index: Item; resume: an asynchronous generator's YIELD of the element, the frame saved in the
                  // task; the step returns, and point `index` goes on at the target at the next MoveNextAsync()
  Ret,            // [Item]
};

// Operations on memory variables, by name; those that may create a PRIVATE of
// the activation running (a macro may assign a name no variable has).
inline bool isMemvarOp(Op op) { return op >= Op::MvGet && op <= Op::MvParam; }
inline bool isMacroOp(Op op) { return op >= Op::MacroGet && op <= Op::MacroText; }
inline bool createsPrivates(Op op) {
  return op == Op::MvSet || op == Op::MvPrivate || op == Op::MvParam ||
         (isMacroOp(op) && op != Op::MacroPublic && op != Op::MacroText);
}

// What an operation does besides computing its result from its operands, for
// the passes that move, merge or remove operations (one row per operation in
// Ops.cpp). "Its variable" is `var`, and for `block` the captured variables.
enum Effect : uint16_t {
  Fails       = 1u << 0,  // may raise an error: runs the error block (xBase code), then either the function
                          // returns with a request to unwind or the operation goes on with a substituted result
  FailsByRef  = 1u << 1,  // fails only when its access goes through a reference (an element may be gone)
  ReadsVar    = 1u << 2,  // reads its variable
  WritesVar   = 1u << 3,  // changes its variable, or its own item (moving it into a cell)
  ReadsHeap   = 1u << 4,  // reads items of heap objects (elements, cells)
  WritesHeap  = 1u << 5,  // changes items of heap objects (so variables reached through them)
  ReadsState  = 1u << 6,  // depends on the context's settings (SET DECIMALS, SET EXACT)
  RunsCode    = 1u << 7,  // may run xBase code: reads and writes anything
  Allocates   = 1u << 8,  // may allocate: a safepoint of the collector
  Terminator  = 1u << 9,
  Commutative = 1u << 10, // the two operands may be swapped
  WritesState = 1u << 11, // changes the context's state (the pending requests, the BREAK value)
  Substitutes = 1u << 12, // may fail with an error whose block substitutes the result: the code goes on after running
                          // xBase code (other errors unwind, or end the program)
};

struct OpInfo {
  Op op;
  const char *name;
  uint16_t effects;
};

const OpInfo &opInfo(Op op);
inline const char *opName(Op op) { return opInfo(op).name; }
inline bool isTerminator(Op op) { return (opInfo(op).effects & Terminator) != 0; }
// A resumable function's step ends there; the next one goes on at its target (Resumable.cpp).
inline bool isSuspension(Op op) { return op == Op::GenYield || op == Op::AsyncSuspend || op == Op::AsyncYield; }

} // namespace xb::xir
