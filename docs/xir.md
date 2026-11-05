# xIR (planned)

**Status: planned.** Nothing in this document exists yet; today the code generator lowers the syntax tree straight
to LLVM ([compiler.md](compiler.md#code-generation)). This is the design the implementation follows.

xIR is xbase's mid-level intermediate representation, between the syntax tree and LLVM. It exists so that the
optimizations that need xBase semantics (types of dynamic values, Clipper's operator rules, the GC, exceptions,
coroutines, classes) run before lowering, where LLVM can no longer see them.

## Form

- Functions of basic blocks in SSA form. Blocks take parameters, and branches pass arguments to them (the
  equivalent of PHI nodes, easier to transform: splitting blocks, inlining, building state machines). Lowering turns
  block parameters into LLVM PHIs.
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
- Variables become SSA values; only captured variables live in cells, and frames keep only values that must stay in
  memory.
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

Type inference; sparse conditional constant propagation; value numbering; dead code elimination; inlining of xBase
functions and of runtime intrinsics (`Len`, `SubStr`, `AScan`, ...); loop-invariant code motion and unswitching of
type tests; bounds-check elimination for loops over arrays (1-based); escape analysis with scalar replacement and
regions; barrier elision; in-place append for `s := s + x` on a string nothing else references. LLVM then does
register allocation, vectorization and unrolling.

## Code

`compiler/src/xir/`: the IR (functions, blocks, values, instructions), builder, verifier and printer; lowering from
the syntax tree; passes; lowering to LLVM, which replaces the current code generator.

## Phases

1. The core, lowering from the syntax tree and to LLVM, reproducing today's behavior with every test passing.
2. mem2reg, type inference and inline fast paths.
3. Statepoints with relocation, evacuation in the collector, barrier elision.
4. Unwind edges and `BEGIN SEQUENCE`; resumable functions (async/await, yield); classes.
