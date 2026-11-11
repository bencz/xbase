# xIR

**Status: phase 1 is implemented** (`compiler/src/xir/`, lowered to LLVM by `compiler/src/codegen/`, see
[compiler.md](compiler.md#code-generation)); the later phases are planned. This is the design the implementation
follows.

xIR is xbase's mid-level intermediate representation, between the syntax tree and LLVM. It exists so that the
optimizations that need xBase semantics (types of dynamic values, Clipper's operator rules, the GC, exceptions,
coroutines, classes) run before lowering, where LLVM can no longer see them.

## Form

- Functions of basic blocks in SSA form. Blocks take parameters, and branches pass arguments to them (the
  equivalent of PHI nodes, easier to transform: splitting blocks, inlining, building state machines). Lowering turns
  machine-typed block parameters into LLVM PHIs; while items live in frame slots (until phase 4), an `item` parameter
  is a slot that each branch writes its argument into.
- Values are typed. `item` is a dynamic value of unknown type; type inference narrows values to `int` (64-bit),
  `double`, `logical`, `date`, `nil`, `string`, `array`, `block`, `object<Class>` and `ref`. A narrowed value is
  unboxed (an `int` lives in a register, not in an item).
- Two levels in the same IR, lowered progressively:
  - high-level operations with xBase semantics: `add`, `concat`, `compare`, `index`, `call`, `send`, `await`,
    `yield`, `seq_enter`, ...;
  - low-level operations: type tests, unboxed arithmetic, `alloc`, field access, `write_barrier`, `safepoint`.
- Where a type is not proven, lowering emits a type test with a specialized fast path and the general path (a
  runtime call). Compilation is ahead of time, so there is no deoptimization.
- A verifier checks every function after it is generated and after each pass that changed it: block shape, operand
  types, branch arguments, dominance, and that use lists match the uses. A text form exists only for debugging and
  tests (`--dump-xir`). The pipeline never reads text.

## Memory and the GC

- `alloc`, `write_barrier` and `safepoint` are instructions, so the optimizer reasons about them: a barrier is
  removed when the stored-into object was allocated with no safepoint since; allocations that do not escape are
  replaced by their fields or placed in a region freed as a whole.
- Variables become SSA values; only captured variables and variables passed by reference live in cells, and frames
  keep only values that must stay in memory.
- Object pointers live across safepoints become LLVM statepoints with relocation (`gc.statepoint`, `gc.relocate`),
  so the collector may move objects (evacuation in an Immix heap). Objects handed to C are pinned.

## Control flow for errors and exceptions

An operation that fails runs the error block ([runtime.md](runtime.md#errors-and-pending-requests)); then either it
goes on with a value the block substituted, or a request to unwind (a `BREAK`, a `QUIT`) is pending. Every block has
an optional unwind block (`BasicBlock::unwind`, printed `unwind <block>` after its label): where its operations go
with a request pending, like LLVM's `invoke` for every operation of the block at once; without one, the function
returns. The unwind block is the block's last successor in the control flow graph (`numSuccessors`, `successor`), so
reachability, dominance and liveness follow it; it has no parameters, since no branch passes values along it. The
edge leaves from the operation that failed, so the block's own instructions do not reach the unwind block or the
blocks it dominates (the verifier checks it).
`simplify-cfg` neither skips nor merges into an unwind block, and merges two blocks only when they unwind to the same
one.

- `BEGIN SEQUENCE`: the body's blocks unwind to a handler, `seq.catch` (`xb_seq_catch`: the request is a `BREAK`
  alone, now taken) then `cond_br` to `RECOVER`, which starts with `seq.value` (the `BREAK` value, stored in the
  `USING` variable), or onward. A function with a sequence keeps its variables in memory (`promote-variables` leaves
  it alone), so the handler reads them as the failed operation left them.
- `FOR EACH`: inside a sequence of the function, or when a loop variable may be seen by others (a `STATIC`, a shared
  variable, a parameter), the loop's blocks unwind to a cleanup block that restores the variables (`each.leave`).
- Onward: an unwind block that has done its work branches to the enclosing one, or returns with the request pending.

`TRY` / `CATCH` / `FINALLY` and `ALWAYS` are planned on the same edges.

## Resumable functions

`await` and `yield` are suspension points. One xIR pass turns a function that contains them into a state machine
whose frame is a GC object with a precise layout. It serves `ASYNC` functions and enumerators (`FOR EACH` over a
function that yields). xIR does this itself instead of using LLVM's coroutine intrinsics, so that coroutine frames
stay visible to the collector and compatible with statepoints.

## Classes

`send`, `get_field` and `set_field` carry the class when it is known: calls are devirtualized through class hierarchy
analysis, polymorphic sites get inline caches, and multiple inheritance uses subobject layouts with `self`
adjustment, as C++ compilers do.

## Types

`xir/Types.*` infers, for each item value of a function, the set of types it may hold (`TypeSet`, one bit per type tag):
an optimistic fixpoint over the SSA form in reverse postorder, where block parameters start empty and grow with the
values their branches pass, `binary` follows its operands with the runtime operators' rules (a table, `binaryResult`:
two integers give an integer or, on overflow, a double; a double with a number gives a double; two strings a string;
dates and numbers a date; the bit operators on numbers an integer; ...), `unary` (`~` gives an integer), `from_bool`,
literals and the constructors give their type, and anything read from memory (a variable, an element, a call's result)
may hold any type. The lowering uses it to keep values that never refer to a heap object out of the frame
([compiler.md](compiler.md#xir-to-llvm)), and where a value has a single possible type, the lowering uses it as a
constant instead of reading it. The error block may substitute a value of any type for the result of an operation that
fails ([Control flow for errors and exceptions](#control-flow-for-errors-and-exceptions)), so an operation that may fail
may produce any type: `binaryMayFail` tells when operands can make a `binary` fail (an operand type pair it does not
take, a divisor that may be zero: not a non-zero constant), `unary` and `switch.check` fail for types they do not take.
An operation that `subst.check` follows gives only its own types (see `typed-continuations` in [Passes](#passes)).
`Substitution::None` types every operation as if no error block substituted anything.

## Passes

The syntax tree is folded first (`opt/Fold`, [compiler.md](compiler.md#constant-folding)). From `-O1` on, the pass
manager (`PassManager.*`, `optimize`) runs a table of passes on every function, in order, repeating the pipeline
while something changes (at most four rounds), and verifies the function after every pass that changed it; invalid
xIR is an internal error naming the pass. `--dump-xir` writes the result. Implemented (`xir/passes/`):

- `append-in-place`, before `promote-variables`: `store x (binary add (load x) y)`, with the load and the add in the
  store's block, used only there, and no other write of `x` between the load and the store, becomes `append x, y` when
  `x` is a `LOCAL` only the function sees (`directLocals` in `Analysis`) and holds strings (a string constant is stored
  into it or appended to it, and no number, logical or date constant is). The variable then stays in the frame, where
  `xb_op_append` grows its string in place; a numeric accumulator, which promotion keeps in registers, is left alone.
- `promote-variables` (mem2reg): `LOCAL`s reached only by direct `load` and `store` (not shared, not parameters, not
  `FOR EACH` variables) become SSA values, except in a function with `BEGIN SEQUENCE`. Block parameters go where
  definitions meet, in the iterated dominance frontiers of the blocks that store (the entry defines the variable as
  `NIL`); renaming along the dominator tree replaces each `load` with the current value and drops the `store`s;
  parameters that only feed each other through a loop are removed (their arguments first, then them). The variables
  leave the frame.
- `fold-constants`: `i64` arithmetic and comparisons, `and` and `not` of booleans, `from_bool` and `truth` of
  constants (`truth` of a logical just made is its boolean), `cond_br` on a constant becomes `br`. A folded
  instruction is removed at once, also one that can fail in general (`truth`) but not with these operands.
- `simplify-cfg`: a block holding only `br` whose parameters nothing else uses is skipped by its predecessors; two
  branches to one block differing only in a `true`/`false` argument become a jump passing the condition; a block
  whose single predecessor jumps to it joins it; unreachable blocks go. The steps repeat until nothing changes.
- `remove-dead-code`: instructions whose result is unused and that cannot fail and write nothing (the effects in
  `Ops.*`), and unused block parameters with their arguments, by a worklist.
- `typed-continuations`, once after the others (then `simplify-cfg` and `remove-dead-code` again): an operation in a
  loop whose error may substitute its result, whose own types hold no heap reference (numbers, logicals, dates) or
  are a single type (a string concatenation: a constant tag), and that another operation computes with (an index
  too) or a branch passes on, ends its block with `subst.check` (its result is not of its own types: only a
  substituted value can be) and a `cond_br` to a block that takes the value with `subst.value` and enters a generic
  copy of the function right after the copy of the operation. The copy is made once per function, of at most 500
  instructions (a larger one doubles code that LLVM unrolls and splits again), with at most 16 checks, those of the
  most nested loops (`loopDepths` in `Analysis`); not in a function with unwind blocks, which take no parameters. The
  values that reach the copy are joined to its own by block parameters, built on demand from the uses (Braun et
  al.), and parameters that receive one value only are removed. The copy's operations keep giving any type, so a
  substituted value of any type runs correctly, while the original code keeps registers and constant type tags. The
  lowering reads the item's real tag for `subst.check` and weights its branch unlikely; between the operation and
  `subst.value` the result may sit in a home the collector does not see, which holds while the pending test after
  the operation (`xb_pending`) never collects (one mutator).

They use the mutation API of `IR.h` (replacing all uses of a value, redirecting a branch, removing a block parameter
with its arguments, taking a terminator; `Builder::jump` replaces a terminator with a jump) and the predecessor
terminators of every block (`Analysis.h`), computed once per round. Constants compare by value (`sameValue`):
each use of a literal may be its own constant. The lowering then elides copies (`FramePlan`).

Still to build for the passes that follow:

- Mutation: moving instructions, splitting blocks, cloning blocks and functions (inlining, unrolling,
  unswitching) as shared utilities; `typed-continuations` splits blocks and copies a function on its own for now.
- Attributes of external functions in their declarations (pure, no allocation, reads its arguments only), next to
  `FOLD`, so calls such as `Len` can be moved, merged and removed.
- Cached analyses with invalidation: natural loops and their nesting, post-dominators and dominance frontiers
  (mem2reg), liveness.
- Types on operations: the type sets ([Types](#types)) are computed for the lowering only; passes need them on the
  values, refined by the operations that test them, and branch weights (error paths are cold before LLVM sees them).

Planned, in layers, each pass with its own tests:

- Cleanup: copy propagation, sparse conditional constant propagation, value numbering (CSE/GVN).
- Memory: load elimination and store-to-load forwarding for the variables that stay in memory, dead store
  elimination, write barrier elision (a store into an object allocated with no safepoint since).
- Types: type inference over SSA (the xBase counterpart of known bits: integer, double, string, array, logical,
  date, `NIL`), inline fast paths where a type is proven or likely, unboxed numbers.
- Loops: loop-invariant code motion, unswitching of type tests, strength reduction, bounds-check elimination for
  loops over arrays (1-based), unrolling and runtime unrolling when trip counts are known.
- Calls: inlining of xBase functions and of more runtime intrinsics (`AScan`, `At`, ...; `Len`, `Asc`, `Chr` and
  one-byte `SubStr` are done), tail recursion elimination, call ordering for argument evaluation.
- Objects: escape analysis with scalar replacement of arrays and codeblocks that do not escape, regions.

LLVM then does instruction selection, register allocation and its own vectorization.

## Code

`compiler/src/xir/`: the IR (`IR.h`: functions, blocks, values, instructions, variables), the catalog of operations
(`Ops.*`: one row per operation with its name and effects: whether it may fail, reads or writes its variable or the
heap, depends on SET values, runs xBase code, allocates, ends a block, is commutative, may go on after its error's block
substituted the result, changes the context's state; `Instruction::effects` adds those of reading references to a list
with `...`, which may fail and run the error block; `mayFail`, the passes and the frame plan consult it), `Builder`,
`Verifier`, `Printer`, control flow analyses (`Analysis`: reverse postorder, dominators and the dominator tree,
dominance frontiers, loops and their depths, the predecessor terminators of every block, removal of unreachable blocks),
and the lowering from the syntax tree (`Gen*.cpp`). Block names are unique in a function (`while.body.2`).
`PassManager.*` and `passes/` optimize it (see [Passes](#passes)). `compiler/src/codegen/` lowers xIR to LLVM
(`FramePlan` assigns the frame).

## Phase 1 in detail

Phase 1 replaced the code generator without changing behavior or speed; the later phases optimize on top of it.

- Types: `item` (an xBase value), `bool`, `i64` (counters), `args` (a list of items whose length is known at run
  time: `...`), `void`. Phase 3 adds the unboxed numeric types.
- Values: instruction results, block parameters, and constants (`nil`, logicals, numbers with their display width and
  decimals, strings), which lower to static items. A number whose decimals are SET DECIMALS at run time is an
  instruction, since it needs the context.
- Variables are memory places of the function: frame variables (parameters and `LOCAL`s; a shared one moves into a
  cell when captured or passed by reference), captured variables of a codeblock, and the module's `STATIC`s. `load`
  reads one into a value, `store` writes one, and `barrier` follows a store into a variable that may live in the
  heap. A `FOR EACH` variable is accessed through its reference; a `STATIC`, a shared or captured variable or a
  parameter is tested for a cell or reference at run time. `ref` and `ref.index` make the reference an argument `@x`
  or `@a[i]` passes.
- Operations mirror the runtime: unary and binary operators (`++`, `--` and the step of a `FOR` without `STEP` are a
  `binary add` or `sub` of 1 marked `byOne`, printed `add.one`: the same fast paths, and an error that is theirs,
  `xb_op_inc`/`xb_op_dec`), memory variables by name (`mv.get`, `mv.set`, `mv.ref`, `mv.private`, `mv.public`,
  `mv.param`; read and written as the heap is, since any call may create, change or end one), the macro operator
  (`macro.get`, `macro.set`, `macro.call`, `macro.private`, `macro.public`, the text an item operand, and
  `macro.text`, the substitution in a string literal; a macro may do anything a call does, and create a PRIVATE of the
  function, which `createsPrivates` tells codegen), the truth test, calls
  (to the module's functions or to
  declared externals), `send` (a message to the first operand, with the others as arguments), array literals (`...` in a
  call, a send or an array literal is its last operand, of type `args`, `args.rest`: the function's arguments past its
  parameters, made right before its list, which is its one use in the block; `restRefs` the positions of a call that
  receive references), hash literals (`hash`, keys and values alternating), element read and write, codeblock creation
  with the cells of its captured variables, the `FOR` test, the `FOR EACH` operations (`each.in_cell`, `each.enter`,
  `each.move`, `each.leave`, and `each.key`, the key at a position), the `SWITCH` check (whose result is the value the
  `CASE`s match: a substituted one after an error) and match, `seq.catch` and `seq.value` for `BEGIN SEQUENCE`, `append`
  (`var := var + value`, `xb_op_append`), `subst.check` and `subst.value` for typed continuations (the verifier keeps
  the check right after its operation and the value first in the block only the check leads to), and for parameter
  versioning `params.ref` (whether parameters hold references their caller passed) and `call.forward` (a call with the
  function's own arguments, as received).
- An operation that fails, or leaves another pending request that asks to unwind, goes to its block's unwind block,
  or makes the function return (see [Control flow for errors and exceptions](#control-flow-for-errors-and-exceptions)).
- Terminators: `br`, `cond_br` (with block arguments), `ret`.
- Lowering to LLVM gives every live `item` value that may refer to a heap object a frame slot (a GC root; others are
  `Local`, see [compiler.md](compiler.md#xir-to-llvm)): liveness is computed per value by walking from its uses back to
  its definition, and slots are assigned by linear scan, so slots are reused once their values die. The operands of a
  call or a `send` and the elements of an array or hash literal form a group of consecutive slots; a value used only
  there (a block parameter too, such as the result of `IIF`) is computed straight into its slot of the group. A `load`
  whose single use follows with nothing in between that can write the variable uses the variable's own item instead of a
  copy, and so does any load of a frame variable nothing writes; a value used only by the `store` that follows it, or by
  `ret`, is computed in place.

## Phases

1. The core, lowering from the syntax tree and to LLVM, reproducing the previous behavior (done).
2. Native frames: item frames on the machine stack, linked per context (done; each frame is the anchor the stack
   maps of phase 4 are relative to). Calls within a module pass the parameters' items in registers to a direct entry
   ([compiler.md](compiler.md#xir-to-llvm)).
3. The pass manager and the passes (see [Passes](#passes)): the manager, mem2reg and the cleanup passes are done,
   and type inference ([Types](#types)) keeps values without heap references in registers and removes the type
   tests of the lowering's fast paths (`codegen/FastPaths.cpp`) for values with a single possible type; next value
   numbering and typed operations.
4. Statepoints: once mem2reg makes items SSA values, LLVM keeps them in registers and records where the live
   object pointers are at each call (stack maps, relative to the frame's anchor), instead of frame slots holding
   every item; relocation, evacuation in the collector, barrier elision.
5. Unwind edges and `BEGIN SEQUENCE` (done); resumable functions (async/await, yield); classes.
