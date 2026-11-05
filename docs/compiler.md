# Compiler (`xbc`)

Code: `compiler/src/`. The preprocessor is described in [preprocessor.md](preprocessor.md); the runtime contract the
generated code relies on is in [runtime.md](runtime.md).

Pipeline (`main.cpp`): preprocess `std.ch` and the source, parse, run semantic checks, generate an in-memory LLVM
module, optimize it, emit the object file. Each stage stops the compilation when it reports errors. After 100 errors
(`Diagnostics::kMaxErrors`) nothing more is reported and the preprocessor and the parser stop early.

## Grammar

`Parser.*` is a recursive descent parser over preprocessed statements; `AST.h` holds the syntax tree.

Keywords are not reserved. A keyword is recognized at the start of a statement, except when:

- the next token is an assignment or access to a variable (`:=`, `=`, compound assignments, `++`, `--`, `->`, `:`,
  `[`), so `next := 1` and `exit = 2` assign variables;
- it is one of `EXIT`, `LOOP`, `NEXT`, `END`, `ENDIF`, `ENDDO`, `ELSE` and something follows it on the statement;
- it is `FOR` and the statement has no `TO`;
- it is `IF` followed by a parenthesized three-argument list ending the statement: `IF( c, a, b )` is an inline IF.

Keywords may be abbreviated down to a minimum length:

| Keyword | Min | Keyword | Min | Keyword | Min |
|---|---|---|---|---|---|
| `DO` | 2 | `FOR` | 3 | `PRIVATE` | 4 |
| `ELSE` | 4 | `FUNCTION` | 4 | `PROCEDURE` | 4 |
| `ELSEIF` | 5 | `IF` | 2 | `PUBLIC` | 4 |
| `END` | 3 | `IIF` | 3 | `RETURN` | 4 |
| `ENDDO` | 4 | `LOCAL` | 4 | `STATIC` | 4 |
| `ENDIF` | 4 | `LOOP` | 4 | `STEP` | 4 |
| `EXIT` | 4 | `MEMVAR` | 4 | `TO` | 2 |
| `FIELD` | 4 | `NEXT` | 4 | `WHILE` | 4 |
| `NIL` | 3 | `PARAMETERS` | 4 | `WITH` | 4 |

Statements:

- `[STATIC] FUNCTION | PROCEDURE name [( params )]`. Statements before the first function form an implicit procedure
  named after the source file.
- `LOCAL name [:= expr], ...`
- `RETURN [expr]` (a `PROCEDURE` cannot return a value)
- `IF ... [ELSEIF ...] [ELSE ...] ENDIF | END`
- `WHILE ... ENDDO | END`
- `FOR v := start TO limit [STEP step] ... NEXT | END`; the limit and step are re-evaluated on every iteration
- `EXIT`, `LOOP`
- `DO name [WITH args]`; empty arguments pass `NIL`. Arguments are passed by value (passing plain variables by
  reference, as Clipper does, needs references, which do not exist yet)
- expression statements; at statement level `x = expr` is an assignment

`STATIC`, `PRIVATE`, `PUBLIC`, `MEMVAR`, `FIELD` and `PARAMETERS` are recognized and reported as not supported yet.

Expression precedence, lowest first: `:=` and compound assignments (right-associative), `.OR.`, `.AND.`, `.NOT.`/`!`,
`=` `==` `!=` `<>` `#`, `<` `<=` `>` `>=` `$`, `+` `-`, `*` `/` `%`, `^`/`**`, unary `-` `+`, prefix and postfix
`++`/`--`, primaries (literals, `NIL`, variables, calls, `IIF(c, a, b)`/`IF(c, a, b)`, parentheses). Empty call
arguments (`f(a, , b)`) pass `NIL`.

## Semantic checks

`Sema.*` assigns frame slots to parameters and `LOCAL`s (in declaration order; an initializer is resolved before its
own name becomes visible) and reports: undeclared variables, duplicate functions, parameters or locals, `EXIT`/`LOOP`
outside a loop, invalid assignment targets, and a `LOCAL` that follows an executable statement or appears inside an
`IF`, `WHILE` or `FOR` body.

## Code generation

`CodeGen.*` lowers each function to an LLVM function with the dynamic calling convention described in
[runtime.md](runtime.md#calling-convention):

- The entry block obtains the item frame (`xb_frame_push`, sized after the body is generated) and copies the
  parameters. The caller has already set the result to `NIL`.
- Expressions evaluate into item slots. Variables and literals are used in place; temporaries come from the frame and
  are released at the end of each statement.
- Operators call the runtime (`xb_op_*`). The result may alias an operand. When the right operand of a binary operator
  has side effects and the left operand is a variable, the left value is copied first.
- An assignment whose value has side effects computes the value into a temporary first, so `x := x++` assigns the old
  value. A `FOR` whose `STEP` has side effects tests a copy of the limit.
- Calls set the result slot to `NIL`, then pass the arguments in consecutive frame slots.
- Conditions test the item type inline; a non-logical value calls `xb_cond_error` and leaves the function.
- After every call that can fail, the generated code loads `ctx->pending` and branches to the function exit when it is
  set (marked unlikely).
- The exit block pops the frame and returns.
- The start module also gets `xb_app_main`, which checks the ABI and calls the first function of the source.
- 32-bit parameters and results of every declared or defined function carry the extension attribute the target ABI
  requires (`zeroext` on s390x, `signext` on RISC-V 64, none on x86-64), taken from LLVM's target library information.

## Constant folding

`Fold.*` evaluates expressions on literals after semantic checks. Folding is part of the language semantics: the
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
| `Len( "literal" )` | the length, unless the module defines `LEN` |

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

`Emit.*` initializes every target LLVM was built with, creates the `TargetMachine` for `--target` (default: the host),
runs the default optimization pipeline for the `-O` level, and writes the object with the target's object writer. Code
is position independent unless `--static` is given.
