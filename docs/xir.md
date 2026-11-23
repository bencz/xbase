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

`yield` and `await` are suspension points. A generator's body becomes a resume function that runs one step
per call (see [compiler.md](compiler.md#syntax-tree-to-xir)): its frame is saved into the enumerator, a GC object, at
`gen.yield` and comes back at `gen.restore`, which dispatches on the state to the block after that `YIELD`. Values the
registers would lose across a `YIELD` move into frame variables (`makeResumable`). xIR does this itself instead of
using LLVM's coroutine intrinsics, so that suspended frames stay visible to the collector and compatible with
statepoints. `ASYNC` functions use the same machinery: their frame goes into their task at `async.suspend` and comes
back at `async.restore` (`isSuspension` names both terminators for the passes).

## Classes

Now: a `send` from a method (or a codeblock it makes) names the method's class (`classIndex`, printed `from n`; `super
n` for `Super:message`), which decides what the message reaches at run time; `class.new` makes an object of a class of
the module (`Module::classes`). Each `send` is lowered with a polymorphic inline cache of its own, which the runtime
fills ([runtime.md](runtime.md#classes)). `object.get` and `object.set` (an instance variable's slot in `index`) read
and assign a variable of an object whose class is known, which `resolve-sends` makes of sends (Passes).

`ClassModel` (`ClassModel.*`) sees the module's classes as the runtime does once they are prepared: it describes each
as an `xb_class_def` and applies the runtime's own rules (`xbclass.h`, `runtime/src/types/members.c`, compiled into
`xbc` too): a class's line, what a message reaches (also from a method of a class, and `Super:`), the scope a sender
needs, an instance variable's slot. A class whose line reaches a class found by name when the program runs (another
module's, or one not declared) has no known line. `ClassInfo` gives the class of the values of a function that hold
objects of one such class: what `class.new` makes, what a `New` without a method of the class's own returns (Init
runs, the object comes back), a parameter of a copy typed for it (`Function::paramClasses`), a block parameter all of
whose arguments hold objects of one class.

## Types

`xir/Types.*` infers, for each item value of a function, the set of types it may hold (`TypeSet`, one bit per type tag):
an optimistic fixpoint over the SSA form in reverse postorder, where block parameters start empty and grow with the
values their branches pass, `binary` follows its operands with the runtime operators' rules (a table, `binaryResult`:
two integers give an integer or, on overflow, a double; a double with a number gives a double; two strings a string;
dates and numbers a date; the bit operators on numbers an integer; ...; `=`, `==` and `!=` with an object on the
left, any type: its class may define the operator, whose method gives the result even where the operands make no
error, as in `o == NIL`; other operators on an object fail without one, which counts),
`unary` (`~` gives an integer), `from_bool`,
literals and the constructors give their type, `table.index` its table's elements' unless the table is broken (any type,
as a substitution; fold-constant-statics below), a parameter of a typed copy the types its calls pass
(`Function::paramTypes`, `specialize-calls` below), a call of a runtime function declared with `RESULT` the types it
returns (those of its arguments for a `CHOOSES` one, which returns one of them; its `INTEGER RANGE` the range of its
integers where no error substitutes the value: a `TOTAL` call of arguments of its types; a value a check lets through
may be any integer) or, when it fails, the value an error block substitutes (any type, unless `subst.check` follows;
none for a `TOTAL` one whose arguments are of its parameters' types), a call of a function of the module (or
`call.forward`) gives the types its `RETURN`s give (`Function::returnTypes`, below), and anything else read from memory
(a variable, an element, another call's result) may hold any type. The lowering uses it to keep values that never refer
to a heap object out of the frame ([compiler.md](compiler.md#xir-to-llvm)), where a value has a single possible type,
uses it as a constant instead of reading it, and emits only the fast paths the operands' types allow.

The return types of a function (`returnTypes`): the union of the types of the values its `ret`s give (`NIL` for a
`ret` without one), with the function's own type inference; every type for a generator's resume function. The pass
manager sets `Function::returnTypes` once it has optimized the function (every type until then), so its callers,
optimized after it, use them; a function of a call cycle entered before it gives every type there. The typed copies'
are settled together at the end (`refineCopies`, `specialize-calls` below). The function's own
errors cannot substitute the call's result (an error inside it substitutes the failed operation's, and the type
inference of its `ret`s counts that), and a call it never enters (the stack's end) unwinds.

The error block may substitute a value of any type for the result of an operation that fails ([Control flow for
errors and exceptions](#control-flow-for-errors-and-exceptions)), so an operation that may fail may produce any type:
`binaryMayFail` tells when operands can make a `binary` fail (an operand type pair it does not take, a divisor that may
be zero: not a non-zero constant), `unary` and `switch.check` fail for types they do not take. An operation that
`subst.check` follows gives only its own types, and a value a guard checks (`subst.check` with a mask) only the types
of the mask (see `typed-continuations` in [Passes](#passes)).
`Substitution::None` types every operation as if no error block substituted anything.

Integer ranges (`IntRange`, the values an item holds when it is an integer) grow with the types in the same optimistic
fixpoint, since each needs the other: `+`, `-` and `*` of two integers give an integer, not a double, when the operands'
ranges keep the result within int64 (`TypeInfo::fits`, the corners computed with LLVM's checked arithmetic), and `Int( x
/ k )` of an integer `x` (`k` an integer constant other than -1, 0 and 1, `intQuotientDividend`) is an integer (its
range exact within 2^53, where the double quotient is; past it the quotient and its truncation move by less than 2^-51
of it and one more, a slack the range takes). `BitAnd` with a non-negative constant is within it, a negation negates the
range and `Int()` of an integer keeps it, and a value an error may substitute has any range, checked or not (a
`subst.check` lets a substituted value of the operation's own types through). A block parameter's range is widened once
it has grown twice: on the side that grows, to the next of the function's integer constants, their neighbours (c - 1,
c + 1) or the end of int64 (thresholds), so that the fixpoint ends (and the hull of two empty ranges is the one empty
range, however a bound wrote them: `1..0` for a FOR that never runs); two rounds of narrowing then compute every range
again from the others, and the types once more with the ranges fixed. The tests bound the values they compare (`Bound`):
a FOR's true branch bounds its counter by its limit (no greater; no less with a negative step) when both and the step
are integers and the step's sign is known, and either branch of a comparison `<` `<=` `>` `>=` of two numbers bounds
each operand by the other when that is an integer (the false branch by the negation). Such a bound holds in the blocks
its branch dominates when the branch's edge is the only way into its block from outside it (its other predecessors are
back edges), at most 16 of each value (the first in reverse postorder: a value compared in thousands of places would
make each look-up as long), and for the arguments that branch passes to a block parameter in any case (a loop's back
edge from an `IF n > 3; EXIT`). `--dump-xir-types` prints the types after each item value and block parameter.

## Passes

The syntax tree is folded first (`opt/Fold`, [compiler.md](compiler.md#constant-folding)). From `-O1` on, the pass
manager (`PassManager.*`, `optimize`) runs a table of passes on every function, in order, repeating the pipeline while
something changes (at most eight rounds: a chain of `Enumerable`'s methods takes one a level), each function after those
it calls, makes or forwards to (post-order of the call graph: a body is inlined optimized, its own calls inlined
already), then the final passes on every function (so that a body inlined elsewhere is copied before them), and verifies
the function after every pass that changed it; invalid xIR is an internal error naming the pass. `--dump-xir` writes the
result. Implemented (`xir/passes/`):

- `drop-reference-bodies` (`Versioning.cpp`), once on the module before the others: a `STATIC FUNCTION` (not a method,
  not `INIT` or `EXIT`) is reached only from the module (no macro, no other module, no message reaches it); when it
  has no function reference (`func.ref`, which may be called with anything), and none of its calls passes a reference
  (`@x`, `@a[i]`, `@name`) or `...` (whose arguments may be references) or forwards its arguments (`call.forward`),
  its parameters never hold a reference: its `params.ref` test becomes false, the
  branch to its other body goes, and that body (`NAME.byref`), which nothing else calls, leaves the module with the
  codeblocks it makes; the module's functions are verified after it.
- `fold-constant-statics` (`FoldConstants.cpp`), once on the module after it: a `STATIC` that its initializer alone
  writes, with a constant (not passed by reference nor a `FOR EACH` variable, so not `shared`, and stored nowhere else
  in the module: no function, method or codeblock writes it), holds that constant wherever code reads it, since every
  module's `STATIC`s are initialized, with constants, before any code of the program runs (`xb_program_run`); each of
  its loads becomes the constant, with its literal's width and decimals (`whetstone`'s `t`, `t1`, `t2`). An initializer
  whose decimals are SET DECIMALS' is an instruction, not a constant, and stays a load. A `STATIC` that its initializer
  alone writes, with an array literal of constants (NIL, logicals, numbers; no `...`), and whose array no code changes
  or lets out (no instruction of the module names it but loads whose every use is an element read `s[i]` or `Len( s )`;
  a codeblock of the module reads it the same way) is a table (`Var::table`, its elements the literal's constants in
  `Var::items`, listed as `table $sN:` under the module): each element read becomes `table.index $sN, i`, which reads
  the elements as constant data (`bitlookup`'s nibble table, `md5_sha1`'s constants). The array stays in the `STATIC`,
  where an index out of bounds or no number finds it; that error gives its handler the array (its arguments), which code
  may then change, so the table breaks there for good (its flag, compiler.md) and every read is the array's from then
  on: `table.index` gives its elements' types and range only as `Substitution::None` sees them, and any type as a
  substitution would (a check narrows it), and `Len()` of it stays a call.

- Inlining (`Inline.cpp`), first of the passes on each function: a call runs its callee's body in place, between
  `inline.enter` (which names the callee: the call stack shows it, as its call would) and `inline.leave`; each `ret`
  becomes `inline.leave` and a branch passing the result to the rest of the caller's block. The callee's parameters and
  `LOCAL`s become frame variables of the function, stored directly on every entry (the arguments, `NIL` past the last
  and for the `LOCAL`s) and set to `NIL` before each `inline.leave` (as its frame would be gone: what they held is no
  longer kept): a call starts with fresh variables, and a shared one gets a fresh cell when a block captures it, the
  store having replaced the cell an earlier entry left. A codeblock's captures are the variables it captured; the
  module's `STATIC`s stay. When an argument is a reference (`@x`, `@a[i]`), the variables of its parameters are marked
  `param` (they keep reading and writing through what they hold), and so do the callee's variables marked so already
  (the parameters of a body inlined in it before). The arguments are `inline.enter`'s operands, which hold them while
  the body runs, as the callee's parameters would until it returns (a `frame.release` of a parameter's `LOCAL` in the
  body lets go of its argument there unless the caller still uses it; a value whose hold nothing can see, a constant or
  a codeblock literal, which folding may leave unmade, is passed as `NIL`: `heldUnseen` in `Transform`), and the inlined
  frame's arguments for a callee that reads them as received (`readsArgs`: `PCount()`), all of them until no call of an
  `ARGS` function in its body is left (`Callee::readsCallerArgs`). A body in which nothing can see the call stack any
  more (no call, nothing that can fail or run code, no loop, whose back edge polls and may run destructors, no body
  inlined in it: what folding leaves of `Mul( 2, 3 )`) needs no frame: its `inline.enter` and `inline.leave` go
  (`trimInlinedBodies`, at the start of `inline-functions`; a leave several bodies reach keeps them all). A body is
  inlined only at a call without `...`, and only when the callee reads no parameters in its frame slots (`readsParams`:
  `PValue()`; the inlined frame has none), creates no `PRIVATE` (nor calls a function that creates its caller's: it
  would be the function's, released later), and has no unwind block, no `...` and no `YIELD`; nothing is inlined into a
  generator, nor into a function that has grown past 3000 instructions. The body is copied with `copyBlocks`, its
  constants made anew in the function. At a call in a `BEGIN SEQUENCE` (or a `FOR EACH` with a cleanup), the body's
  blocks unwind to a block that leaves its frame (`inline.leave`) and goes on to the handler, as the call would return
  with the error (`unwindBody` in `Transform`: one such block for each set of bodies a copied block ends with open, the
  callee's own inlined bodies first); the block that enters the body unwinds there too and the one that leaves it to the
  handler, so that each unwind block starts with the bodies the end of every block reaching it has open. -
  `inline-functions`: a call of a function of the module, or of a public function of a carried runtime source
  (`Module::library`, below; a method too, which `resolve-sends` made a call of), not an `INIT` or `EXIT` procedure, a
  generator, nor one that calls itself, of at most 30 instructions, or 200 when the call passes a codeblock literal
  (which the body may then run in place too). A function versioned for references runs, for arguments without a
  reference, the body after its `params.ref` test (which they pass); otherwise its other body (`NAME.byref`). -
  `fuse-generators`, after it (which shows the enumerator a generator's function makes: `gen.new`): a `FOR EACH` over an
  enumerator made in the function by `gen.new` of a generator of the module, used only by that loop's `each.source` (and
  held by the `inline.enter` of a body it was passed to that does not read its arguments as received: given `NIL` once
  fused) (ascending, one collection, no key), whose source goes only to its `each.enter`, `enum.len` and `each.move`,
  entered as often as its enumerator is made (every loop around its `each.enter` goes round its `gen.new` too: an
  enumerator made once and gone through again goes on where it was), whose variable is the frame's alone (not shared,
  not a parameter) with no loop on it inside this one, and whose generator's body (the resume function's blocks a step
  runs, of at most 300 instructions) has no unwind block, keeps no handler across `YIELD`, reads no parameter in its
  frame and creates no `PRIVATE`, runs the generator in place (`FuseGenerators.cpp`). The body is copied between
  `inline.enter` and `inline.leave` of the resume function (the call stack shows it as its steps would), its variables
  new ones of the function (its parameters given the arguments, the rest NIL: `bodyVars`, `enterBody` in `Transform`,
  which inlining uses too), started where a first step would, the caller's line shown the loop's; each `gen.yield`
  assigns the loop variable and enters the loop's body, the body's way back goes on after that `YIELD` (the state, when
  there are several, in a variable of its own), and `gen.done` leaves the loop, its variables set to NIL as the end
  drops its frame (`leaveBody`). In a sequence, the body unwinds through a block that leaves its frame (`unwindBody`),
  and each way out to the loop is a block that unwinds where the loop does. The variable is then an ordinary one in the
  loop (over an enumerator, assigning it changes nothing else: its reads and writes become direct), saved before and
  restored after it. The values the loop's header carried are joined again through the generator's blocks (`SSAUpdater`;
  an edge that passes a parameter back to itself defines nothing new). A generator fed by another (`FOR EACH x IN Evens(
  Range( 1, n ) )`) fuses in turn once `promote-variables` shows the inner enumerator's `gen.new` to the copied loop.
  The `generator` benchmark runs in 2 ms (59 through the enumerators). - `inline-blocks`, after it (which may pass a
  block literal to an `Eval`): `Eval( {| p | body }, args )` of a codeblock literal of the function (`Eval` of the
  native runtime function), of at most 200 instructions, not the function itself (a block a call inlined in it runs),
  none of whose captured variables is bound anew by an inlined body (a direct store of a shared variable: the block
  holds the cell of when it was made) or by a `FOR EACH` (a block made while the variable holds a loop's reference holds
  a cell of its own with the element of then). The block is immutable and its body reaches its cells through the same
  variables, so other uses may keep it; a value it captured (a value cell) is a new variable of the function, given the
  value the block was made with at the inlined entry (shared when a block the body makes captures it on). It is no
  longer made once none is left. Afterwards, `unshare-variables` runs.

  The driver compiles the runtime's carried sources (`driver/RuntimeLibrary.h`: the `.prg` files `XB_INLINE_PRG` in
  `compiler/CMakeLists.txt` lists, held in `xbc` as bytes with their public functions' names and the messages their
  classes declare: `arrays.prg` and `collections.prg`, with `Enumerable` and the classes of the built-in types) as
  modules of the runtime, through the pipeline's rounds only, once a module calls one of their functions or sends one of
  those messages (at `-O1` and above, not with `--runtime`, and only when `std.ch` is found in the include directories:
  else nothing of them is inlined); each module gets them as `Module::library`. Their functions are `carried`
  (`Function::carried`): compiled in the runtime, which exports them by their library symbols
  (`XB_LIBRARY_FUNC_PREFIX`), a program reaches them there, through their xBase-callable entry (`hasDirectEntry` is
  false for them). A body from one is inlined, and a generator of one fused, only when it needs nothing of its module
  but its functions (`selfContained`: no `STATIC` of it, none of its classes, no codeblock of it made, no typed copy of
  it called, which the runtime need not have made alike). Their functions are marked `runtime` (`Function::runtime`):
  their frames show no source file and no lines, as the runtime's do.

  The verifier checks that `inline.enter` and `inline.leave` nest (`openInlineBodies` in `Analysis`: the functions of
  the bodies open where each block starts, the same on every path into it, also through the copies
  `typed-continuations` makes).
- `append-in-place`, before `promote-variables`: `store x (binary add (load x) y)`, with the add in the store's block,
  the load in a block that dominates it (an inlined body or a branch may lie between: `c += F( x )`), both used only
  there, and no write of `x` on any way from the load to the store, becomes `append x, y` when `x` is a `LOCAL` only the
  function sees (`directLocals` in `Analysis`) and holds strings (a string constant is stored into it or appended to it,
  and no number, logical or date constant is). The variable then stays in the frame, where `xb_op_append` grows its
  string in place; a numeric accumulator, which promotion keeps in registers, is left alone. After the typed
  continuations, `split-appends` makes `append x, a + b + ... + z` whose pieces the type inference finds all strings
  (their checks passed) one `append` per piece, in order: no string is made for the sum (a `+` of two strings fails only
  past the longest string, where the piece's append then fails instead).
- `promote-variables` (mem2reg): `LOCAL`s reached only by direct `load` and `store` (not shared, not parameters, not
  `FOR EACH` variables) become SSA values, except in a function with `BEGIN SEQUENCE`. Block parameters go where
  definitions meet, in the iterated dominance frontiers of the blocks that store (the entry defines the variable as
  `NIL`); renaming along the dominator tree replaces each `load` with the current value and drops the `store`s;
  parameters that only feed each other through a loop are removed (their arguments first, then them). The variables
  leave the frame.
- `fold-constants`: `i64` arithmetic and comparisons, `and` and `not` of booleans, `unbox.i64` and `unbox.bool` of what
  `box.i64` and `box.bool` made or of a constant, `from_bool` and `truth` of constants (`truth` of a logical just made
  is its boolean), `+`, `-`, `*`, the bit operators, unary `-` and `~`, and the comparisons of two number constants (and
  of two logicals, and `=`, `==`, `!=` with a `NIL` constant: `compareLogicals`, `compareNil`), as the run-time
  operators compute them from two values (`foldNumbers`, `foldNumber` and `compareNumbers` of `opt/Fold`, with
  `runtime`: an integer result is a computed one, of width 0; a double's decimals are the operator's; `/`, `%`, `^` and
  an integer product that overflows, whose decimals are SET DECIMALS at run time, stay), `+` and `-` of two string
  constants (`stringJoin`), `=`, `==` and `!=` of two string constants when SET EXACT cannot change the result, and
  their `$` (`stringTest`; both in `opt/Fold`, which the syntax tree's folding uses too), a call of a `FOLD` function on
  constants (`evaluateCall` of `opt/Fold`, without `literal`: the value the function returns at run time, an integer a
  computed one; [compiler.md](compiler.md#constant-folding)), `ValType()` of a value whose types all have one letter
  (`XB_TYPE_LETTERS`; not a string, which may be a memo field's value: an array, codeblock or hash literal, a number, a
  logical), an `index` at a constant position within an array literal (no `...`) whose every use reads an element of it
  at a position that is a number (one that is not raises an error whose handler receives the array, may change it and
  goes on) or holds it in an `inline.enter` of a body that does not read its arguments as received (`readOnlyArray`:
  nothing changes it or lets it out) is the element it was made with (`codeblock_eval`'s blocks, which `inline-blocks`
  then runs inline; `ray_tracer`'s vectors); such an array that inlined bodies only hold, whose elements' lifetimes
  nothing can see (`heldUnseen`, or values that refer to no heap object), is held as `NIL` instead, so that
  `remove-dead-code` takes it away (an element that may be an object stays held by its array until the bodies end, as a
  parameter holds its argument); and `cond_br` on a constant becomes `br`. A folded instruction is removed at once, also
  one that can fail in general (`truth`) but not with these operands.
- `simplify-cfg`: a block holding only `br` whose parameters nothing else uses is skipped by its predecessors; two
  branches to one block differing only in a `true`/`false` argument become a jump passing the condition; a block
  whose single predecessor jumps to it joins it; unreachable blocks go. The steps repeat until nothing changes.
- `remove-dead-code`: instructions whose result is unused and that cannot fail and write nothing (the effects in
  `Ops.*`; a `block` nothing uses too: the variables it would move into cells are reached through the cell test), and
  unused block parameters with their arguments, by a worklist; then the frame variables (not parameters) no instruction
  reaches any more (those of inlined bodies that promote-variables took).
- `unshare-variables`, after it: a frame variable (not a parameter) that nothing captures or passes by reference any
  more (the block that did is gone: inlined, or dead code) loses its cell (`shared`); its accesses become direct and
  their barriers go, so `promote-variables` takes it in the next round.
- `join-strings`, once after the others, before `typed-continuations` (whose checks would split the chains): `a + b +
  c + ...`, three pieces or more, each sum used only as the left operand of the next `+` (no `add.one` nor
  `add.grow`), whose pieces may all be strings (their types include it) and one of which is a string for sure, and
  whose sum no `append` appends (`split-appends` appends those pieces without a sum). Each `+` becomes a join in its
  own place, so the pieces are computed, and a failing `+` fails, in the order they were: the first `join.step` takes
  pieces 0 and 1, each later one the step before and pieces 0 to k, the last `+` is `join.end` of the step before and
  every piece (a piece that is the sum of another chain is that chain's `join.end`). A join holds at most
  `kMaxJoinPieces` (16) pieces: a longer chain is joined in parts, each part's sum the first piece of the next, so its
  operands stay linear in its length. `join.end`'s own types are those of the `+`s one after the other (a substituted
  sum is what its check finds). Code generation tests inline that the step before is the pending mark (the first: that
  piece 0 is a string) and that its piece is a string, then gives the mark (`XB_JOIN_PENDING`: no string made); else,
  and for `join.end`, it calls `xb_op_join` with the pieces' addresses (one array of the function, filled before each
  call), which joins the strings in one allocation or makes the `+` of the sum so far and the piece
  ([runtime.md](runtime.md#values)). A chain of seven short strings costs one allocation, not six (string_concatenation:
  half the time).
- `typed-continuations`, once after the others (then `simplify-cfg` and `remove-dead-code` again): an operation in a
  loop whose error may substitute its result, whose own types hold no heap reference (numbers, logicals, dates) or are a
  single type (a string concatenation: a constant tag), and that another operation computes with (an index too) or a
  branch passes on, ends its block with `subst.check` (its result is not of its own types: only a substituted value can
  be) and a `cond_br` to a block that takes the value with `subst.value` and enters a generic copy of the function right
  after the copy of the operation. The copy is made once per function, of at most 500 instructions (a larger one doubles
  code that LLVM unrolls and splits again), with at most 64 checks, those of the most nested loops (`Loops` in
  `Analysis`); not in a function with unwind blocks, which take no parameters. The checks are chosen first with the
  types as if every operation were checked (`Substitution::None`), then again, up to 4 more rounds while one adds any,
  with the types where only those chosen so far are (`TypeInfo`'s assumed checks): an operation not chosen may still
  give any type, and what takes it (a `TOTAL` call whose argument it is, an operator) may then fail too (`Asc( SubStr(
  c, i, 1 ) )`: `SubStr` is not computed with, so `Asc` is checked); a sum that a loop carries stays unchecked where its
  operands are. The values that reach the copy are joined to its own by block parameters, built on demand from the uses
  (Braun et al.), and parameters that receive one value only are removed. The copy's operations keep giving any type, so
  a substituted value of any type runs correctly, while the original code keeps registers and constant type tags. The
  lowering reads the item's real tag for `subst.check` and weights its branch unlikely; between the operation and
  `subst.value` the result may sit in a home the collector does not see, which holds because the pending test after such
  an operation calls `xb_pending`, which never collects nor parks the thread for another thread's collection (the
  request waits for the next poll), and not `xb_pending_gc`. The same pass guards the values that enter a loop with a
  type the code would not know there (with substitutions counted: a parameter, a variable, a call's result, an operation
  whose error may substitute it) when the loop takes them as numbers (a `FOR`'s start or limit, an operand of `-`, `*`,
  `/`, `%`, `^`, a comparison, a negation): `subst.check` with a mask (`imm`: the number types it gives itself, as an
  integer for `Max()` of integers, both when it gives none) right after the value, into the same copy otherwise, so the
  loop keeps its values typed and out of the frame. The guarded value itself keeps a home the collector sees
  (`TypeInfo::guardNeedsHome`) when until its check it may hold anything (a substituted error, a value read from memory:
  the pending test after a call may collect). It also speculates: in a loop, `+`, `-` and `*` of two values that are
  integers once the speculations hold (the greatest set of the loops' values that are integers, `+ - *` or `Int( x / k
  )` of such, or block parameters all of whose arguments are) and whose ranges may overflow get a guard against the
  integers alone (`subst.check` with the integer mask; an overflow's double goes on in the copy), so the loop computes
  with integers in registers; one whose ranges cannot overflow (`fits`) needs none.
- `resolve-sends`, after `fold-constants`: a `send` (without `...`, not one an AWAIT fuses) to an object whose class
  `ClassInfo` knows, and a `Super` send of a method to its own `Self` that reaches a method (its parameter 0, which only
  loads read, `onlyLoaded`: the runtime finds a `Super` member from the method's class alone, `Self` being of it or of a
  subclass, and a value that is no object whatever class its type has now; a variable's slot is the object's class's,
  which only the runtime knows), reaches what `ClassModel` finds for it from the send's class (`classIndex`) and
  `Super`: a method becomes a `call` of its function (which `inline-functions` may then inline: a method is inlined like
  a function, with its frame shown), an instance variable read an `object.get` (the arguments ignored, as the runtime
  does) and one assigned with one argument that is no reference an `object.set` of its slot. `New` that no member of the
  line answers nor names is what the runtime answers itself (`newByInit`): the class's `Init` sent from the class with
  the arguments, a `call` of that method when the class reaches it (its result unused), nothing when no member answers
  `Init`; the send's value is the object (`object_creation`: 0.35 of the cycles before). Anything else stays a send: a
  class variable, a cast, `New` whose `Init` is a variable or a member only a friend reaches, `Init` without a method, a
  message no member answers, a member whose scope the sender reaches only as a friend (the runtime checks friends), a
  line not known. The classes' methods are optimized before the other functions (`calleesFirst`), so that a call a send
  became inlines an optimized body. In the runtime's classes, `ARRAY:Count`'s `::Super:Count( b )` becomes a call of
  `Enumerable:Count`, so that `a:Count( {| v | ... } )` runs in place too (`resolve-scalar-sends`).
- `resolve-scalar-sends`, after `resolve-sends` (`ScalarSends.cpp`): a send to a value whose types (as the type
  inference knows them) include built-in types whose class of the runtime (a `SCALAR CLASS` of a carried source:
  `ARRAY`, `CHARACTER`, `HASH`, ... and `Enumerable` they inherit) answers it with one method (the most such types;
  arrays first) becomes a call of that method, and so does each next send of a chain to the enumerator the method
  before made (a generator: its messages are `Enumerable`'s), behind one test: `scalar.plain`, the value of one of
  those types (a mask) and no class of the program answering them before the runtime's (`xb_scalar_replaced`: a
  `SCALAR CLASS` of the program, `ClassAssociate()` while it runs). The chain's instructions (its sends, the arguments
  computed between them) are copied into the version that calls, the sends kept in the other (`dispatched`: not
  resolved again), and the values they make joined after them (`joinDefinitions`). A value `ClassInfo` knows is an
  object of a class of the module is left alone, and one that may be an object only for a chain or a send of a
  codeblock literal (`Enumerable`'s way of use). The rounds then inline the methods, fuse their generators
  (`fuse-generators`: an `inline.enter` of a body that only holds the enumerator it was passed gets `NIL` instead)
  and run the codeblocks in place: `a:Where( {| v | ... } ):Select( {| v | ... } ):Sum()` becomes one loop
  (`bench/primitives/iteration.prg`: 25 to 6 ns an element), and `s:ForEach( {| c, i | ... } )` a loop written in
  place.
- `speculate-sends`, after `resolve-scalar-sends` (`ResolveSends.cpp`): a `send` (without `...`, not `Super`, not one
  an AWAIT fuses, not kept by a version already: `dispatched`) to a value that may be an object whose class `ClassInfo`
  does not know (made by a macro, read from an element or a parameter), when the classes of the module that answer it
  (not `SCALAR` ones) all reach one method by `ClassModel`'s rules from the send's class, at most `kMaxSpeculated` of
  them (or one class reaches an instance variable, read or assigned with one value that is no reference), is versioned
  on the object's class: `object.is` of those classes (their "or": `not( and( not a, not b ) )`) right before it, and
  the copy that runs then holds what `resolve-sends` makes of it (a call, inlined in later rounds; an `object.get` or
  `object.set`), the other the send (`dispatched`), its value joined after (`joinDefinitions`). A class's members are
  fixed once it is prepared, so an object of one of those classes, exactly, reaches that member whatever made it; any
  other value (an object of another module's class, of a class whose line the compiler does not know, a value that is
  no object) gets what the runtime finds. A method's sends to its own `Self` are left alone (`readsOwnSelf`): its body
  stays small enough to be inlined where its class is known, and the sends of a body inlined where it is not are the
  caller's, which this versions. `method_call_overhead`: 180 to 47 ms (its sends to an object a macro made).
- `specialize-calls`, on each function after its pipeline (then the pipeline again when it changed it): a call of a
  function of the module (a `FUNCTION`, `PROCEDURE` or method with parameters, not of a carried runtime source (whose
  classes and `STATIC`s are its module's), not an `INIT` or `EXIT` procedure, a body versioned for references, one that
  reads its arguments as received (`PCount()`, `PValue()`, `...`), a generator, an `ASYNC` function, nor one that makes
  codeblocks or has unwind blocks; at most 2000 instructions, and past 300 only when a value its parameters give,
  through what is computed from it and the variables and block parameters it goes to, is used in a loop: elsewhere
  knowing their types pays little and the copy doubles the code generator's work, `paramsReachLoops`) that passes no
  reference nor `...` and whose arguments the type inference finds all integers, else all numbers, all logicals or all
  dates for some parameter (an argument with no type at all, the result of a call that never returns, takes the
  integers: no value of it reaches the call), or objects of one class `ClassInfo` knows, goes to a copy of the function
  typed for them (`NAME.typedN`, shown on the call stack as the function: `stackName`). A function has at most 4 copies
  of each kind (`kMaxCopies`: those that know every parameter, its types or its object's class, and those that leave
  some parameter of any type); past that, a call goes to the copy made already whose parameters take its arguments with
  the fewest types (the first made of those), else to the function itself. The copy's parameters have those types and
  classes (`Function::paramTypes`, `Function::paramClasses`; a parameter the body writes keeps any type and class; a
  method's copy, its Self of a known class, resolves the sends to Self), its versioning test is false (no call of it
  passes a reference) and its body versioned for references goes. The copy is made once per function and parameter
  types, optimized at once (its own calls with such arguments go to it, so a recursive function calls its copy), and its
  return types are the least fixed point of what its `RETURN`s give with its calls of itself giving the types found so
  far, from none (`settleReturnTypes`). `Fib( n - 1 ) + Fib( n - 2 )` then computes with numbers in registers.

  Settled one at a time, a copy that calls another copy sees its result of any type while that one is being optimized,
  and a result passed to a call, `Ack( m - 1, Ack( m, n - 1 ) )`, then goes to the function itself, whose result is of
  any type too. So once every function is done, `refineCopies` settles the copies' return types together: an optimistic
  fixed point from none for every copy at once. Each round moves every call of a function that has copies or may have
  them (in every function of the module, the copies too, each function again while one of its calls moves, so that a
  result's new types reach the calls it goes to) to the copy its arguments' types now ask for, made then while the
  function's budget allows (a copy of its body as optimized, not optimized again: its tidying after the versioning test
  sees the return types the code was optimized for), else to what it called before (its copy still takes its
  arguments: the code around the call may have lost their types since, as a body inlined in a function that keeps its
  variables in memory); then each copy's return types grow by what its `RETURN`s give. The result is kept when it
  settles within 32 rounds (`kMaxRefineRounds`) and no call then gives types beyond those of what it called before (the
  code around it, and in a copy made there the code it copies, was optimized for those); otherwise every call, return
  type and copy goes back as it was. Ackermann's function then runs on its copies typed for numbers: `ackermann` 19%
  fewer instructions, `tak` 19% (perf stat, x86-64). Last, a `STATIC FUNCTION` its copies replaced, which no root
  reaches any more, leaves the module with its body versioned for references, and so does a copy nothing reaches any
  more (`dropCopiedFunctions`; the module's first function, which may be the program's start, `firstProgramFunction`,
  stays).
- `peel-for-types`, after `typed-continuations`: a small innermost loop (at most 80 instructions, in a function of at
  most 600; up to 4 loops) without a suspension point or an inlined body's frame, one of whose header's parameters gets
  one number type from the loop's passes and another number type from outside (a sum that starts at the integer 0 and
  adds doubles), has its first pass copied in front of it (with the blocks its checks lead to): the copy's edges back
  to its header enter the loop, the loop's entries enter the copy, and the values of the loop used after it are joined
  with the copy's (`SSAUpdater`). The loop then carries that value with one type, which the lowering keeps in a
  register of its kind (a double in a floating-point register): the Basel sum's loop runs in about 8 cycles a term
  (`bench/async_pi.prg`), as .NET's does.
- `version-each`, after `peel-for-types`: a `FOR EACH` loop whose collection (`each.source`, used only from its
  `each.enter` on, which follows it: a loop of one collection) may be a string, an array or a hash among other types is
  versioned for them (`VersionEach.cpp`), innermost loops first (up to 8 loops in a function of at most 4000
  instructions, each loop of at most 250 with the blocks its checks lead to and the cleanup blocks only it unwinds to,
  which restore its variables): a guard on the collection (`subst.check` with the first type's mask) lets the original
  loop go on with that type, a guard on what goes on (`subst.value`) enters a copy for the next type, and so on, and the
  rest enters a copy for any collection. The values of the loop used after it, and the collection where the copies meet
  again (a typed continuation's copy of the loop that the checks lead to), are joined from the copies (`SSAUpdater`).
  The lowering then knows the collection's type in each copy: the length test, the step and the variable's reads are
  done in place without testing the loop reference's collection at every pass ([compiler.md](compiler.md)). A loop over
  a string runs in about 1 ns an element, over a hash in about 3 (`bench/primitives/iteration.prg`).

Shared changes of shape are in `Transform.*`: `splitAfter` (the rest of a block into a new one it jumps to),
`copyBlocks` (blocks copied into their function or another, with the map from each original block and value to its
copy; a value from outside the copied blocks stays, or is what a given function makes of it), `SSAUpdater` (SSA form
rebuilt for a value that gets more definitions, given at the end of blocks: the value reaching a block, through block
parameters made on demand where definitions meet, as in Braun et al.) and `removeTrivialParams` (a parameter made so
whose arguments are one value is that value), and `joinDefinitions` (the uses after copied blocks of each value they
define, given its definitions, take the one that reaches them). `typed-continuations`, `peel-for-types`,
`version-each` and the inlining passes use them.

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
- Calls: inlining of xBase functions and of more runtime intrinsics (`AScan`, `At`, ...; `Len`, `Asc`, `Chr`,
  one-byte `SubStr`, `Int`, `Abs`, `Max`, `Min`, `Round`, `Sqrt`, `Exp`, `Log` and `Mod` are done), tail recursion
  elimination, call ordering for argument evaluation.
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
- Variables are memory places of the function: frame variables (parameters and `LOCAL`s; a shared one moves into a cell
  when captured or passed by reference), captured variables of a codeblock, and the module's `STATIC`s. `load` reads one
  into a value, `store` writes one, and `barrier` follows a store into a variable that may live in the heap. A `FOR
  EACH` variable is accessed through its reference; a shared or captured variable, a parameter, and a `STATIC` passed by
  reference or made a `FOR EACH` variable somewhere in its module (Sema's `staticsInCell`: macros never see a `STATIC`,
  so its uses are all in the module's source) is tested for a cell or reference at run time; another `STATIC` is read
  and written directly (its storage still goes through the runtime once threads share it, `xb_storage_shared`, which
  also keeps it from being a forwarded store's place and gives an elided load of it a shadow slot the runtime may read
  it into). `ref` and `ref.index` make the reference an
  argument `@x` or `@a[i]` passes. `load ... move` is the load of a parameter into the `LOCAL` that stands for it
  (parameter versioning): nothing reads the parameter after; its slot keeps the value until the function returns or that
  `LOCAL` is set to `NIL`.
- Operations mirror the runtime: unary and binary operators (`++`, `--` and the step of a `FOR` without `STEP` are a
  `binary add` or `sub` of 1 marked `byOne`, printed `add.one`: the same fast paths, and an error that is theirs,
  `xb_op_inc`/`xb_op_dec`; the adds of `x += y`, and of `x := x + y + ...` written out on the same variable, element or
  instance variable, are marked `grows`, printed `add.grow`: the same fast paths, and `xb_op_add_grow`, whose long
  string results are views that later appends grow in place, see [runtime.md](runtime.md#values); `join.step` and
  `join.end` are the `+`s of a chain that `join-strings` joins), memory variables by
  name (`mv.get`, `mv.set`, `mv.ref`, `mv.private`, `mv.public`, `mv.param`; read and written as the heap is, since any
  call may create, change or end one; `mv.field` reads an undeclared name, a field of the current work area first),
  fields of work areas (`field.get` and `field.set`, by name, with the alias's name in the instruction or its item as
  the first operand; they read the context's state and may run code: an error block), the macro operator (`macro.get`,
  `macro.set`, `macro.call`, `macro.ref`, `macro.private`, `macro.public`, the text an item operand, and `macro.text`,
  the substitution in a string literal; a macro may do anything a call does, and create a PRIVATE of the function, which
  `createsPrivates` tells codegen), the truth test, calls (to the module's functions or to declared externals), `send`
  (a message to the first operand, with the others as arguments), `send.macro` (a text, then the object and the
  arguments: the message the text names when it runs; `index` 1 to assign), array literals (`...` in a call, a send or
  an array literal is its last operand, of type `args`, `args.rest`: the function's arguments past its parameters, made
  right before its list, which is its one use in the block; the callee's `byref` the positions of a call that receive
  references), `func.ref` (a function reference, `@name()`: the `xb_funcref` of its callee as an item), hash literals
  (`hash`, keys and values alternating), element read and write (`index`, `index.set`; `table.index`, an element of a
  table `STATIC`, fold-constant-statics in [Passes](#passes)), codeblock creation with the cells of its captured
  variables (`block`; with value cells, `cells vc...` in order, a cell of its own for each operand, a value captured,
  `v`, and a variable's, `c`), the `FOR` test, the `FOR EACH` operations (`each.source`, `each.in_cell`, `each.enter`,
  `each.move`, which takes the loop's collection too, `each.leave`, and `each.key`, the key at a position), the `SWITCH`
  check (whose result is the value the `CASE`s match: a substituted one after an error) and match, `seq.catch` and
  `seq.value` for `BEGIN SEQUENCE`, the generator operations (`gen.new`, `gen.restore`, `gen.done`, the terminator
  `gen.yield`, and `box.i64`, `box.bool`, `unbox.i64`, `unbox.bool` for the values kept in the frame), the asynchronous
  operations (`async.new`, `async.restore`, `await.ready`, `await.value`, `await.wait`, `await.fuse`, `await.fused`, the
  terminator `async.suspend`), `append` (`var := var + value`, `xb_op_append`), `scalar.plain` (a value of one of the
  types of a mask whose classes of the runtime answer its messages: `resolve-scalar-sends`), `object.is` (an object of a
  class of the module, exactly: `speculate-sends`), `has.operator` (an object whose class overloads an operator: the
  left side of `.AND.` and `.OR.`, `Generator::genLogic`, which gives the operator the right side's value, evaluated
  in a block both ways share; `fold-constants` makes it false where the value cannot be an object, and the plain
  logic is then all that is left), `subst.check` and `subst.value` for typed continuations (the
  verifier keeps the check right after its operation and the value first in the block only the check leads to), and for
  parameter versioning `params.ref` (whether parameters hold references their caller passed) and `call.forward` (a call
  with the function's own arguments, as received), `inline.enter` (naming a function or block function) and
  `inline.leave`, around an inlined body: the call stack shows that function in between, and `frame.release`, right
  after `x := NIL` to a local or a parameter: the frame slots of the values dead there are set to `NIL` (the store
  itself may go as a dead store, but the program let go of what x held); after the `LOCAL` standing for a parameter
  (`frame.release param k of NAME`), also the parameter's own slot, or in the body of NAME inlined, the argument its
  `inline.enter` holds, which nothing else lets go of before the function returns.
- An operation that fails, or leaves another pending request that asks to unwind, goes to its block's unwind block,
  or makes the function return (see [Control flow for errors and exceptions](#control-flow-for-errors-and-exceptions)).
- Terminators: `br`, `cond_br` (with block arguments), `gen.yield`, `async.suspend`, `ret`.
- Lowering to LLVM gives every live `item` value that may refer to a heap object a frame slot (a GC root; others are
  `Local`, see [compiler.md](compiler.md#xir-to-llvm)): liveness is computed per value by walking from its uses back to
  its definition, and slots are assigned by linear scan, so slots are reused once their values die. The operands of a
  call or a `send` and the elements of an array or hash literal form a group of consecutive slots; a value used only
  there (a block parameter too, such as the result of `IIF`) is computed straight into its slot of the group. A `load`
  whose single use follows with nothing in between that can write the variable uses the variable's own item instead of a
  copy (through a cell or a reference: what is in place, a local cell's value or a loop variable's element, else a
  shadow slot the runtime copies into), and so does
  any load of a frame variable nothing writes; a value used only by the `store` that follows it, when that writes the
  variable's own item, or by `ret`, is computed in place.

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
