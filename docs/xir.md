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
  machine-typed block parameters into LLVM PHIs; while items live in frame slots (phase 1), an `item` parameter is a
  slot that each branch writes its argument into.
- Values are typed. `item` is a dynamic value of unknown type; type inference narrows values to `int` (64-bit),
  `double`, `logical`, `date`, `nil`, `string`, `array`, `block`, `object<Class>` and `ref`. A narrowed value is
  unboxed (an `int` lives in a register, not in an item).
- Two levels in the same IR, lowered progressively:
  - high-level operations with xBase semantics: `add`, `concat`, `compare`, `index`, `call`, `send`, `await`,
    `yield`, `seq_enter`, ...;
  - low-level operations: type tests, unboxed arithmetic, `alloc`, field access, `write_barrier`, `safepoint`.
- Where a type is not proven, lowering emits a type test with a specialized fast path and the general path (a
  runtime call). Compilation is ahead of time, so there is no deoptimization.
- A verifier checks every function after each pass; a text form exists only for debugging and tests
  (`--dump-xir`). The pipeline never reads text.

## Memory and the GC

- `alloc`, `write_barrier` and `safepoint` are instructions, so the optimizer reasons about them: a barrier is
  removed when the stored-into object was allocated with no safepoint since; allocations that do not escape are
  replaced by their fields or placed in a region freed as a whole.
- Variables become SSA values; only captured variables and variables passed by reference live in cells, and frames
  keep only values that must stay in memory.
- Object pointers live across safepoints become LLVM statepoints with relocation (`gc.statepoint`, `gc.relocate`),
  so the collector may move objects (evacuation in an Immix heap). Objects handed to C are pinned.

## Control flow for errors and exceptions

Every operation that can fail has an optional unwind successor (like LLVM's `invoke`). `BEGIN SEQUENCE` / `RECOVER`,
`BREAK`, `TRY` / `CATCH` / `FINALLY`, and the restoring of `FOR EACH` variables on unwinding are edges in the graph.
The runtime's pending flag ([runtime.md](runtime.md#errors-and-pending-requests)) is one way to lower those edges.

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

Planned. Today the syntax tree is folded (`opt/Fold`, [compiler.md](compiler.md#constant-folding)) and the lowering
elides copies (`FramePlan`); xIR has the analyses the passes build on (reverse postorder, dominators, the verifier).
A pass manager runs the passes in order, with the verifier after each one and a dump option per pass.

The passes need infrastructure xIR does not have yet:

- Mutation: replacing all uses of a value, erasing and moving instructions, splitting blocks, cloning blocks and
  functions (inlining, unrolling, unswitching).
- An effect table, one row per operation (data, not switches spread over passes): what it reads and writes (a
  variable, an element, any heap object), whether it may fail, run user code (calls, codeblocks) or allocate (a
  safepoint), whether it is pure or commutative. `mayFail` and the writes the frame plan tracks become lookups in it.
- Attributes of external functions in their declarations (pure, no allocation, reads its arguments only), next to
  `FOLD`, so calls such as `Len` can be moved, merged and removed.
- Cached analyses with invalidation: natural loops and their nesting, post-dominators and dominance frontiers
  (mem2reg), liveness.
- Types on values (the lattice type inference computes) and branch weights (error paths are cold before LLVM sees
  them).

They come in layers, each pass with its own tests:

- Cleanup: CFG simplification (merging blocks, threading jumps, removing empty and unreachable blocks), dead code
  elimination, copy propagation, sparse conditional constant propagation, value numbering (CSE/GVN).
- Memory: mem2reg (variables that are not shared become SSA values), load elimination and store-to-load forwarding,
  dead store elimination, write barrier elision (a store into an object allocated with no safepoint since).
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

`compiler/src/xir/`: the IR (`IR.h`: functions, blocks, values, instructions, variables), `Builder`, `Verifier`,
`Printer`, control flow analyses (`Analysis`: reverse postorder, dominators, removal of unreachable blocks), and the
lowering from the syntax tree (`Gen*.cpp`). `compiler/src/codegen/` lowers xIR to LLVM (`FramePlan` assigns the
frame). Optimization passes on xIR come with phase 2.

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
- Every operation that can fail has an optional unwind successor, where control goes when the operation leaves a
  pending error or request; without one, the function returns. Phase 1 never sets it.
- Terminators: `br`, `cond_br` (with block arguments), `ret`.
- Lowering to LLVM gives every live `item` value a frame slot (a GC root): liveness is computed per value by walking
  from its uses back to its definition, and slots are assigned by linear scan, so slots are reused once their values
  die. The arguments of a call and the elements of an array literal form a group of consecutive slots; a value used
  only there (a block parameter too, such as the result of `IIF`) is computed straight into its slot of the group. A
  `load` whose single use follows with nothing in between that can write the variable uses the variable's own item
  instead of a copy; a value used only by the `store` that follows it, or by `ret`, is computed in place.

## Phases

1. The core, lowering from the syntax tree and to LLVM, reproducing the previous behavior (done).
2. Native frames: item values in LLVM values, with the collector finding them through LLVM stack maps
   (statepoints) instead of frame slots; direct calls with fixed arity.
3. The pass manager and the passes (see [Passes](#passes)), starting with mem2reg, type inference and inline fast
   paths.
4. Relocation through statepoints, evacuation in the collector, barrier elision.
5. Unwind edges and `BEGIN SEQUENCE`; resumable functions (async/await, yield); classes.
