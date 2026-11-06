#pragma once

#include <cstdint>

namespace xb::xir {

// The operations of xIR and what each one does (one row per operation in
// Ops.cpp): the catalog passes consult instead of switching on operations.
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
  Send,           // message; object, args... -> Item       [may fail]
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
  SwitchCheck,    // value -> Item: the value the CASEs match [may fail]
  SwitchMatch,    // value, case constant -> Bool
  ParamsRef,      // parameters (captures) -> Bool: one of them holds a reference its caller passed
  CallForward,    // callee -> Item: calls it with this function's own arguments, as received [may fail]
  SeqCatch,       // -> Bool: in an unwind block of BEGIN SEQUENCE, the request is a BREAK alone (then taken)
  SeqValue,       // -> Item: the BREAK value (RECOVER USING), moved out of the context
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
};

struct OpInfo {
  Op op;
  const char *name;
  uint16_t effects;
};

const OpInfo &opInfo(Op op);
inline const char *opName(Op op) { return opInfo(op).name; }
inline bool isTerminator(Op op) { return (opInfo(op).effects & Terminator) != 0; }

} // namespace xb::xir
