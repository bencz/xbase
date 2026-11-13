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
- The collector moves objects (evacuation in the Immix heap) and pins those that the machine stack and registers
  point into, so compiled code keeps object pointers in registers across safepoints without telling it where
  ([runtime.md](runtime.md#garbage-collector)). *Planned:* LLVM statepoints (`gc.statepoint`, `gc.relocate`) for
  pointers held in registers, so that objects they reach can move too, and frames need not keep every item in
  memory.

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

- `WITH`: `seq.with` makes the handler the error block and gives the old one, which `seq.restore` puts back at the
  end of the body and first thing in the handler.
- `ALWAYS` (`TRY`'s `FINALLY`): the code runs where the body and `RECOVER` join, and in an unwind block of its own,
  where `RECOVER` unwinds and the handler goes on (without `RECOVER`, every request goes on): `seq.suspend` sets the
  requests aside, `seq.value` takes the `BREAK` value, the code runs (its blocks unwind to the block of `seq.resume`,
  which follows it too), `seq.resume` puts them back unless the code made one of its own (a `QUIT` set aside stays
  then), and unwinding goes on. A function with `seq.suspend` keeps its variables in memory too.

## Resumable functions

`yield` (and later `await`) is a suspension point. A generator's body becomes a resume function that runs one step
per call (see [compiler.md](compiler.md#syntax-tree-to-xir)): its frame is saved into the enumerator, a GC object, at
`gen.yield` and comes back at `gen.restore`, which dispatches on the state to the block after that `YIELD`. Values the
registers would lose across a `YIELD` move into frame variables (`makeResumable`). xIR does this itself instead of
using LLVM's coroutine intrinsics, so that suspended frames stay visible to the collector and compatible with
statepoints. Planned: `ASYNC` functions on the same machinery.

## Classes

Now: a `send` from a method (or a codeblock it makes) names the method's class (`classIndex`, printed `from n`; `super
n` for `Super:message`), which decides what the message reaches at run time; `class.new` makes an object of a class of
the module (`Module::classes`). Each `send` is lowered with a polymorphic inline cache of its own, which the runtime
fills ([runtime.md](runtime.md#classes)). Planned: `send`, `get_field` and `set_field` carry the receiver's class when
it is known: calls are devirtualized through class hierarchy analysis, and variables are read from known offsets.

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
while something changes (at most four rounds), each function after those it calls, makes or forwards to (post-order of
the call graph: a body is inlined optimized, its own calls inlined already), then the final passes on every function
(so that a body inlined elsewhere is copied before them), and verifies the function after every pass that changed
it; invalid xIR is an internal error naming the pass. `--dump-xir` writes the result. Implemented (`xir/passes/`):

- Inlining (`Inline.cpp`), first: a call runs its callee's body in place, between `inline.enter` (which names the
  callee: the call stack shows it, as its call would) and `inline.leave`; each `ret` becomes `inline.leave` and a
  branch passing the result to the rest of the caller's block. The callee's parameters and `LOCAL`s become frame
  variables of the function, stored directly on every entry (the arguments, `NIL` past the last and for the `LOCAL`s)
  and set to `NIL` before each `inline.leave` (as its frame would be gone: what they held is no longer kept): a call
  starts with fresh variables, and a shared one gets a fresh cell when a block captures it, the store having replaced
  the cell an earlier entry left. A codeblock's captures are the variables it captured; the module's `STATIC`s stay.
  When an argument is a reference (`@x`, `@a[i]`), the variables of its parameters are marked `param` (they keep
  reading and writing through what they hold), and so do the callee's variables marked so already (the parameters of
  a body inlined in it before). A callee that reads the arguments it received (`readsArgs`: `PCount()`) gets them as
  `inline.enter`'s operands, the inlined frame's arguments, until no call of an `ARGS` function in its body is left
  (`Callee::readsCallerArgs`; a branch folded away may take the last one): then they are no longer passed. A body in
  which nothing can see the call stack any more (no call, nothing that can fail or run code, no loop, whose back edge
  polls and may run destructors, no body inlined in it: what folding leaves of `Mul( 2, 3 )`) needs no frame: its
  `inline.enter` and `inline.leave` go (`trimInlinedBodies`, at the start of `inline-functions`; a leave several
  bodies reach keeps them all). A body is inlined only at a call without `...` in a block that unwinds to the
  function's exit, and only when the callee reads no parameters in its frame slots (`readsParams`: `PValue()`; the
  inlined frame has none), creates no `PRIVATE` (nor calls a function that creates its caller's: it would be the
  function's, released later), and has no unwind block, no `...` and no `YIELD`; nothing is inlined into a generator,
  nor into a function that has grown past 3000 instructions. The body is copied with `copyBlocks`, its constants made
  anew in the function.
  - `inline-functions`: a call of a function of the module, or of a public function of a carried runtime source
    (`Module::library`, below), not a method, an `INIT` or `EXIT` procedure, a generator, nor one that calls itself, of
    at most 30 instructions, or 200 when the call passes a codeblock literal (which the body may then run in place
    too). A function versioned for references runs, for arguments without a reference, the body after its
    `params.ref` test (which they pass); otherwise its other body (`NAME.byref`).
  - `inline-blocks`, after it (which may pass a block literal to an `Eval`): `Eval( {| p | body }, args )` of a
    codeblock literal of the function (`Eval` of the native runtime function), of at most 200 instructions, not the
    function itself (a block a call inlined in it runs), none of whose captured variables an inlined body binds anew
    (a direct store of a shared variable: the block holds the cell of when it was made). The block is immutable and
    its body reaches its cells through the same variables, so other uses may keep it; it is no longer made once none
    is left. Afterwards, `unshare-variables` runs.

  The driver compiles the runtime's carried sources (`driver/RuntimeLibrary.h`: the `.prg` files `XB_INLINE_PRG` in
  `compiler/CMakeLists.txt` lists, held in `xbc` as bytes with their public functions' names) as modules of the
  runtime, through the pipeline's rounds only, once a module calls one of their functions (at `-O1` and above, not
  with `--runtime`, and only when `std.ch` is found in the include directories: else nothing of them is inlined);
  each module gets them as `Module::library`. A body from one is inlined only when it needs nothing of its module
  (`selfContained`: no `STATIC` of it, none of its classes, no function of it called or codeblock made; its own
  `STATIC` functions and codeblocks run in place in it already): it calls the runtime only. Their functions are
  marked `runtime` (`Function::runtime`): their frames show no source file and no lines, as the runtime's do.

  The verifier checks that `inline.enter` and `inline.leave` nest (`openInlineBodies` in `Analysis`: the functions of
  the bodies open where each block starts, the same on every path into it, also through the copies
  `typed-continuations` makes).
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
  constants (`truth` of a logical just made is its boolean), `+`, `-`, `*`, the bit operators, unary `-` and `~`, and
  the comparisons of two number constants, as the run-time operators compute them from two values (`foldNumbers`,
  `foldNumber` and `compareNumbers` of `opt/Fold`, with `runtime`: an integer result is a computed one, of width 0; a
  double's decimals are the operator's; `/`, `%`, `^` and an integer product that overflows, whose decimals are SET
  DECIMALS at run time, stay), `=`, `==` and `!=` of two string constants when SET
  EXACT cannot change the result (`stringEquality` in `opt/Fold`, which the syntax tree's folding uses too),
  `ValType()` of a value whose types all have one letter (`XB_TYPE_LETTERS`; not a string, which may be a memo field's
  value: an array, codeblock or hash literal, a number, a logical), and `cond_br` on a constant becomes `br`. A folded
  instruction is removed at once, also one that can fail in general (`truth`) but not with these operands.
- `simplify-cfg`: a block holding only `br` whose parameters nothing else uses is skipped by its predecessors; two
  branches to one block differing only in a `true`/`false` argument become a jump passing the condition; a block
  whose single predecessor jumps to it joins it; unreachable blocks go. The steps repeat until nothing changes.
- `remove-dead-code`: instructions whose result is unused and that cannot fail and write nothing (the effects in
  `Ops.*`), and unused block parameters with their arguments, by a worklist; then the frame variables (not
  parameters) no instruction reaches any more (those of inlined bodies that promote-variables took).
- `unshare-variables`, after it: a frame variable (not a parameter) that nothing captures or passes by reference any
  more (the block that did is gone: inlined, or dead code) loses its cell (`shared`); its accesses become direct and
  their barriers go, so `promote-variables` takes it in the next round.
- `typed-continuations`, once after the others (then `simplify-cfg` and `remove-dead-code` again): an operation in a
  loop whose error may substitute its result, whose own types hold no heap reference (numbers, logicals, dates) or are a
  single type (a string concatenation: a constant tag), and that another operation computes with (an index too) or a
  branch passes on, ends its block with `subst.check` (its result is not of its own types: only a substituted value can
  be) and a `cond_br` to a block that takes the value with `subst.value` and enters a generic copy of the function right
  after the copy of the operation. The copy is made once per function, of at most 500 instructions (a larger one doubles
  code that LLVM unrolls and splits again), with at most 16 checks, those of the most nested loops (`Loops` in
  `Analysis`); not in a function with unwind blocks, which take no parameters. The values that reach the copy are joined
  to its own by block parameters, built on demand from the uses (Braun et al.), and parameters that receive one value
  only are removed. The copy's operations keep giving any type, so a substituted value of any type runs correctly, while
  the original code keeps registers and constant type tags. The lowering reads the item's real tag for `subst.check` and
  weights its branch unlikely; between the operation and `subst.value` the result may sit in a home the collector does
  not see, which holds because the pending test after such an operation calls `xb_pending`, which never collects nor
  parks the thread for another thread's collection (the request waits for the next poll), and not `xb_pending_gc`.

Shared changes of shape are in `Transform.*`: `splitAfter` (the rest of a block into a new one it jumps to),
`copyBlocks` (blocks copied into their function or another, with the map from each original block and value to its
copy; a value from outside the copied blocks stays, or is what a given function makes of it), `SSAUpdater` (SSA form
rebuilt for a value that gets more definitions, given at the end of blocks: the value reaching a block, through block
parameters made on demand where definitions meet, as in Braun et al.) and `removeTrivialParams` (a parameter made so
whose arguments are one value is that value). `typed-continuations` and the inlining passes use them.

They use the mutation API of `IR.h` (replacing all uses of a value, redirecting a branch, removing a block parameter
with its arguments, taking a terminator; `Builder::jump` replaces a terminator with a jump) and the predecessor
terminators of every block (`Analysis.h`), computed once per round. Constants compare by value (`sameValue`):
each use of a literal may be its own constant. The lowering then elides copies (`FramePlan`).

Still to build for the passes that follow:

- Mutation: moving instructions, cloning whole functions (inlining, specialization), on top of `Transform.*`.
- Attributes of external functions in their declarations (pure, no allocation, reads its arguments only), next to
  `FOLD`, so calls such as `Len` can be moved, merged and removed.
- Cached analyses with invalidation (the CFG, loops, dominance frontiers, types: each pass computes its own now);
  post-dominators, liveness.
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
dominance frontiers, natural loops (`Loops`: each loop's header, blocks, parent and depth, outermost first, and the
innermost loop of each block), the predecessor terminators of every block, removal of unreachable blocks), shared
changes of shape (`Transform`), and the lowering from the syntax tree (`Gen*.cpp`). Block names are unique in a function
(`while.body.2`). `PassManager.*` and `passes/` optimize it (see [Passes](#passes)). `compiler/src/codegen/` lowers xIR
to LLVM (`FramePlan` assigns the frame).

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
  `mv.param`; read and written as the heap is, since any call may create, change or end one; `mv.field` reads an
  undeclared name, a field of the current work area first), fields of work areas (`field.get` and `field.set`, by name,
  with the alias's name in the instruction or its item as the first operand; they read the context's state and may run
  code: an error block), the macro operator (`macro.get`, `macro.set`, `macro.call`, `macro.private`, `macro.public`,
  the text an item operand, and `macro.text`, the substitution in a string literal; a macro may do anything a call does,
  and create a PRIVATE of the function, which `createsPrivates` tells codegen), the truth test, calls (to the module's
  functions or to declared externals), `send` (a message to the first operand, with the others as arguments), array
  literals (`...` in a call, a send or an array literal is its last operand, of type `args`, `args.rest`: the function's
  arguments past its parameters, made right before its list, which is its one use in the block; `restRefs` the positions
  of a call that receive references), hash literals (`hash`, keys and values alternating), element read and write,
  codeblock creation with the cells of its captured variables, the `FOR` test, the `FOR EACH` operations (`each.source`,
  `each.in_cell`, `each.enter`, `each.move`, `each.leave`, and `each.key`, the key at a position), the `SWITCH` check
  (whose result is the value the `CASE`s match: a substituted one after an error) and match, `seq.catch` and `seq.value`
  for `BEGIN SEQUENCE`, the generator operations (`gen.new`, `gen.restore`, `gen.done`, the terminator `gen.yield`, and
  `box.i64`, `box.bool`, `unbox.i64`, `unbox.bool` for the values kept in the frame), `append` (`var := var + value`,
  `xb_op_append`), `subst.check` and `subst.value` for typed continuations (the verifier keeps the check right after its
  operation and the value first in the block only the check leads to), and for parameter versioning `params.ref`
  (whether parameters hold references their caller passed) and `call.forward` (a call with the function's own arguments,
  as received), and `inline.enter` (naming a function or block function) and `inline.leave`, around an inlined body:
  the call stack shows that function in between.
- An operation that fails, or leaves another pending request that asks to unwind, goes to its block's unwind block,
  or makes the function return (see [Control flow for errors and exceptions](#control-flow-for-errors-and-exceptions)).
- Terminators: `br`, `cond_br` (with block arguments), `gen.yield`, `ret`.
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
   every item; relocation of what registers point to (the collector evacuates already, pinning those objects),
   barrier elision.
5. Unwind edges and `BEGIN SEQUENCE` (done); resumable functions (async/await, yield); classes.
