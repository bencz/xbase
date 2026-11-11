# Compiler (`xbc`)

Code: `compiler/src/`. The preprocessor is described in [preprocessor.md](preprocessor.md); the runtime contract the
generated code relies on is in [runtime.md](runtime.md).

Pipeline (`main.cpp`): preprocess `std.ch` and the source, parse, resolve calls, fold constants, run semantic checks,
generate xIR and verify it, optimize it (from `-O1` on, [xir.md](xir.md#passes)), lower it to an in-memory LLVM
module, optimize that, emit the object file. A stage that reports errors stops the compilation of that source; after
100 errors (`Diagnostics::kMaxErrors`) nothing more is reported and the preprocessor and the parser stop early.

## Sources and modules

`xbc` takes one or more sources (`xbc main.prg repository.prg`, `xbc *.prg`); each is a module, and a file given
twice is compiled once. A source whose file name contains `*` (any characters) or `?` (one character) and names no
file stands for the files of its directory that match it, sorted by name, except names starting with `.` unless the
pattern does; names compare without case on Windows. This serves shells that do not expand patterns. Each
source is preprocessed on its own, with `std.ch` and the command-line defines (its `#define`s and rules do not reach
the next source), and goes through the front end; a source with errors does not stop the others from being checked.
Two modules defining the same public function is an error. Then every module is lowered into one LLVM module and one
object (`-o`, by default the first source's name with `.o`), where calls between the modules are direct and LLVM can
inline across them; `-p` writes a `.ppo` per source, `-E` prints them all, `--depfile` lists every file read.

The program starts at the public function `MAIN` of any module, or else at the first function of the first source that
has one (a `STATIC` function named `MAIN`, and `INIT` and `EXIT` procedures, do not count). Clipper starts at the first
function; xbase prefers `MAIN`, so that `xbc *.prg` works whatever order the files come in, and a source may define
helpers before `Main`. `--no-entry` leaves the entry out, for modules compiled on their own and linked into a program
whose entry is in another object.

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
| `ANNOUNCE` | 4 | `EXTERNAL` | 4 | `PRIVATE` | 4 |
| `BEGIN` | 4 | `FIELD` | 4 | `PROCEDURE` | 4 |
| `CASE` | 4 | `FOR` | 3 | `PUBLIC` | 4 |
| `DECLARE` | 4 | `FUNCTION` | 4 | `RECOVER` | 4 |
| `DESCEND` | 7 | `IF` | 2 | `RETURN` | 4 |
| `DO` | 2 | `IIF` | 3 | `SEQUENCE` | 4 |
| `EACH` | 4 | `IN` | 2 | `STATIC` | 4 |
| `ELSE` | 4 | `INIT` | 4 | `STEP` | 4 |
| `ELSEIF` | 5 | `LOCAL` | 4 | `SWITCH` | 4 |
| `END` | 3 | `LOOP` | 4 | `TO` | 2 |
| `ENDCASE` | 4 | `MEMVAR` | 4 | `USING` | 4 |
| `ENDDO` | 4 | `NEXT` | 4 | `WHILE` | 4 |
| `ENDIF` | 4 | `NIL` | 3 | `WITH` | 4 |
| `ENDSWITCH` | 5 | `OTHERWISE` | 4 |  |  |
| `EXIT` | 4 | `PARAMETERS` | 4 |  |  |

Statements:

- `[STATIC | INIT | EXIT] FUNCTION | PROCEDURE name [( params [, ...] )]`. An `INIT` or `EXIT` procedure runs before or
  after the start function ([runtime.md](runtime.md#program-startup)); its name is its own: no call reaches it, a call
  by that name goes to the function of that name (of the module or another one), and a function may share it.
  Statements before the first function form an implicit procedure named after the source file. Any function accepts
  more arguments than it has parameters (`PCount()`, `PValue()` reach them); `...` after the parameters
  (`FuncDecl::variadic`) also lets its body pass them on (below).
- `STATIC name [:= expr], ...` before the first function declares file-wide statics, visible in every function of the
  source file.
- `EXTERNAL name, ...` (`REQUEST` in `std.ch`), anywhere in the module, names functions the program must link in though
  the source may never call them (they are reached by name, `Do()`): a function of another module is referenced from
  the object (a table kept with `llvm.compiler.used`, `xb.module.requests`), so the linker brings in the library member
  that defines it or reports it missing; the module's own functions and the runtime's (which are always linked in)
  need nothing. `EXTERNAL FUNCTION` is the declaration of [Calls](#calls).
- `ANNOUNCE name`, anywhere in the module, makes `name` a public procedure of the module, so that `REQUEST name`
  elsewhere links the module in: an empty procedure (it returns `NIL`) unless the module defines a public function of
  that name; a `STATIC` one is an error, and a second `ANNOUNCE` in the module is ignored with a warning.
- `LOCAL name [:= expr], ...` and `STATIC name [:= expr], ...` at the start of a function, in any order. A function
  `STATIC` keeps its value between calls.
- Memory variables ([runtime.md](runtime.md#memory-variables)), found by name at run time: `MEMVAR name, ...`
  declares names (before the first function, for the whole file; in a function, before its statements);
  `PRIVATE name [:= expr], ...` and `PUBLIC name [:= expr], ...` (also `name[n1, n2]`) create them when they run, the
  statement's initializers all running first (they read the variables visible before it); `DECLARE` is Clipper's other
  spelling of `PRIVATE`; `PARAMETERS name, ...` creates PRIVATEs holding the arguments received, in order (the function
  keeps its arguments, as for `PCount()`), and may come before the `LOCAL`s. `M->name` and `MEMVAR->name` are a memory
  variable whatever else the name is; other aliases (`alias->field`) wait for the work areas. `RELEASE` and
  `CLEAR MEMORY` are commands (`std.ch`).
- `RETURN [expr]` (a `PROCEDURE` cannot return a value)
- `IF ... [ELSEIF ...] [ELSE ...] ENDIF | END`
- `DO CASE`, then `CASE cond` arms, an optional `OTHERWISE`, and `ENDCASE | END`; the first true arm runs. Nothing
  may stand between `DO CASE` and the first `CASE`.
- `SWITCH expr`, then `CASE constant` arms, an optional `OTHERWISE`, and `ENDSWITCH | END`. Control falls through
  from one arm into the next until `EXIT`, which leaves the `SWITCH`; `LOOP` continues the enclosing loop.
- `WHILE ... ENDDO | END`
- `FOR v := start TO limit [STEP step] ... NEXT | END`; the limit and step are re-evaluated on every iteration;
  without `STEP` the counter steps as `++` does (an error is `++`'s)
- `FOR EACH v1 [, v2 ...] IN coll1 [, coll2 ...] [DESCEND] ... NEXT | END`, and `FOR EACH k => v IN coll ...` (one
  collection), whose `k` gets the key of each hash entry or the index of each array element or character (the position
  for an object); an object is gone through with its enumerator (`GetEnumerator()`, `MoveNext()`, `Current`, see
  [runtime.md](runtime.md#objects-and-messages))
- `EXIT` (leaves the innermost loop or `SWITCH`), `LOOP`
- `BEGIN SEQUENCE [WITH expr] ... [RECOVER [USING v] ...] [ALWAYS ...] END` (`std.ch` makes `END SEQUENCE` and
  `ENDSEQUENCE` an `END`); a `BREAK` in the body, or in what it calls, continues after `RECOVER` (or `END`; with
  `ALWAYS` and no `RECOVER`, it goes on after `ALWAYS`). The `WITH` block is the error block while the body runs;
  `ALWAYS` runs however the body and `RECOVER` end. `RETURN`, and `EXIT` or `LOOP` for a loop around the sequence,
  cannot leave its body (Clipper rule) nor `ALWAYS`; in `RECOVER` they can (`ALWAYS` runs first). `BREAK [value]` is
  a `std.ch` command (`__Break()`, the same as `Break()` and `Throw()`).
- `YIELD expr` in a function or method makes it a generator: calling it returns an enumerator (see
  [runtime.md](runtime.md#generators)) whose `MoveNext()` runs the body up to the next `YIELD`, the element in
  `Current`; `FOR EACH` goes through it. `YIELD` followed by `(` is a call of a function `Yield()`, as in Clipper: the
  element is never written in parentheses right after `YIELD`. `RETURN` (without a value) and the end of the body end
  it. Not in a codeblock, nor in any part of a `BEGIN SEQUENCE` with `ALWAYS` or a `TRY` with `FINALLY` (a generator
  left before its end would never run that code); in another sequence an error of a later step reaches its `RECOVER` or
  `CATCH`, and between the steps the caller runs with its own error block. A generator takes no `...`, reads no
  arguments as received (`PCount()`, `PValue()`, `PARAMETERS`) and creates no `PRIVATE` or `PUBLIC`.
- `WITH OBJECT expr ... END` (`END WITH` too): inside, `:name` (also `:name( args )`, assigned, `++`) is a message to
  the object, evaluated once into a `LOCAL` of its own (`WithObjectStmt::hidden`, a name no program can write). A
  codeblock made there captures that variable, as it would any `LOCAL`: it sends to the object the statement holds,
  also after `END`, and to the next one when the statement runs again (the reference sends to the `WITH OBJECT`
  running when the block is evaluated). The innermost `WITH OBJECT` wins; `:name` outside one is an error.
- `TRY ... [CATCH [v] ...] [FINALLY ...] END` (`END TRY` too): a sequence whose error block is `{| e | Break( e ) }`,
  so an error reaches `CATCH` as its `Error` object and a `Throw( x )` as `x`; `FINALLY` is its `ALWAYS`. `RETURN`,
  `EXIT` and `LOOP` may leave its body and `CATCH` (`FINALLY` runs first), not `FINALLY`.
- `EXTERN FUNCTION name( params ) [attributes]` at file level, before or between functions (see [Calls](#calls))
- Classes ([runtime.md](runtime.md#classes)), at file level: `[STATIC | CREATE | SCALAR] CLASS name [INHERIT | FROM
  parent, ...]`, then members, then `ENDCLASS` (or `END [CLASS]`): `EXPORTED:` (`EXPORT:`, `VISIBLE:`), `PROTECTED:` and
  `HIDDEN:` set the scope of the members after them; `VAR | DATA name, ... [AS | TYPE type] [INIT | ASSIGN expr] [scope]
  [READONLY | RO]` (the type is accepted and ignored; each name gets the INIT expression; `scope` is `EXPORTED`,
  `VISIBLE`, `PROTECTED` or `HIDDEN`); `CLASSVAR`, `CLASSDATA`, `CLASS VAR` or `CLASS DATA`, as `VAR` with `SHARED` too,
  declare class variables; `FRIEND CLASS name, ...` and `FRIEND FUNCTION name, ...`; `DESTRUCTOR name[()]` (one per
  class, implemented as a procedure: `METHOD PROCEDURE name CLASS ...` or `DESTRUCTOR name CLASS ...`; a method
  implementation may also be written `METHOD FUNCTION name`); `METHOD name[( params )] [AS type] [OPERATOR "op"]
  [CONSTRUCTOR] [INLINE expr, ...] [scope]` (with `OPERATOR`, the method answers the operator); `OPERATOR "op" [ARG |
  ARGS name, ...] INLINE expr, ... [scope]`; `ACCESS name [INLINE expr, ...]` and `ASSIGN name( param ) [INLINE expr,
  ...]` (an `INLINE` list is evaluated in order and returns its last value). A method is implemented with `METHOD name[(
  params )] [CLASS name]` (also `ACCESS name CLASS ...` and `ASSIGN name( param ) CLASS ...`; without `CLASS`, the last
  class declared before it in the preprocessed source, an included file's too) and a body, like a function. In a method,
  `Self` is the object, `::name` is `Self:name`, and `Super:name` (when no variable named `Super` is declared there) or
  `::Super:name` the member of its class's ancestors. Elsewhere `Self` and `Super` are ordinary names. `SCALAR CLASS
  type` declares the class of a built-in type's values (its name is the type's as `ClassName()` gives it; no class
  function, no instance variables).
- `DO name [WITH args]`; empty arguments pass `NIL`. A variable written alone is passed by reference, as in Clipper;
  other arguments (an element, a variable in parentheses, an expression) by value
- expression statements; a statement `operand = expr` is an assignment, and `=` inside `expr` compares:
  `x = y = 3` is `x := ( y = 3 )`, `a[1] = 5` assigns. The parser reports an invalid assignment target (anything but
  a variable or an element, also in parentheses) here, for `:=`, `op=`, `++`/`--` and `@`.

`FIELD` is recognized and reported as not supported yet.

Expression precedence, lowest first: `:=` and compound assignments (`+=` `-=` `*=` `/=` `%=` `^=`, and xbase's `&=` `|=`
`^^=` `<<=` `>>=`; right-associative), `.OR.`, `.AND.`, `.NOT.`/`!`, xbase's bit operators `|`, `^^` and `&`, `=` `==`
`!=` `<>` `#`, `<` `<=` `>` `>=` `$`, the shifts `<<` `>>`, `+` `-`, `*` `/` `%`, `^`/`**`, unary `-` `+` `~`, prefix
and postfix `++`/`--`, element access `a[i]` (`a[i, j]` is `a[i][j]`) and messages `o:name`, `o:name( args )` (in any
order: `a[1]:cargo[2]`), primaries (literals, `NIL`, variables, calls, `IIF(c, a, b)`/`IF(c, a, b)`, parentheses,
expression lists `( a, b, ... )` evaluated in order with the value of the last, array literals `{ a, b }`, hash literals
`{ k => v, ... }` and `{=>}`, codeblocks `{| params | expr, ... }`). Empty call arguments (`f(a, , b)`) and empty array
elements pass `NIL`; a codeblock body has at least one expression. An extended codeblock, `{| params |` at the end of a
line, then statements, then a line that starts with `}` (the expression goes on after it), has statements for a body
(`BlockExpr::extended`, `statements`): `LOCAL`s of its own after the parameters, `RETURN` with its result (`NIL` when it
ends without one; the procedure rule does not apply), any other statement but `STATIC`, `MEMVAR`, `PRIVATE`, `PUBLIC`
and `PARAMETERS`. A codeblock inside captures its `LOCAL`s like a function's, and they can be passed by reference
(`BlockExpr::boxed`). It is generated like any codeblock, with a frame of its own. A call argument `@name` or
`@name[i]...` passes a variable or an element by reference (`RefExpr`); `@` anywhere else is an error (references in
array literals are not supported yet), and so are function references `@name()`. `...` as the last argument of a call or
a send or the last element of an array literal (`SpreadExpr`; once, with nothing after it, not even an empty argument)
stands for the arguments the function received past its parameters, as many as there are: `f( a, b, ... )`, `o:m( ...
)`, `{ ... }`. Sema accepts it only in a function declared with `...`, outside its codeblocks (which have arguments of
their own); a `CHECKED` function's count includes the other arguments only, so only too many of them is an error.
Variables, elements (`a[i] := v`, `a[i] += v`, `a[i]++`), instance variables (`o:x := v`, a message without arguments)
and macros can be assigned. `LOCAL a[n1, n2]` (or `a[n1][n2]`) declares `a := Array( n1, n2 )`.

The levels of the binary operators are a table (`binaryLevels` in `parser/Expressions.cpp`). The bit operators sit where
C has them, so a comparison binds tighter than `&`, `^^` and `|`: `x & 1 == 0` is `x & ( 1 == 0 )`, an argument error at
run time, and is written `( x & 1 ) == 0`. `&` is the bit operator after an operand; before one it is the macro
operator. `|` is the operator inside a codeblock's body; the bars around its parameters stay delimiters.

The macro operator ([runtime.md](runtime.md#macro-operator)) takes the text of an expression, compiled when the code
runs (`MacroExpr`, whose `text` is the expression giving the string):

- `&name` (no blanks between), optionally ended by a `.`: the text in the variable `name`. Identifiers joined to it
  with no blanks make a name of their own (`&name.suffix`, `prefix&name.`, `a&b`): its text is the name as written
  (`"A&B"`, a `MacroTextExpr`), which the text substitution turns into the name when the code runs (a part that is
  not a memory variable holding a string stays, and the text is then error 1449). With blanks, `a & b` is the bit
  operator.
- `&( expr )`: the text is the expression's value.
- A macro can be assigned (`&c := v`, `&c += v`, `&c++`; the text is evaluated once), and called: `&name( args )`
  (`MacroCallExpr`, the function the text names, found by name when the code runs; `...` is not accepted there).
- `PRIVATE &name` and `PUBLIC &name` (also `&( expr )`, with an initializer or array dimensions) create the variable
  the text names. A macro cannot be passed by reference (`@&name`, an error).
- A string literal with `&name` in it (`"Hello &cName."`) is substituted when the code runs (`MacroTextExpr`; Clipper's
  rule): it is not a constant, so nothing folds it. A name there (or in a macro name joined to identifiers) that is a
  `LOCAL`, `STATIC` or parameter of the code is an error (the substitution sees memory variables only).

Nesting is limited, so that a deep source is reported instead of exhausting the compiler's stack: 1024 levels of
nested expressions (parentheses, calls, arrays, codeblocks, prefix operators; `Parser::kMaxExprNesting`) and 1024 of
nested statements (`Parser::kMaxBlockNesting`). One error is reported, and the rest of the statement, or of the
source, is skipped. Chains of binary operators are not nested in the source but make deep trees; the preprocessor's
limit on a statement's tokens bounds them, and `xbc` runs its stages on a thread with a 256 MB stack
(`kCompilerStackSize` in `main.cpp`; where LLVM is built without threads, on the main thread's own stack).

## Calls

A call goes to a function the module defines, to a declared external function, or else, as in Clipper, to a public
function of another module, found by name when the program is linked (`xbp_NAME`): no declaration is needed for the
functions of the program's other modules, whether they are compiled in the same run or on their own. `EXTERN FUNCTION`
(`EXTERNAL`, which may be abbreviated down to `EXTE`) declares a function the module calls but does not define (a
runtime function, a function of a C library written to the runtime's convention, or a function of another module whose
argument count is to be checked); the runtime's are declared in `include/rtl.ch`, which `std.ch` includes:

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
| `ARGS` | the function reads the arguments the function calling it received (`PCount()`): that function (or codeblock) keeps them in its frame (`FuncDecl::readsArgs`, `BlockExpr::readsArgs`, `xir::Function::readsArgs`; `args` in the xIR listing) |
| `PARAMS` | `ARGS`, and the function reads and writes the parameters of the function calling it (`PValue()`): that function keeps them in its frame slots, as written (no parameter versioning, no SSA values) |
| `PRIVATES` | the function creates PRIVATEs of the function calling it (`RESTORE`): that function (or codeblock) takes a PRIVATE mark and ends them when it returns (`FuncDecl::callsPrivates`, `BlockExpr::callsPrivates`, `xir::Function::callsPrivates`; `privates` in the xIR listing) |

`sema/Calls.*` resolves every call before the other passes. A function the module defines comes first (a name that is
not `RESERVED` may be redefined); otherwise the call goes to the declaration with that name, or that it abbreviates; a
name neither defined nor declared is another module's function (`CallExpr::external`). The call is renamed to the full
name and linked to its declaration (`CallExpr::callee`), and an argument `@x` the declaration does not take by reference
becomes `x` (`ExternDecl::takesRef`). The argument count of a `CHECKED` name is checked even where the module redefines
it. `LOCAL a[n]` calls the declared `Array()` even if the module defines one (Clipper creates the array itself). It
reports a wrong argument count, a definition of a `RESERVED` name (also through an abbreviation), an abbreviation that
fits several declarations, conflicting declarations of one name (identical ones are accepted), a `FOLD` function the
compiler has no evaluator for, and a declaration inside a function.

`xbc --runtime` compiles modules of the runtime library ([runtime.md](runtime.md#functions-written-in-xbase)): a module
may define `RESERVED` functions, its public functions are runtime functions (`xbf_NAME`, not registered: the runtime's
symbol table has them), their frames keep line 0 (no line is stored before calls), and its `STATIC` functions and
codeblocks are hidden from the call stack (`xir::Function::hidden`, `XB_PROC_HIDDEN`). No program entry is emitted.

## Semantic checks

`sema/Classes.*` runs first and makes classes functions: a method's implementation becomes the `STATIC` function
`CLS:MESSAGE` (what the call stack shows) whose first parameter is `SELF`, an `INLINE` method a function returning its
expression, the class gets its class function `NAME()` (`FuncDecl::instantiates`: a new object) and, when its
variables have `INIT` values, `CLS:(INIT)`, which returns them as an array. It reports a class defined twice or with a
function's name, a class that inherits from itself through the module's classes, a member declared twice (a variable
and a method of the same message too), an implementation of nothing declared or implemented twice, and a declared
method without an implementation. Sema decides whether `Super:name` and `::Super:name` are a send to the ancestors
(`SendExpr::super`); `::` outside a method is an error, even where a variable is named `Self`.

`sema/Sema.*` assigns frame slots to parameters and `LOCAL`s (in declaration order; a `LOCAL`'s initializer is resolved
before its own name becomes visible). A codeblock has its own frame for its parameters. A variable of the enclosing
function used inside a codeblock is *captured*: it is marked shared ("boxed") in the function (`FuncDecl::boxed`), and
each codeblock between the use and the function gets a capture (`BlockExpr::captures`) that passes the cell along; the
reference resolves to `VarWhere::Capture` with the capture index. A variable of an enclosing codeblock (a parameter, an
extended codeblock's `LOCAL`) is captured the same way, marked shared in that codeblock (`BlockExpr::boxed`); Clipper
and the reference reject that ("outer codeblock variable is out of reach"). A function's or a codeblock's `LOCAL` or
parameter passed by reference (`@x`) is marked shared too, and so are all the parameters of a function that calls a
`PARAMS` function (`PValue()`), which reads and writes them in the frame. A shared variable moves into a cell only when
a codeblock captures it or a reference is taken, at run time.

`STATIC`s get slots in the module's static storage (`Module::statics`, `VarWhere::Static`). File-wide statics are
visible in every function; a function's statics are visible in that function only. They are never captured: codeblocks
use the storage directly. A `STATIC` initializer must be a constant after folding: a number, string, logical, `NIL`, an
array or hash literal of constants, or a codeblock whose variables are its own parameters and `STATIC`s (anything may be
called inside it). Anything else, a plain variable included, is "illegal initializer for STATIC". A `STATIC` is visible
in its own initializer, so a codeblock stored in it can call itself; a `LOCAL`'s initializer is resolved before the
`LOCAL` exists.

A name that is not a parameter, `LOCAL`, `STATIC` or capture is a memory variable (`VarWhere::Memvar`), as in Clipper
(which looks for a field of the current work area first; fields come with the work areas). A `MEMVAR` of a frame (the
file or a function) makes the name a memory variable there, before a `STATIC` of an enclosing frame, and cannot name a
`LOCAL`, parameter or `STATIC` of the same frame; a function's `LOCAL` hides a file-wide `MEMVAR`. `PRIVATE`, `PUBLIC`
and `PARAMETERS` do not declare (as in the reference, a `STATIC` of the name stays the `STATIC`), and their names cannot
be a `LOCAL` or parameter of the same function. A name nothing declares (no `MEMVAR`, and no `PRIVATE`, `PUBLIC` or
`PARAMETERS` earlier in the function) gets a warning; `-Wno-undeclared` drops it and `-Werror=undeclared` makes it an
error. A memory variable cannot be the variable of a `FOR EACH` yet.

Sema reports: duplicate functions, parameters or locals, `EXIT` outside a loop or `SWITCH`, `LOOP`
outside a loop, a `LOCAL`, `STATIC` or `MEMVAR` that follows an executable statement or appears inside a nested block, a
non-constant `STATIC` initializer, a `SWITCH` `CASE` that is not an integer or string constant ("CASE requires either
numeric or string constant"), and a `CASE` value repeated in a `SWITCH`.

## Code generation

Code generation has two steps: the syntax tree becomes xIR ([xir.md](xir.md)), which becomes an LLVM module.

### Syntax tree to xIR

`xir/Gen*.cpp` (`xir::Generator`) emits each function's xIR in Clipper's evaluation order; `xir/Verifier.*` checks
the result before it goes further (an invalid xIR is an internal error). From `-O1` on the xIR passes run
([xir.md](xir.md#passes)); `--dump-xir=<file>` writes the xIR that is lowered to LLVM.

- Evaluation order: binary operators evaluate the left operand, then the right; calls evaluate their arguments in order;
  `x := v` evaluates `v`, then stores; `x op= v` loads `x`, then evaluates `v`; `a[i] := v` evaluates `v`, then `a`,
  then `i`; `a[i] op= v` and `a[i]++` evaluate `a`, then `i`, then the element, then `v`; `o:x := v` evaluates `o`, then
  `v`, then sends `_X` with it; `o:x op= v` and `o:x++` evaluate `o`, send `X`, evaluate `v`, then send `_X` with the
  new value (the expression's value is what `_X` returns, the old value for `o:x++` and `o:x--`); every operand is
  evaluated once. A `load` is the variable's value at that point, so `x + (x := 5)`, `x := x + (x := 5)` and
  `x += (x := 5)` are 6, as in Clipper, and `x := x++` leaves `x` unchanged; the lowering to LLVM decides where a copy
  is actually needed. `...` is read when the call is made (`args.rest` right before it), after the other arguments.
- Messages: `o:name` and `o:name( args )` (`SendExpr`; postfix like `[`, so `a[1]:cargo[2]` chains) are `send`, the
  object first and then the arguments, all by value (`@` is not accepted there). A message written without arguments
  is an lvalue (`isLvalue`): it can be assigned, incremented and be the target of a statement `o:x = v`, but not
  passed by reference.
- Variables: frame variables (parameters and `LOCAL`s), a codeblock's captures, and the module's `STATIC`s. A frame
  variable moves into a cell when a codeblock captures it (`block`) or a reference is taken (`ref`), as in Clipper,
  and a `STATIC` when a reference is taken; its own item then holds the cell (`Var::shared` marks the frame variables
  that may). A `FOR EACH`
  variable of a loop being generated is accessed through its reference; a `STATIC`, a shared variable, a captured
  variable or a parameter is tested at run time for a cell or a reference (an argument passed by reference); other
  `LOCAL`s never hold one. An argument `@x` is `ref` (the variable's cell), `@a[i]` is `ref.index`. A store into a
  variable that may live in the heap (a cell, an element through a reference) is followed by `barrier`.
- Parameter versioning: a parameter may hold a reference only if its caller passed one, and one that is never shared
  cannot become one. A function with such parameters is generated twice: its own body starts with `params.ref`,
  which tests them; when none holds a reference, each is copied once into a `LOCAL` standing for it (which mem2reg
  makes an SSA value), and otherwise `call.forward` passes the arguments as received to the other body, `NAME.byref`
  (internal), which reaches the parameters through their references. Codeblocks inside are generated with each body.
- `.AND.`, `.OR.` and `IIF` branch; their results are block parameters. Conditions are `truth` tests of items. The
  left side of `.AND.`/`.OR.` must be logical; when it does not settle the result, the right side's value is the
  result as it is, whatever its type (`.T. .AND. 5` is 5, as in Clipper), and only a condition tests it.
- `DO CASE` becomes an `IF`/`ELSEIF` chain. `SWITCH` evaluates its value once, checks it (`switch.check`; a `SWITCH`
  without arms accepts anything) and compares it with each `CASE` constant in order (`switch.match`); the arm bodies
  follow each other so that control falls through.
- `FOR` re-evaluates the limit and the step at every test (the step after the limit), as Clipper does.
- `FOR EACH` evaluates the collections once, in order (`each.source`: an object's enumerator, or for `DESCEND` its
  elements in an array; anything else as it is), and saves the loop variables (`each.enter`). During the loop each
  variable holds a reference (`XB_T_REF`, see [runtime.md](runtime.md#references)) to the current element, so reading or
  assigning the variable reads or assigns the element, and changes the body makes to the array through other paths are
  seen through the variable. An `i64` step counter (a block parameter) selects the element: index `step + 1`, or with
  `DESCEND` the collection's length at loop entry minus the step, so each collection is walked from its own end. An
  ascending loop checks every index against the collection's current length before each iteration, so it stops at the
  shortest collection and sees elements added by the body; a descending one stops when an index reaches 0. The variables
  get their saved values back at the end (`each.leave`, also through `EXIT`), and on `RETURN` and when unwinding leaves
  the loop those others can see (a `STATIC`, a shared variable, a parameter), all of them when the unwinding stops at a
  `BEGIN SEQUENCE` of the function or a `RETURN` runs `ALWAYS` code (before that code, the loops inside its sequence).
  An ascending loop over several collections stops at the first one at its end, so an enumerator after it is not moved.
  One reference per variable serves the loop and is moved to each element (`each.move`). A variable shared when the loop
  starts (in a cell, or a parameter that received an element by reference; `each.in_cell`) holds it inside its cell, so
  everyone sharing the variable sees the current element and, after the loop, the restored value. A codeblock or
  reference that takes a `LOCAL` or parameter while it holds a loop's reference gets the current element instead of the
  variable, which stays the loop's: later iterations, inner loops on the same variable (here or in a callee) and the
  restore do not change what it designates. A `STATIC` is one variable for everyone: a reference to it designates it,
  and the loop follows its cell from the moment one is made. In `FOR EACH k => v`, `k` is an ordinary variable, other
  than `v` (the parser rejects `FOR EACH x => x`): before each iteration `each.key` stores into it the key of the
  current hash entry, or the index for an array or a string, so assigning it changes nothing else, and after the loop it
  keeps the last key stored (the one `EXIT` left). A `k` that is the variable of an enclosing `FOR EACH` receives the
  keys as any assignment would: in that loop's element.
- `BEGIN SEQUENCE` gives the blocks of its body an unwind block, the handler: a `BREAK` (pending after an operation
  of the body, or a call) is taken there and `RECOVER` follows, with the `BREAK` value stored in the `USING`
  variable; other requests go on unwinding (see [xir.md](xir.md#control-flow-for-errors-and-exceptions)). `WITH`
  starts with `seq.with` and puts the error block back (`seq.restore`) at the end of the body and in the handler.
  `ALWAYS` code is generated where the body and `RECOVER` join, in an unwind block of its own (`seq.suspend` and
  `seq.value` before it, `seq.resume` after it, then onward), and before each `RETURN`, `EXIT` or `LOOP` that leaves
  the sequence (`Generator::exitCleanups`, which also puts the error block back), in a block that unwinds where the
  sequence does. `TRY` is the parser's `SequenceStmt` with the handler block built in.
- A generator (`FuncDecl::generator`, set by the parser; Sema adds the enumerator as parameter 0) becomes two xIR
  functions (`Generator::genGenerator`): the function itself, which returns `gen.new` of its resume function with its
  arguments, and `NAME.resume`, the body: it starts with `gen.restore` and a dispatch on the state it returns (0: the
  start; k: after the k-th `YIELD`), `YIELD` is the terminator `gen.yield` (the frame and the element saved, `.T.`
  returned) whose target is where state k goes on, and `RETURN` and the end are `gen.done` and `.F.`. Inside a sequence
  `WITH` a handler (also `TRY`), the error block it replaced lives in a frame variable of its own
  (`Cleanup::oldBlockVar`, `Generator::hiddenVar`): before `gen.yield` each such handler, innermost first, is read
  (`seq.with` of NIL) and the replaced block put back; after it, outermost first, `seq.with` puts the handler back and
  stores the caller's block of the moment as the one to put back in the end. Then `makeResumable`
  (`xir/passes/Resumable.cpp`) moves every value whose definition does not dominate a use (one that lives across a
  `YIELD`) into a frame variable of its own, stored where it is computed and loaded before each use (a `Bool` or `I64`
  boxed with `box.bool`/`box.i64`); promote-variables and typed continuations leave generators alone.
- A codeblock becomes an xIR function of its own (`<function>.blockN`) whose frame holds its parameters; `block`
  creates it with the cells of the variables it captures (moving them into cells if they are not yet).
- `STATIC` initializers form the module initialization function.
- Code after `RETURN`, `EXIT` or `LOOP` lands in blocks nothing reaches, removed once the function is complete.

### xIR to LLVM

`codegen/` lowers each xIR function to an LLVM function with the dynamic calling convention described in
[runtime.md](runtime.md#calling-convention), which is, for a function with a direct entry (below), a thunk in front of
it: `CodeGen.cpp` (types, runtime declarations, the module, functions and frames), `FramePlan.*` (where each item
lives), `Instructions.cpp`, `Variables.cpp` (variable addresses, references, write barriers), `Literals.cpp`; `Emit.cpp`
emits the object file.

- Frame: `FramePlan` gives every live item value a frame slot (a GC root): each value's liveness is found by walking
  from its uses back to its definition, slots are assigned by linear scan and reused once free. The parameters take
  slots 0 to n-1 (the prologue copies them there), then the frame variables still in memory (those mem2reg did not make
  values), then temporaries. The arguments of a call and the elements of an array literal take consecutive slots, and a
  value used only there (a block parameter too) is computed straight into its slot; a branch argument used only there
  shares the slot of its block parameter (on a loop's back edge, when the jump is the block's only way out and nothing
  after the argument reads the parameter); a `load` whose single use follows with nothing in between that can write the
  variable (for a variable others can reach: nothing that writes, runs code, or goes on after an error block ran)
  uses the variable's own item (no copy), and so does every load of a frame variable nothing writes once the function
  runs (`FramePlan::findUnwritten`: a parameter the body versioned for references leaves to the other body, a `LOCAL`
  never assigned); a value used only by the `store` right after it is computed into the
  variable, and one used only by `ret` into the caller's result slot. A call's result is never computed straight into a
  heap object (a cell, an element) and only into a plain frame variable, and an operation that can fail is not computed
  in place through a reference (finding the element can fail too, and the operation's error comes first) or in a block
  that unwinds (the handler may read the variable, and a call clears its result first). Two edges from
  one block to the same block do not share their arguments' slots. A value that never refers to a heap object
  (`xir::TypeInfo`, [xir.md](xir.md#types)) and is not computed in a slot as above is `Local`: an item of its own
  outside the frame (an `alloca` reached only by loads and stores, which LLVM keeps in registers). Block parameters are
  chosen first, other values after the groups and the parameter slots. Runtime calls never receive a `Local`'s address:
  an operand goes through a scratch item, and a result is written there and copied back (`callArg`, `callResult`). When
  type inference finds a single possible type for an operand (`typeOf`), the fast paths take its tag as a constant, and
  LLVM removes the tests and the paths of other types: arithmetic on values known to be doubles is plain floating-point
  code.
- Alias information (TBAA): every load and store of generated code whose memory is known is tagged with its class,
  classes that never overlap (`CodeGen::Mem`): items outside the heap (an `alloca` or a global of the module: frames,
  `Local` and scratch items, `STATIC` storage, literals), items in the heap (addresses from `heapItem`: elements, cells,
  reference targets), array headers, codeblocks' cell pointers, the context's head, frame links, the collector's state
  bytes and strings (lengths and bytes, which compiled code reads, and the flags, where it clears `XB_STR_APPEND`). An
  address that may be either kind (a variable reached through a possible reference, an argument such as `argv` or the
  result pointer) is not tagged. LLVM then knows that a store into an element changes no frame slot and no array header;
  by itself this changes little in the code today, and it is the ground for moving array headers out of fast loops.
- Items are stored and copied as two 8-byte words, the header (type, width, decimals; `headerWord`, in the target's byte
  order, as `xb_header_word` in `xbrt.h`) and the value, and fields are read out of the header word (their bit positions
  come from the target's layout, `headerShift`): a load is served by the store that wrote it (store forwarding) only
  when that store covers it. Between items in memory the value word is copied as two 32-bit integers (bit for bit), a
  type different from the header's, so that code generation does not merge the pair into one 16-byte access spanning two
  stores.
- The entry block allocates the frame of the planned size on the stack (`pushFrame`: an `alloca` of `xb_frame`, see
  [runtime.md](runtime.md#frames)), copies the parameters into the first slots (in a direct entry, from the words in
  registers; otherwise branch-free from a selected source, `argv[k]` or a `NIL` constant: `argItem`), clears the other
  slots, stores the arguments as received (`argv`, `argc`) when the function reads them (`readsArgs`) and links the
  frame into the context; the caller has already set the result to `NIL`. The exit block unlinks the frame and returns.
  Blocks are laid out in reverse postorder visiting successors last first, so a block's first successor (a loop body, a
  THEN branch) follows it and exits come later.
- Operations call the runtime (`xb_op_*` and the others); the result may alias an operand. `FastPaths.cpp` inlines the
  common cases first: `/` and `%` of two numbers with a divisor other than 0 (a double with SET DECIMALS decimals, read
  from the context's head; `%` of two integers is their remainder, 0 for a divisor of -1, and of doubles that hold
  integers below 2^53 in magnitude their integer remainder with the dividend's sign, fmod's exactly, without calling it:
  `doubleRemainder`); `+`, `-` and `*` of two integers with LLVM's overflow intrinsics (an overflow, like any other
  operand types, calls the operator, which makes a double); `+`, `-` and `*` of two numbers of which one is a double, in
  doubles (an integer beyond 2^53 in magnitude, which a double cannot hold exactly, goes to the runtime, so that every
  target converts it as the runtime does), with the decimals the runtime gives (the larger for `+` and `-`, the sum for
  `*`); two doubles, the usual case of these paths, take a path of their own (their bits and decimals as they are, no
  test of exactness); the comparisons of two numbers (integers exactly, others as doubles with IEEE 754's comparisons,
  as `xb_compare` and `xb_order_is` make them); `for.test` with an integer counter, limit and step; `index` and
  `index.set` of an array with an index within it, an integer or a truncated double (the array's `len` and `items`,
  `xb_array` in `xbrt.h`), where the store calls `xb_gc_write_item` only for a value of a heap type (`xb_type_is_heap`),
  and an index out of bounds calls the runtime, which raises the error. A comparison whose only use is the `truth` right
  after it gives that test its boolean directly, without the logical item. The bit operators (`&` `|` `^^` `<<` `>>`
  `~`) and the calls to a bit function (`BitAnd`, ..., the table `xb_bit_function` in `xbrt.h`) with number operands are
  the machine operation (`and`, `or`, `xor`, `shl`, `ashr`, `lshr`, a select for counts outside 0..63), a double taken
  as an integer by `llvm.fptosi.sat` (truncated, saturated, NaN 0, as `xb_dbl_to_i64`); other operands call the runtime
  operator or function, which raises the error. `=`, `==` and `!=` of two one-character strings compare their bytes in
  place (so `ValType( x ) == "C"` calls nothing). Unary `-` of an integer other than INT64_MIN or of a double is done in
  place too. `InlineCalls.cpp` does a few runtime functions in place when their arguments allow it, with their rules:
  `Len()` of a string or an array, `Asc()` of a string, `Chr()` of a number, `SubStr( c, n, 1 )` with `n` within `c`
  (the last two give the shared one-byte string, `xb_str_bytes`), and an `Asc()` whose argument is the `SubStr()` right
  before it, used nowhere else, reads the byte itself; `Int()` of an integer is it, of a double its truncation when that
  fits 64 bits (`xb_dbl_fits_i64`; else, NaN too, the runtime keeps a double); `PCount()` is the function's own `argc`
  (less `Self` in a method), and `PValue( n )` with an integer `n` copies the parameter's slot or the argument as passed
  (`NIL` outside 1 .. `argc`); `ValType()` of a value is the shared one-byte string of its tag's letter
  (`XB_TYPE_LETTERS`, the runtime's table too); anything else calls the function, which raises the errors, and so does a
  `PValue()` of an argument that is a reference. A call whose only argument is `...`, to a function that takes
  references, in a function with no other `...` and no `PCount()`/`PValue()`, passes the arguments where they are
  instead of copying them (`forwardInPlace_`). `append` copies a string's bytes into the room of the variable's growing
  string in place (one store for one byte) when they fit, and calls `xb_op_append` otherwise. A `load` that an `append`
  of its variable reaches with no other load in between ends the growth before the value goes on (`shareString`), unless
  only `Len()` and `Asc()` read its value, in place; a forward data flow finds them (`findSharingLoads`), where a block
  entered by unwinding (a `BEGIN SEQUENCE` handler, a `FOR EACH` cleanup) starts with every such variable growing, and a
  block reached growing along some edges only, from which every path ends the growth before any append (a loop that
  reads what a loop before it built), ends it on those edges instead, so the loop does not test it again; the edge of a
  `gen.yield` ends it before the frame is saved, since the next step enters through the dispatch. The bit operators and
  functions take two integers in two instructions and convert doubles on a path of their own, and the fast paths of
  calls read their operands in place: the argument group is built only for the call. Calls set the result slot to `NIL`,
  then pass `(argc, argv)` pointing at the argument slots; a call to an `EXTERN FUNCTION` goes to `xbf_NAME` or
  `xbp_NAME` (`NATIVE` or not), one to another module's function to `xbp_NAME`, one to a function of the module to its
  direct entry when it has one (below), else to it. A public function is `xbp_NAME` (prefixes in `xbrt.h`); a `STATIC`
  function, a codeblock and a module initialization are internal, under names nothing looks up, so modules sharing the
  object never clash. A call, a send or an array literal with `...` (`withRest`) passes the count and the items of a
  frame built for it on the stack ([runtime.md](runtime.md#calling-convention)); the inline calls and the bit functions
  leave such calls alone.
- Direct entries (`directEntries`, `xir::hasDirectEntry`): a function of the program that does not read its arguments as
  received (its frame keeps none, `readsArgs`, and it has no `...`, `usesRest`) and is not the body versioned for
  references (`byRefBody`) gets its body in an internal function, `xb.direct.NAME`, taking the context, the result slot,
  the caller's frame (the context's chain has it at the call: linking and unlinking the frame then read no memory, so a
  call does not wait for the store of the one before) and each parameter's item as two words (header and value) in
  registers. Calls of the module go there (`emitDirectCall`, `xir::isDirectCall`) with no `argc` or `argv` and no
  argument slots in the frame plan: each operand is read where it is and passed as words; a parameter not passed gets
  `NIL`, and arguments past the parameters are evaluated and left out, since nothing reads them. The function's symbol
  is a thunk (`genThunk`) that reads the items from `argv` (`argItem`) and calls the direct entry: the runtime, `Do()`
  and other modules call it. The body still tests its parameters for references, and its `call.forward` passes its
  parameters' slots as the arguments. Measured against calls through `argv` (callgrind, x86-64): `fib` 8% fewer
  instructions.
- Values held in frame slots (`Home::Kind::Slot`) keep their header and value words in registers, read once right
  after their definition (`cacheWords`; for a block parameter, at the start of its block): the frame is linked into
  the context, so LLVM must reload a slot after every call, though a slot keeps its value unchanged while the value
  lives. Type tests and integer reads use these words (`loadHeader`, `intValue`); pointers, which a moving collector
  may change, are still read from the slot. With the direct entries, against neither (callgrind, x86-64): `fib` 10%
  fewer instructions, `qsort` 17%, `AEval` 15%; `matmul` 2% more (its inner loop has no calls, and the words of the
  outer loops' counters take registers there).
- Classes ([runtime.md](runtime.md#classes)): a module emits an `xb_class_def` per class (`xb.module.classes`: its name,
  its parents (each one's name, and its entry when the module has the class), its variables with their flags, its
  methods with their functions and flags, its INIT function, `XB_CLASS_STATIC` for a `STATIC CLASS`) and registers them
  (`xb_classes_register`) with the module, with the class variables, the friends' names and the destructor. Every `send`
  calls `xb_op_send_from` with its own inline cache (`xb.send.cache`, a private zeroed `xb_send_cache`: three arrays of
  `XB_SEND_CACHE_WAYS` pointer-sized words) and, from a method or one of its codeblocks (`classIndex`, `superSend`), its
  class's descriptor (else a null one). `class.new` calls `xb_class_instance`.
- Memory variables ([runtime.md](runtime.md#memory-variables)): a module emits one `xb_memvar` per name its
  functions use (`xb.module.memvars`, numbered by the runtime when the module registers), and the `mv.*` operations
  call the runtime with it. A function (or codeblock) whose xIR may create a PRIVATE (`mv.set`, `mv.private`,
  `mv.param`, and the `macro.*` operations but `macro.public` and `macro.text`: `xir::createsPrivates`), or that calls
  a `PRIVATES` function (`xir::Function::callsPrivates`), takes the activation's mark in its prologue (`xb_mv_mark`)
  and releases to it in its exit block (`xb_mv_release`), the one returns and unwinding both reach; other functions do
  neither.
- Macros ([runtime.md](runtime.md#macro-operator)): `macro.get`, `macro.set`, `macro.private`, `macro.public` and
  `macro.text` call `xb_macro_get`, `xb_macro_set`, `xb_macro_private` (with the activation's mark),
  `xb_macro_public` and `xb_macro_text` with the text's item; `macro.call` puts the text and the arguments in
  consecutive slots (a list, as a send does) and calls `xb_macro_call`.
- Call stack ([runtime.md](runtime.md#frames)): the prologue stores the function's `xb_proc` (its
  `xir::Function::stackName`, its number of parameters, `XB_PROC_ARGS` when it keeps its arguments, `XB_PROC_HIDDEN`
  when it is hidden, `XB_PROC_METHOD` for a method (`xir::Function::method`, `method` in the xIR listing), and its
  source file name without directories, one string per module, none for a module of the runtime) in the frame. While an
  xIR instruction is lowered, every call the builder creates is marked with the instruction's source line
  (`IRBuilderCallbackInserter`, metadata `xb.line`); once the function is complete, `placeLines` stores that line in the
  frame right before each marked call that can see the call stack (any call but intrinsics and the runtime entry points
  that never raise an error or run code, `quietCalls_`), once per LLVM block and line. The fast paths of operations thus
  store nothing; their slow paths and calls do. Before `call.forward`, the function versioned for references clears its
  own `proc`: the other body shows instead.
- After every operation that can fail, the generated code loads `ctx->pending`; when it is set (marked unlikely) it
  calls `xb_pending_gc` (which may run destructors, and so collect) or, right after an operation whose error may
  substitute its result while that result has a `Local` home the collector does not see (`checkPending`, from the
  instruction being lowered), `xb_pending`, which never collects; if that asks to unwind, it goes to the LLVM block of
  the xIR block's unwind block, or leaves through the function's exit. A runtime operator may also return a value the
  error block substituted, of any type: the code after it reads the result's type instead of assuming one, unless
  `subst.check` follows the operation (its types then hold; the check reads the item's own tag and leaves for the
  generic copy on an unlikely edge, see [xir.md](xir.md#passes)), and a comparison that feeds a condition tests the
  truth of what its slow path returned. `truth` tests the item type inline; a non-logical value calls `xb_cond_error`
  and leaves.
- Variables: a captured variable's cell is loaded from `self->cells[]` (the `xb_block` layout is mirrored with the
  target `DataLayout`); a `FOR EACH` variable, a `STATIC`, a shared or captured variable or a parameter tests its type
  inline (one unsigned comparison covers `XB_T_CELL` and `XB_T_REF`) and follows a cell inline. An element
  (`XB_T_REF`, `xb_ref` and `xb_hash` mirrored like `xb_array`) is read in place when it is one of an array within its
  length, the entry at a position of a hash without holes, or the reference's own value for any other collection
  (`inlineRefTarget`); the rest calls `xb_ref_target`, which finds a hash entry by key or with holes and raises the
  bound error.
- `block` obtains the cells first (`xb_cell_of`: the variable's cell, made now if it has none, or a cell holding the
  current element of a `FOR EACH` variable), then creates the block.
  `ref` calls `xb_ref_of` (or builds the `XB_T_CELL` item of a captured variable's cell); `ref.index` calls
  `xb_ref_elem`. `each.enter` and `each.leave` call `xb_each_enter` and `xb_each_leave`; `each.move` stores the
  position in the loop's reference and calls `xb_each_move` only when the element is not one `inlineRefTarget` finds
  in place (a string's character, an enumerator's, a hash with holes, the bound error); `each.key` stores the position
  for anything but a hash, reads the key of a hash without holes in place, and calls `xb_each_key` for the rest.
- `barrier` ([runtime.md](runtime.md#garbage-collector)): after a store into a cell, the cell's GC state byte is
  tested inline and `xb_gc_remember` called for an old cell; after a store through an element reference,
  `xb_ref_written` is called. Frame slots and `STATIC` storage are roots and need nothing.
- Block parameters: item parameters live in slots, and branches copy their arguments in (through a staging buffer
  when a destination is also a source); machine parameters (`bool`, `i64`) become PHIs.
- A module with `STATIC`s gets an internal global `xb.statics` (an array of NIL items) and its initialization
  function; every module gets a global constructor `xb.module.register` that calls `xb_module_register` before
  `main` with them, its `INIT` and `EXIT` procedures (`xb.module.procs`) and its public functions by name
  (`xb.module.symbols`, `xb_symbol`). `CodeGen::run` adds
  one module to the LLVM module; several modules share it.
- `CodeGen::finish` adds `xb_app_main( ctx, argc, argv )` (unless `--no-entry`), which checks the ABI and calls
  `xb_program_run` with the start function (see [Sources and modules](#sources-and-modules)) and the command-line
  arguments the runtime passes it ([runtime.md](runtime.md#program-startup)).
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
| bit operators `&` `\|` `^^` `<<` `>>` `~` on numbers | an integer without a display width of its own (`xb_bit_apply`, the operands taken as integers by `xb_dbl_to_i64`) |
| comparisons of numbers or logicals | `.T.`/`.F.` |
| strings `+`, `-`, `==`, `$`; `=`/`!=` of two empty strings | the result (`=` on other strings depends on SET EXACT and is not folded) |
| `.NOT.`, `.AND.`/`.OR.` with a logical literal on the left, `IIF` with a logical literal condition | the result or the remaining operand |
| a call to a `FOLD` function with constant arguments | its result, from the evaluator table in `opt/Fold.cpp`: `Len` of a string literal (its length) or of an array literal of constants (its element count); the bit functions on number constants (an integer without a display width of its own, as the runtime makes it; arithmetic on such an integer gives one too) |

Division or `%` by a literal zero is not folded, so the error happens at run time. A folded integer is a program
constant (`xb_num_const_int_width`); a folded double has the default display width for its value. "SET DECIMALS
decimals" is resolved at run time: such a number is materialized by `xb_num_default_dec`, so `SET DECIMALS TO 4`
affects `10 / 4` as it does in Clipper. A logical literal on the right of `.AND.`/`.OR.` is not used to drop the left
operand, which keeps its side effects; Harbour drops it.

### Literals

Literals are constant globals in the object, so using one costs nothing at run time. A string literal is a static
`xb_str` (header, bytes, terminating NUL) flagged `XB_STR_STATIC`, whose `hash` field already holds `xb_hash_bytes` of
its bytes, so using it as a hash key computes nothing. An item literal is a structure whose value field has the type of
the member it represents, padded to the union size; the generator checks that its size and value offset match `xb_item`
for the target.

### Numeric literals

The display width of a numeric literal's integer part is fixed at compile time, as in Clipper. An integer literal is
a program constant: `xb_num_const_int_width` (10 columns plus one per digit beyond nine, 20 for -1000000000 or less;
leading zeros do not count). A double literal gets `xb_num_dbl_width` when it has fewer than 10 integer digits,
otherwise its own digit count. Literals too large for 64-bit integers become doubles.

## Object emission

`codegen/Emit.*` initializes every target LLVM was built with, creates the `TargetMachine` for `--target` (default: the
host), runs LLVM's default optimization pipeline for the `-O` level (new pass manager), then, from `-O1` on, splits the
loops whose fast paths have cold edges (`LoopSplit.*`, below) and runs the pipeline again when it did, and writes the
object with the target's object writer (code generation runs on the legacy pass manager: in LLVM 22 it is the one every
target implements). Code is position independent unless `--static` is given. The CPU is `generic` unless `--cpu` names
one, so programs run on any CPU of the target (a processor that keeps doubles with more precision than 64 bits between
operations, as some 32-bit processors do, can give a different last digit in rounding edge cases; xbase does not change
a processor's floating-point mode); `--cpu=native` (host target only) uses the CPU and features of the machine xbc runs
on.

Loops without slow paths (`codegen/LoopSplit.*`): every inline fast path keeps a call to the runtime for the other
cases, behind an edge weighted unlikely (`branchLikely`: edges to runtime calls, and to the rare double index; a choice
between two inline paths, such as an integer or a double operand, or a variable that holds a cell, is a plain branch).
The innermost loops with such cold edges, found once per function, are put in simplified and LCSSA form and cloned: in
the original, every cold edge goes to the same block of the copy, which runs the rest of the loop generically and
never comes back; `SSAUpdater` joins the values of both where the edges enter the copy, and the exits receive the
copy's values. The original keeps only the calls on its own paths (a `Len()` in a `FOR` limit stays), so the
function simplification pipeline, run again on the functions that changed, optimizes it as a fast loop: values keep
the types its fast paths give them, and loads can leave it. A loop of more than 2000 instructions or 64 cold edges is
left as it is, and the copies grow a function by at most its own size (`kMaxLoopInstructions`, `kMaxColdEdges`). The
module is verified after the split (an invalid module is an internal error).
