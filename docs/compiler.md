# Compiler (`xbc`)

Code: `compiler/src/`. The preprocessor is described in [preprocessor.md](preprocessor.md); the runtime contract the
generated code relies on is in [runtime.md](runtime.md).

Pipeline (`main.cpp`): preprocess `std.ch` and the source, parse, resolve calls, fold constants, run semantic checks,
generate xIR and verify it, lower it to an in-memory LLVM module, optimize it, emit the object file. Each stage stops
the compilation when it reports errors. After 100 errors (`Diagnostics::kMaxErrors`) nothing more is reported and the
preprocessor and the parser stop early.

## Grammar

`parser/` is a recursive descent parser over preprocessed statements: `Parser.cpp` (tokens, keywords, the module and
its functions, blocks), `Statements.cpp`, `Expressions.cpp`; `AST.h` holds the syntax tree.

Keywords are not reserved. A keyword is recognized at the start of a statement, except when:

- the next token is an assignment or access to a variable (`:=`, `=`, compound assignments, `++`, `--`, `->`, `:`,
  `[`), so `next := 1` and `exit = 2` assign variables; before `++` and `--`, `IF`, `WHILE`, `ELSEIF`, `CASE`,
  `RETURN`, `SWITCH`, `NEXT`, `END`, `ENDIF`, `ENDDO` and `ENDCASE` stay keywords, as in Clipper (`IF --n > 0` is an
  `IF`, `next++` an error);
- it is one of `EXIT`, `LOOP`, `NEXT`, `END`, `ENDIF`, `ENDDO`, `ELSE`, `ENDCASE`, `OTHERWISE`, `ENDSWITCH` and
  something follows it on the statement;
- it is `FOR` and the statement has no `TO`, unless `EACH` follows (`FOR each := 1 TO n` is a `FOR` loop over a
  variable named `EACH`);
- it is `IF` followed by a parenthesized three-argument list ending the statement: `IF( c, a, b )` is an inline IF.

Keywords may be abbreviated down to a minimum length:

| Keyword | Min | Keyword | Min | Keyword | Min |
|---|---|---|---|---|---|
| `CASE` | 4 | `ENDSWITCH` | 5 | `OTHERWISE` | 4 |
| `DESCEND` | 7 | `EXIT` | 4 | `PARAMETERS` | 4 |
| `DO` | 2 | `FIELD` | 4 | `PRIVATE` | 4 |
| `EACH` | 4 | `FOR` | 3 | `PROCEDURE` | 4 |
| `ELSE` | 4 | `FUNCTION` | 4 | `PUBLIC` | 4 |
| `ELSEIF` | 5 | `IF` | 2 | `RETURN` | 4 |
| `END` | 3 | `IIF` | 3 | `STATIC` | 4 |
| `ENDCASE` | 4 | `IN` | 2 | `STEP` | 4 |
| `ENDDO` | 4 | `LOCAL` | 4 | `SWITCH` | 4 |
| `ENDIF` | 4 | `LOOP` | 4 | `TO` | 2 |
| `EXTERNAL` | 4 | `MEMVAR` | 4 | `WHILE` | 4 |
| | | `NEXT` | 4 | `WITH` | 4 |
| | | `NIL` | 3 | | |

Statements:

- `[STATIC] FUNCTION | PROCEDURE name [( params )]`. Statements before the first function form an implicit procedure
  named after the source file.
- `STATIC name [:= expr], ...` before the first function declares file-wide statics, visible in every function of the
  source file.
- `LOCAL name [:= expr], ...` and `STATIC name [:= expr], ...` at the start of a function, in any order. A function
  `STATIC` keeps its value between calls.
- `RETURN [expr]` (a `PROCEDURE` cannot return a value)
- `IF ... [ELSEIF ...] [ELSE ...] ENDIF | END`
- `DO CASE`, then `CASE cond` arms, an optional `OTHERWISE`, and `ENDCASE | END`; the first true arm runs. Nothing
  may stand between `DO CASE` and the first `CASE`.
- `SWITCH expr`, then `CASE constant` arms, an optional `OTHERWISE`, and `ENDSWITCH | END`. Control falls through
  from one arm into the next until `EXIT`, which leaves the `SWITCH`; `LOOP` continues the enclosing loop.
- `WHILE ... ENDDO | END`
- `FOR v := start TO limit [STEP step] ... NEXT | END`; the limit and step are re-evaluated on every iteration
- `FOR EACH v1 [, v2 ...] IN coll1 [, coll2 ...] [DESCEND] ... NEXT | END`
- `EXIT` (leaves the innermost loop or `SWITCH`), `LOOP`
- `EXTERN FUNCTION name( params ) [attributes]` at file level, before or between functions (see [Calls](#calls))
- `DO name [WITH args]`; empty arguments pass `NIL`. A variable written alone is passed by reference, as in Clipper;
  other arguments (an element, a variable in parentheses, an expression) by value
- expression statements; at statement level `x = expr` is an assignment

`PRIVATE`, `PUBLIC`, `MEMVAR`, `FIELD` and `PARAMETERS` are recognized and reported as not supported yet.

Expression precedence, lowest first: `:=` and compound assignments (right-associative), `.OR.`, `.AND.`, `.NOT.`/`!`,
`=` `==` `!=` `<>` `#`, `<` `<=` `>` `>=` `$`, `+` `-`, `*` `/` `%`, `^`/`**`, unary `-` `+`, prefix and postfix
`++`/`--`, element access `a[i]` (`a[i, j]` is `a[i][j]`), primaries (literals, `NIL`, variables, calls,
`IIF(c, a, b)`/`IF(c, a, b)`, parentheses, array literals `{ a, b }`, codeblocks `{| params | expr, ... }`). Empty
call arguments (`f(a, , b)`) and empty array elements pass `NIL`; a codeblock body has at least one expression. A
call argument `@name` or `@name[i]...` passes a variable or an element by reference (`RefExpr`); `@` anywhere else is
an error (references in array literals, which Harbour accepts, are not supported yet), and so are function
references `@name()`.
Variables and elements (`a[i] := v`, `a[i] += v`, `a[i]++`) can be assigned. `LOCAL a[n1, n2]` (or `a[n1][n2]`)
declares `a := Array( n1, n2 )`.

## Calls

A call goes to a function the module defines or to a declared external function. `EXTERN FUNCTION` (`EXTERNAL`, which
may be abbreviated down to `EXTE`) declares a function the module calls but does not define (a runtime function, a
function of a C library written to the runtime's convention, or a function of another module of the program); the
runtime's are declared in `include/rtl.ch`, which `std.ch` includes:

```
EXTERN FUNCTION SubStr( cString, nStart, [nCount] ) NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION QOut( ... ) NATIVE
```

Parameters are names; `[name]` is optional. `[ ]` delimit strings in xBase, so the parser receives a string and takes
any string whose text is a name as an optional parameter (a `.ppo` listing writes it as `"name"`, which compiles the
same). Required parameters come first, and `...` allows any number of further arguments. In a `NATIVE` declaration,
`@` before a parameter (`@name`, `[@name]`, `@...`) means the function receives a reference passed there (`Eval`
passes them on to the block); a native function otherwise receives the value of an argument written `@x`. An xBase
function always receives the reference, so `@` in a declaration without `NATIVE` is an error. Attributes:

| Attribute | Meaning |
|---|---|
| `RESERVED` | the program cannot define a function of this name (Clipper's predefined functions) |
| `CHECKED` | the argument count is checked at compile time, between the required and all parameters |
| `NATIVE` | written in C to the xBase calling convention: the call goes to `xbf_NAME`; without it the declaration names an xBase function of another module (`xbp_NAME`) |
| `ABBREV n` | calls may abbreviate the name to its first `n` or more letters (the keyword rule, `keywordMatches`) |
| `FOLD` | calls with constant arguments are evaluated at compile time (see [Constant folding](#constant-folding)) |

`sema/Calls.*` resolves every call before the other passes. A function the module defines comes first (a name that
is not `RESERVED` may be redefined); otherwise the call goes to the declaration with that name, or that it
abbreviates. The call is renamed to the full name and linked to its declaration (`CallExpr::callee`), and an argument
`@x` the declaration does not take by reference becomes `x` (`ExternDecl::takesRef`). The argument
count of a `CHECKED` name is checked even where the module redefines it. `LOCAL a[n]` calls the declared `Array()`
even if the module defines one (Clipper creates the array itself). It reports a call to a function that is neither
defined nor declared, a wrong argument count, a definition of a `RESERVED` name (also through an abbreviation), an
abbreviation that fits several declarations, conflicting declarations of one name (identical ones are accepted), a
`FOLD` function the compiler has no evaluator for, and a declaration inside a function.

## Semantic checks

`sema/Sema.*` assigns frame slots to parameters and `LOCAL`s (in declaration order; a `LOCAL`'s initializer is resolved
before its own name becomes visible). A codeblock has its own frame for its parameters. A variable of the enclosing
function used inside a codeblock is *captured*: it is marked shared ("boxed") in the function (`FuncDecl::boxed`),
and each codeblock between the use and the function gets a capture (`BlockExpr::captures`) that passes the cell
along; the reference resolves to `VarWhere::Capture` with the capture index. Parameters of an enclosing codeblock
cannot be captured ("outer codeblock variable is out of reach", as in Clipper). A function's `LOCAL` or parameter
passed by reference (`@x`) is marked shared too. A shared variable moves into a cell only when a codeblock captures it
or a reference is taken, at run time.

`STATIC`s get slots in the module's static storage (`Module::statics`, `VarWhere::Static`). File-wide statics are
visible in every function; a function's statics are visible in that function only. They are never captured: codeblocks
use the storage directly. A `STATIC` initializer must be a constant after folding: a number, string, logical, `NIL`,
an array literal of constants, or a codeblock whose variables are its own parameters and `STATIC`s (anything may be
called inside it). Anything else, a plain variable included, is "illegal initializer for STATIC". A `STATIC` is
visible in its own initializer, so a codeblock stored in it can call itself; a `LOCAL`'s initializer is resolved
before the `LOCAL` exists.

Sema reports: undeclared variables, duplicate functions, parameters or locals, `EXIT` outside a loop or `SWITCH`,
`LOOP` outside a loop, invalid assignment targets, a `LOCAL` or `STATIC` that follows an executable statement or
appears inside a nested block, a non-constant `STATIC` initializer, a `SWITCH` `CASE` that is not an integer or
string constant ("CASE requires either numeric or string constant"), and a `CASE` value repeated in a `SWITCH`.

## Code generation

Code generation has two steps: the syntax tree becomes xIR ([xir.md](xir.md)), which becomes an LLVM module.

### Syntax tree to xIR

`xir/Gen*.cpp` (`xir::Generator`) emits each function's xIR in Clipper's evaluation order; `xir/Verifier.*` checks
the result before it goes further (an invalid xIR is an internal error), and `--dump-xir=<file>` writes its text.

- Evaluation order: binary operators evaluate the left operand, then the right; calls evaluate their arguments in
  order; `x := v` evaluates `v`, then stores; `x op= v` loads `x`, then evaluates `v`; `a[i] := v` evaluates `v`,
  then `a`, then `i`; `a[i] op= v` and `a[i]++` evaluate `a`, then `i`, then the element, then `v`; every operand
  is evaluated once. A `load` is the variable's value at that point, so `x + (x := 5)`, `x := x + (x := 5)` and
  `x += (x := 5)` are 6 (Clipper's results; Harbour's own mode rewrites `x := x op v` as `x op= v` and reads `x`
  last, giving 10), and `x := x++` leaves `x` unchanged; the lowering to LLVM decides where a copy is actually
  needed.
- Variables: frame variables (parameters and `LOCAL`s), a codeblock's captures, and the module's `STATIC`s. A frame
  variable moves into a cell when a codeblock captures it (`block`) or a reference is taken (`ref`), as in Clipper,
  and a `STATIC` when a reference is taken; its own item then holds the cell (`Var::shared` marks the frame variables
  that may). A `FOR EACH`
  variable of a loop being generated is accessed through its reference; a `STATIC`, a shared variable, a captured
  variable or a parameter is tested at run time for a cell or a reference (an argument passed by reference); other
  `LOCAL`s never hold one. An argument `@x` is `ref` (the variable's cell), `@a[i]` is `ref.index`. A store into a
  variable that may live in the heap (a cell, an element through a reference) is followed by `barrier`.
- `.AND.`, `.OR.` and `IIF` branch; their results are block parameters. Conditions are `truth` tests of items.
- `DO CASE` becomes an `IF`/`ELSEIF` chain. `SWITCH` evaluates its value once, checks it (`switch.check`; a `SWITCH`
  without arms accepts anything) and compares it with each `CASE` constant in order (`switch.match`); the arm bodies
  follow each other so that control falls through.
- `FOR` re-evaluates the limit and the step at every test (the step after the limit), as Clipper does.
- `FOR EACH` evaluates the collections once, in order, and saves the loop variables (`each.enter`). During the loop
  each variable holds a reference (`XB_T_REF`, see [runtime.md](runtime.md#references)) to the current element, so
  reading or assigning the variable reads or assigns the element, and changes the body makes to the array through
  other paths are seen through the variable. An `i64` step counter (a block parameter) selects the element: index
  `step + 1`, or with `DESCEND` the collection's length at loop entry minus the step, so each collection is walked
  from its own end. An ascending loop checks every index against the collection's current length before each
  iteration, so it stops at the shortest collection and sees elements added by the body; a descending one stops when
  an index reaches 0. The variables get their saved values back at the end (`each.leave`, also through `EXIT`), and
  on `RETURN` those others can see (a `STATIC`, a shared variable, a parameter). One reference per variable serves
  the loop and is moved to each element (`each.move`). A variable shared when the loop starts (in a cell, or a
  parameter that received an element by reference; `each.in_cell`) holds it inside its cell, so everyone sharing the
  variable sees the current element and, after the loop, the restored value. A codeblock or reference that takes a
  `LOCAL` or parameter while it holds a loop's reference gets the current element instead of the variable, which
  stays the loop's: later iterations, inner loops on the same variable (here or in a callee) and the restore do not
  change what it designates. A `STATIC` is one variable for everyone: a reference to it designates it, and the loop
  follows its cell from the moment one is made.
- A codeblock becomes an xIR function of its own (`<function>.blockN`) whose frame holds its parameters; `block`
  creates it with the cells of the variables it captures (moving them into cells if they are not yet).
- `STATIC` initializers form the module initialization function.
- Code after `RETURN`, `EXIT` or `LOOP` lands in blocks nothing reaches, removed once the function is complete.

### xIR to LLVM

`codegen/` lowers each xIR function to an LLVM function with the dynamic calling convention described in
[runtime.md](runtime.md#calling-convention): `CodeGen.cpp` (types, runtime declarations, the module, functions and
frames), `FramePlan.*` (where each item lives), `Instructions.cpp`, `Variables.cpp` (variable addresses, references,
write barriers), `Literals.cpp`; `Emit.cpp` emits the object file.

- Frame: `FramePlan` gives every live item value a frame slot (a GC root): each value's liveness is found by walking
  from its uses back to its definition, slots are assigned by linear scan and reused once free (frame variables take
  slots 0 to n-1). The arguments of a call and the elements of an array literal take consecutive slots, and a value
  used only there (a block parameter too) is computed straight into its slot; a branch argument used only there
  shares the slot of its block parameter; a `load` whose single use follows with nothing in between that can write
  the variable uses the variable's own item (no copy); a value used only by the `store` right after it is computed
  into the variable, and one used only by `ret` into the caller's result slot. A call's result is never computed
  straight into a heap object (a cell, an element) and only into a plain frame variable, and an operation that can
  fail is not computed in place through a reference (finding the element can fail too, and the operation's error
  comes first). Two edges from one block to the same block do not share their arguments' slots.
- The entry block obtains the frame (`xb_frame_push`, of the planned size) and copies the parameters
  (`xb_args_copy`); the caller has already set the result to `NIL`. The exit block pops the frame and returns. Blocks
  are laid out in reverse postorder visiting successors last first, so a block's first successor (a loop body, a
  THEN branch) follows it and exits come later.
- Operations call the runtime (`xb_op_*` and the others); the result may alias an operand. Calls set the result slot
  to `NIL`, then pass `(argc, argv)` pointing at the argument slots; a call to an `EXTERN FUNCTION` goes to
  `xbf_NAME` or `xbp_NAME` (`NATIVE` or not), one to a function of the module to `xbp_NAME` (prefixes in `xbrt.h`).
- After every operation that can fail, the generated code loads `ctx->pending`; when it is set (marked unlikely) it
  calls `xb_pending` and leaves through the operation's unwind block (the function's exit when it has none) if that
  asks to unwind.
  `truth` tests the item type inline; a non-logical value calls `xb_cond_error` and leaves.
- Variables: a captured variable's cell is loaded from `self->cells[]` (the `xb_block` layout is mirrored with the
  target `DataLayout`); a `FOR EACH` variable goes through `xb_ref_target`; a `STATIC`, a shared or captured
  variable or a parameter tests its type inline (one unsigned comparison covers `XB_T_CELL` and `XB_T_REF`), follows
  a cell inline and calls `xb_ref_target` only for an element.
- `block` obtains the cells first (`xb_cell_of`: the variable's cell, made now if it has none, or a cell holding the
  current element of a `FOR EACH` variable), then creates the block.
  `ref` calls `xb_ref_of` (or builds the `XB_T_CELL` item of a captured variable's cell); `ref.index` calls
  `xb_ref_elem`. `each.enter`, `each.move` and `each.leave` call `xb_each_enter`, `xb_each_move` and `xb_each_leave`.
- `barrier` ([runtime.md](runtime.md#garbage-collector)): after a store into a cell, the cell's GC state byte is
  tested inline and `xb_gc_remember` called for an old cell; after a store through an element reference,
  `xb_ref_written` is called. Frame slots and `STATIC` storage are roots and need nothing.
- Block parameters: item parameters live in slots, and branches copy their arguments in (through a staging buffer
  when a destination is also a source); machine parameters (`bool`, `i64`) become PHIs.
- A module with `STATIC`s gets an internal global `xb.statics` (an array of NIL items), its initialization function,
  and a global constructor `xb.module.register` that calls `xb_module_register` before `main`.
- The start module also gets `xb_app_main`, which checks the ABI, runs the module initializations
  (`xb_modules_init`, followed by the pending check) and calls the first function of the source.
- 32-bit parameters and results of every declared or defined function carry the extension attribute the target ABI
  requires (`zeroext` on s390x, `signext` on RISC-V 64, none on x86-64), taken from LLVM's target library information.

## Constant folding

`opt/Fold.*` evaluates expressions on literals before semantic checks, so that checks on constants (`STATIC`
initializers, `SWITCH` cases) see the folded values. Folding is part of the language semantics: the
folded result's type and decimals follow Clipper's compile-time rules, which differ from the run-time operators.

| Expression on literals | Result |
|---|---|
| integer `+` `-` `*` integer | integer; on 64-bit overflow a double with 0 decimals |
| `+` `-` with a double | double; decimals: the larger of both, or those of the double operand |
| `*` with a double | double; decimals: the sum for two doubles, or those of the double operand |
| integer `/` integer | integer when exact, otherwise a double with SET DECIMALS decimals |
| `/` with a double, `%` with a double, `^` | double with SET DECIMALS decimals |
| integer `%` integer | integer |
| unary `-` | the negated number, same decimals |
| comparisons of numbers or logicals | `.T.`/`.F.` |
| strings `+`, `-`, `==`, `$`; `=`/`!=` of two empty strings | the result (`=` on other strings depends on SET EXACT and is not folded) |
| `.NOT.`, `.AND.`/`.OR.` with a logical literal on the left, `IIF` with a logical literal condition | the result or the remaining operand |
| a call to a `FOLD` function with constant arguments | its result, from the evaluator table in `opt/Fold.cpp`: `Len` of a string literal (its length) or of an array literal of constants (its element count) |

Division or `%` by a literal zero is not folded, so the error happens at run time. A folded integer is a program
constant (`xb_num_const_int_width`); a folded double has the default display width for its value. "SET DECIMALS
decimals" is resolved at run time: such a number is materialized by `xb_num_default_dec`, so `SET DECIMALS TO 4`
affects `10 / 4` as it does in Clipper. A logical literal on the right of `.AND.`/`.OR.` is not used to drop the left
operand, which keeps its side effects; Harbour drops it.

### Literals

Literals are constant globals in the object, so using one costs nothing at run time. A string literal is a static
`xb_str` (header, bytes, terminating NUL) flagged `XB_STR_STATIC`. An item literal is a structure whose value field has
the type of the member it represents, padded to the union size; the generator checks that its size and value offset
match `xb_item` for the target.

### Numeric literals

The display width of a numeric literal's integer part is fixed at compile time, as in Clipper. An integer literal is
a program constant: `xb_num_const_int_width` (10 columns plus one per digit beyond nine, 20 for -1000000000 or less;
leading zeros do not count). A double literal gets `xb_num_dbl_width` when it has fewer than 10 integer digits,
otherwise its own digit count. Literals too large for 64-bit integers become doubles.

## Object emission

`codegen/Emit.*` initializes every target LLVM was built with, creates the `TargetMachine` for `--target` (default: the
host), runs LLVM's default optimization pipeline for the `-O` level (new pass manager), and writes the object with the
target's object writer (code generation runs on the legacy pass manager: in LLVM 22 it is the one every target
implements). Code is position independent unless `--static` is given. The CPU is `generic` unless `--cpu` names one,
so programs run on any CPU of the target; `--cpu=native` (host target only) uses the CPU and features of the machine
xbc runs on.
