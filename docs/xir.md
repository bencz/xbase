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

Planned (phase 5): every operation that can fail gets an optional unwind successor (like LLVM's `invoke`), an edge
of the control flow graph that the analyses and passes follow. `BEGIN SEQUENCE` / `RECOVER`, `BREAK`, `TRY` /
`CATCH` / `FINALLY`, and the restoring of `FOR EACH` variables on unwinding become such edges. The runtime's pending
flag ([runtime.md](runtime.md#errors-and-pending-requests)) is one way to lower them. Until then a failing operation
makes its function return, with the error pending.

## Resumable functions

`await` and `yield` are suspension points. One xIR pass turns a function that contains them into a state machine
whose frame is a GC object with a precise layout. It serves `ASYNC` functions and enumerators (`FOR EACH` over a
function that yields). xIR does this itself instead of using LLVM's coroutine intrinsics, so that coroutine frames
stay visible to the collector and compatible with statepoints.

## Classes

`send`, `get_field` and `set_field` carry the class when it is known: calls are devirtualized through class hierarchy
analysis, polymorphic sites get inline caches, and multiple inheritance uses subobject layouts with `self`
adjustment, as C++ compilers do.

## Passes

The syntax tree is folded first (`opt/Fold`, [compiler.md](compiler.md#constant-folding)). From `-O1` on, the pass
manager (`PassManager.*`, `optimize`) runs a table of passes on every function, in order, repeating the pipeline
while something changes (at most four rounds), and verifies the function after every pass that changed it; invalid
xIR is an internal error naming the pass. `--dump-xir` writes the result. Implemented (`xir/passes/`):

- `promote-variables` (mem2reg): `LOCAL`s reached only by direct `load` and `store` (not shared, not parameters,
  not `FOR EACH` variables) become SSA values. Block parameters go where definitions meet, in the iterated dominance
  frontiers of the blocks that store (the entry defines the variable as `NIL`); renaming along the dominator tree
  replaces each `load` with the current value and drops the `store`s; parameters that only feed each other through a
  loop are removed (their arguments first, then them). The variables leave the frame.
- `fold-constants`: `i64` arithmetic and comparisons, `and` and `not` of booleans, `from_bool` and `truth` of
  constants (`truth` of a logical just made is its boolean), `cond_br` on a constant becomes `br`. A folded
  instruction is removed at once, also one that can fail in general (`truth`) but not with these operands.
- `simplify-cfg`: a block holding only `br` whose parameters nothing else uses is skipped by its predecessors; two
  branches to one block differing only in a `true`/`false` argument become a jump passing the condition; a block
  whose single predecessor jumps to it joins it; unreachable blocks go. The steps repeat until nothing changes.
- `remove-dead-code`: instructions whose result is unused and that cannot fail and write nothing (the effects in
  `Ops.*`), and unused block parameters with their arguments, by a worklist.

They use the mutation API of `IR.h` (replacing all uses of a value, redirecting a branch, removing a block parameter
with its arguments, taking a terminator; `Builder::jump` replaces a terminator with a jump) and the predecessor
terminators of every block (`Analysis.h`), computed once per round. Constants compare by value (`sameValue`):
each use of a literal may be its own constant. The lowering then elides copies (`FramePlan`).

Still to build for the passes that follow:

- Mutation: moving instructions, splitting blocks, cloning blocks and functions (inlining, unrolling,
  unswitching).
- Attributes of external functions in their declarations (pure, no allocation, reads its arguments only), next to
  `FOLD`, so calls such as `Len` can be moved, merged and removed.
- Cached analyses with invalidation: natural loops and their nesting, post-dominators and dominance frontiers
  (mem2reg), liveness.
- Types on values (the lattice type inference computes) and branch weights (error paths are cold before LLVM sees
  them).

Planned, in layers, each pass with its own tests:

- Cleanup: copy propagation, sparse conditional constant propagation, value numbering (CSE/GVN).
- Memory: load elimination and store-to-load forwarding for the variables that stay in memory, dead store
  elimination, write barrier elision (a store into an object allocated with no safepoint since).
- Types: type inference over SSA (the xBase counterpart of known bits: integer, double, string, array, logical,
  date, `NIL`), inline fast paths where a type is proven or likely, unboxed numbers.
- Loops: loop-invariant code motion, unswitching of type tests, strength reduction, bounds-check elimination for
  loops over arrays (1-based), unrolling and runtime unrolling when trip counts are known.
- Calls: inlining of xBase functions and of runtime intrinsics (`Len`, `SubStr`, `AScan`, ...), tail recursion
  elimination, call ordering for argument evaluation.
- Objects: escape analysis with scalar replacement of arrays and codeblocks that do not escape, regions, in-place
  append for `s := s + x` on a string nothing else references.

LLVM then does instruction selection, register allocation and its own vectorization.

## Code

`compiler/src/xir/`: the IR (`IR.h`: functions, blocks, values, instructions, variables), the catalog of operations
(`Ops.*`: one row per operation with its name and effects: whether it may fail, reads or writes its variable or the
heap, depends on SET values, runs xBase code, allocates, ends a block, is commutative; `mayFail` and the copy elision
of the frame plan consult it), `Builder`, `Verifier`, `Printer`, control flow analyses (`Analysis`: reverse
postorder, dominators and the dominator tree, dominance frontiers, the predecessor terminators of every block,
removal of unreachable blocks), and the lowering
from the syntax tree (`Gen*.cpp`). Block names are unique in a function (`while.body.2`).
`PassManager.*` and `passes/` optimize it (see [Passes](#passes)). `compiler/src/codegen/` lowers xIR to LLVM
(`FramePlan` assigns the frame).

## Phase 1 in detail

Phase 1 replaced the code generator without changing behavior or speed; the later phases optimize on top of it.

- Types: `item` (an xBase value), `bool`, `i64` (counters), `void`. Phase 3 adds the unboxed numeric types.
- Values: instruction results, block parameters, and constants (`nil`, logicals, numbers with their display width and
  decimals, strings), which lower to static items. A number whose decimals are SET DECIMALS at run time is an
  instruction, since it needs the context.
- Variables are memory places of the function: frame variables (parameters and `LOCAL`s; a shared one moves into a
  cell when captured or passed by reference), captured variables of a codeblock, and the module's `STATIC`s. `load`
  reads one into a value, `store` writes one, and `barrier` follows a store into a variable that may live in the
  heap. A `FOR EACH` variable is accessed through its reference; a `STATIC`, a shared or captured variable or a
  parameter is tested for a cell or reference at run time. `ref` and `ref.index` make the reference an argument `@x`
  or `@a[i]` passes.
- Operations mirror the runtime: unary and binary operators, the truth test, calls (to the module's functions or to
  declared externals), array literals, element read and write, codeblock creation with the cells of its captured
  variables, the `FOR` test, the `FOR EACH` operations (`each.in_cell`, `each.enter`, `each.move`, `each.leave`), the
  `SWITCH` check and match.
- An operation that fails, or leaves another pending request that asks to unwind, makes the function return (unwind
  successors are planned, see [Control flow for errors and exceptions](#control-flow-for-errors-and-exceptions)).
- Terminators: `br`, `cond_br` (with block arguments), `ret`.
- Lowering to LLVM gives every live `item` value a frame slot (a GC root): liveness is computed per value by walking
  from its uses back to its definition, and slots are assigned by linear scan, so slots are reused once their values
  die. The arguments of a call and the elements of an array literal form a group of consecutive slots; a value used
  only there (a block parameter too, such as the result of `IIF`) is computed straight into its slot of the group. A
  `load` whose single use follows with nothing in between that can write the variable uses the variable's own item
  instead of a copy; a value used only by the `store` that follows it, or by `ret`, is computed in place.

## Phases

1. The core, lowering from the syntax tree and to LLVM, reproducing the previous behavior (done).
2. Native frames: item frames on the machine stack, linked per context (done; each frame is the anchor the stack
   maps of phase 4 are relative to). Direct calls with fixed arity (planned).
3. The pass manager and the passes (see [Passes](#passes)): the manager, mem2reg and the cleanup passes are done;
   next value numbering and type inference, so the integer fast paths of the lowering (`codegen/FastPaths.cpp`)
   drop their type tests where types are proven.
4. Statepoints: once mem2reg makes items SSA values, LLVM keeps them in registers and records where the live
   object pointers are at each call (stack maps, relative to the frame's anchor), instead of frame slots holding
   every item; relocation, evacuation in the collector, barrier elision.
5. Unwind edges and `BEGIN SEQUENCE`; resumable functions (async/await, yield); classes.
