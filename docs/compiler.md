# Compiler (`xbc`)

Code: `compiler/src/`. The preprocessor is described in [preprocessor.md](preprocessor.md); the runtime contract the
generated code relies on is in [runtime.md](runtime.md).

Pipeline (`main.cpp`): preprocess `std.ch` and the source, parse, resolve calls, fold constants, run semantic checks,
generate xIR and verify it, optimize it (from `-O1` on, [xir.md](xir.md#passes)), lower it to an in-memory LLVM
module, optimize that, emit the object file. A stage that reports errors stops the compilation of that source; after
100 errors (`Diagnostics::kMaxErrors`) nothing more is reported and the preprocessor and the parser stop early.

## Sources and modules

`xbc` takes one or more sources (`xbc main.prg repository.prg`, `xbc *.prg`); each is a module, and a file given twice
is compiled once. A source whose file name contains `*` (any characters) or `?` (one character) and names no file stands
for the files of its directory that match it, sorted by name, except names starting with `.` unless the pattern does;
names compare without case on Windows. This serves shells that do not expand patterns. Each source is preprocessed on
its own, with `std.ch` and the command-line defines (its `#define`s and rules do not reach the next source), and goes
through the front end; a source with errors does not stop the others from being checked. Two modules defining the same
public function is an error. Then every module is lowered into one LLVM module and one object, where calls between the
modules are direct and LLVM can inline across them, which `xbc` links into the program (`-o`, by default the first
source's name; with `-c` the object only, by default the first source's name with `.o`); `-p` writes a `.ppo` per
source, `-E` prints them all, `--depfile` lists every file read. A module that calls a function of the runtime's sources
`xbc` carries (`driver/RuntimeLibrary.h`) has them compiled too, as modules of the runtime with `std.ch` and the
target's defines only, for its calls to inline ([xir.md](xir.md#passes)); they are not lowered.

A project file (`xbc <name>.xbprj`) gives the sources and options of a program or a library, which `xbc` then links or
archives (`-c`, `--library`, `--lib`, `-L`, `-l`, `--out-dir`: [projects.md](projects.md)); a library a module
uses (`--lib`) gives it the declarations of its public functions (made from the module file, before the module's
own).

The default include directories (`defaultIncludeDirs`), searched after the `-I` ones: where the install puts the
standard headers, by the path from `xbc`'s own folder to them (`XB_INCLUDE_FROM_BIN`, computed by the top-level
`CMakeLists.txt`: `../include` in a Windows package, `../include/xbase` in a system's install, so that an installed
`xbc` needs no `-I`), then, when `xbc` was built with `XB_SOURCE_INCLUDE` (the default), the source tree's `include/`
(`XB_DEFAULT_INCLUDE_DIR`: an `xbc` run from its build folder, as the tests run it; a package's `xbc` holds no such
path). The build itself names the source tree's headers with `-I` when it compiles the runtime's xBase sources
(`xb_compile_prg`).

The program starts at the public function `MAIN` of any module, or else at the first function of the first source that
has one (a `STATIC` function named `MAIN`, and `INIT` and `EXIT` procedures, do not count). Clipper starts at the first
function; xbase prefers `MAIN`, so that `xbc *.prg` works whatever order the files come in, and a source may define
helpers before `Main`. `--no-entry` leaves the entry out, for modules compiled on their own and linked into a program
whose entry is in another object.

## Grammar

`parser/` is a recursive descent parser over preprocessed statements: `Parser.cpp` (tokens, keywords, the module and
its functions, blocks), `Statements.cpp`, `Expressions.cpp`; `AST.h` holds the syntax tree.

Keywords are not reserved. A keyword is recognized at the start of a statement, except when:

- the next token is an assignment or access to a variable (`:=`, `=`, compound assignments, `++`, `--`, `->`, `:`, `[`),
  so `next := 1` and `exit = 2` assign variables; before `++` and `--`, `IF`, `WHILE`, `ELSEIF`, `CASE`, `RETURN`,
  `SWITCH`, `NEXT`, `END`, `ENDIF`, `ENDDO` and `ENDCASE` stay keywords, as in Clipper (`IF --n > 0` is an `IF`,
  `next++` an error), and `YIELD` when an operand follows (`YIELD ++n` yields, `yield++` increments a variable);
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
- Memory variables ([runtime.md](runtime.md#memory-variables)), found by name at run time: `MEMVAR name, ...` declares
  names (before the first function, for the whole file; in a function, before its statements);
  `PRIVATE name [:= expr], ...` and `PUBLIC name [:= expr], ...` (also `name[n1, n2]`) create them when they run, the
  statement's initializers all running first (they read the variables visible before it); `DECLARE` is Clipper's other
  spelling of `PRIVATE`; `PARAMETERS name, ...` creates PRIVATEs holding the arguments received, in order (the function
  keeps its arguments, as for `PCount()`), and may come before the `LOCAL`s. `M->name` and `MEMVAR->name` are a memory
  variable whatever else the name is. `RELEASE` and `CLEAR MEMORY` are commands (`std.ch`).
- Fields ([rdd.md](rdd.md#field-references)): `FIELD name, ... [IN alias]` declares fields as `MEMVAR` declares memory
  variables; `alias->name`, `FIELD->name` (`_FIELD->name`), `FIELD->alias->name`, `( expr )->name` and a macro before
  `->` (`&cAlias->name`, `&( expr )->name`) are fields (`FieldExpr`, assignable); a macro after the arrow
  (`alias->&cName`, `FIELD->&( expr )`, `M->&cName`, also after a macro alias) is the macro of the text they make
  (`parseAliasedMacro`); `alias->( expr, ... )` (also after `( expr )` or a macro, not after `FIELD->alias`) becomes
  `__AreaEval( alias, {|| expr, ... } )`.
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
  it. Not in a codeblock, nor in any part of a `BEGIN SEQUENCE` with `ALWAYS`, a `TRY` with `FINALLY` or a `USING` (a
  generator left before its end would never run that code); in another sequence an error of a later step reaches its
  `RECOVER` or `CATCH`, and between the steps the caller runs with its own error block. A generator takes no `...`,
  reads no arguments as received (`PCount()`, `PValue()`, `PARAMETERS`) and, unless it is `ASYNC`, creates no `PRIVATE`
  or `PUBLIC` (its steps run in its caller's environment). `YIELD` in an `ASYNC` function makes it an asynchronous
  generator ([concurrency.md](concurrency.md#language)).
- `ASYNC FUNCTION`, `ASYNC PROCEDURE`, `ASYNC METHOD` (also after `STATIC`, `INIT` or `EXIT`; a class declares
  `ASYNC METHOD name` and its implementation must be `ASYNC` too, and not `INLINE`) make an asynchronous function
  ([concurrency.md](concurrency.md#asynchronous-functions)); `AWAIT expr` is a unary operator, at the level of `-`
  and `++`, when an operand starts right after it (a name, a number, a string, a logical, `{`, `::`, `&` stuck to a
  name: the rule `isAwaitOperator` in `common/Token.h`, which the preprocessor follows too, so a command's expression
  marker takes `AWAIT f( x )` whole); otherwise, `AWAIT(` among them, it is a name. In an `ASYNC` function, `AWAIT` may
  be anywhere but a codeblock; elsewhere only in a function that may start the program (a public `MAIN`, or the first
  function of its module, `Sema::mayStart`), where it waits. An `ASYNC` function cannot `YIELD`, takes no `...` and
  reads no arguments as received (each reported once); its resume function creates its `PRIVATE`s in the function's
  own environment, with no mark of its own (`xir::Function::ownsPrivates`: none of its steps ends them). In a
  command, `AWAIT` followed by one of the words of the rule being matched is a name (`INDEX ON await TO x`,
  `Preprocessor::ruleWord`).
- `WITH OBJECT expr ... END` (`END WITH` too): inside, `:name` (also `:name( args )`, assigned, `++`) is a message to
  the object, evaluated once into a `LOCAL` of its own (`WithObjectStmt::hidden`, a name no program can write). A
  codeblock made there captures that variable, as it would any `LOCAL`: it sends to the object the statement holds,
  also after `END`, and to the next one when the statement runs again (the reference sends to the `WITH OBJECT`
  running when the block is evaluated). The innermost `WITH OBJECT` wins; `:name` outside one is an error.
- `TRY ... [CATCH [v] ...] [FINALLY ...] END` (`END TRY` too): a sequence whose error block is `{| e | Break( e ) }`,
  so an error reaches `CATCH` as its `Error` object and a `Throw( x )` as `x`; `FINALLY` is its `ALWAYS`. `RETURN`,
  `EXIT` and `LOOP` may leave its body and `CATCH` (`FINALLY` runs first), not `FINALLY`.
- `AWAIT FOR EACH [k =>] x IN expr ... NEXT` (`END` too), in an `ASYNC` function or the procedure that starts the
  program, not in a codeblock (else one error for the statement: its own `AWAIT`s are marked `AwaitExpr::generated`,
  which sema leaves to it; the body is checked still): the parser builds an `AwaitEachStmt`. Hidden `LOCAL`s (names no
  program can write) take, in order, `__AwaitEach( expr )` (the asynchronous enumerator), the values of `x` and `k` to
  put back and a position; then a sequence whose body is `DO WHILE AWAIT e:MoveNextAsync()`, `x := IIF(
  __AwaitEachAwaits( e ), AWAIT e:Current, e:Current )`, `k := __AwaitEachKey( e, ++position )`, then the body, and
  whose `ALWAYS` code puts `x` and `k` back on every way out, as `FOR EACH` does. That sequence is marked
  `SequenceStmt::restoresOnly`: its `ALWAYS` code only puts variables back, so `YIELD` may stand in the body of an
  asynchronous generator (one left there leaves them as they are, as `FOR EACH` does); in a synchronous generator (the
  start procedure) `YIELD cannot be inside AWAIT FOR EACH`. One variable (with its key) and one collection, neither a
  field; not `DESCEND`, whose elements would all be needed first.
- `USING [v :=] expr ... END` (`END USING` too): `expr` is evaluated once into a `LOCAL` of its own
  (`UsingStmt::hidden`, a name no program can write), then `v` (when written) is assigned from it and the body runs;
  however they end (their end, `RETURN`, `EXIT`, `LOOP`, a `BREAK`, an error, `QUIT`), `__Dispose()` sends the value
  `Dispose()` unless it is NIL (through a send cache of its own), and a `BREAK` or an error goes on after it. Not
  `YIELD` inside. `USING`, like `YIELD`, starts the statement unless the name is followed by what makes it a variable or
  a function (an assignment, `(`, `[`, `:`, `->`, the end of the statement).
  `std.ch` builds `SYNCHRONIZED` and `AWAIT SYNCHRONIZED` (`USING AWAIT oMutex:HoldAsync()`, the operand of `USING`
  an awaited value) on it ([concurrency.md](concurrency.md#threads)).
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
  function, no instance variables, nor inherited ones from a class of the module: an error; from a class of another
  module, the runtime's `BASE/3005` at its first message).
- `DO name [WITH args]`; empty arguments pass `NIL`. A variable written alone is passed by reference, as in Clipper;
  other arguments (an element, a variable in parentheses, an expression) by value. `DO &name [WITH args]` (also
  `&name.suffix` and `&( expr )`, which the reference lacks) calls the procedure the text names when it runs, as
  `&name( args )` does (`MacroCallExpr`), with the arguments by the same rule
- expression statements; a statement `operand = expr` is an assignment, and `=` inside `expr` compares:
  `x = y = 3` is `x := ( y = 3 )`, `a[1] = 5` assigns. The parser reports an invalid assignment target (anything but
  a variable, an element, an instance variable, a macro or a field, also in parentheses) here, for `:=`, `op=` and
  `++`/`--`, and anything but a variable or an element for `@`.

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
`@name[i]...` (or `@&name`, see the macro operator below) passes a variable or an element by reference (`RefExpr`), and
so does a message's argument (a method compiled from xBase receives the reference, as a function does; what takes no
reference, an instance or class variable assigned or a method of the runtime's own classes, takes its value:
`xb_args_values`); `@` anywhere else is an error (also in an array literal, by design: an element never holds a
reference, which keeps every array operation free of the test; the reference's live elements read through it but are
replaced by `:=`), and so are fields (`@alias->name`, and a name `FIELD` declares, also as the argument `DO ... WITH`
passes by reference or as the variable of `FOR EACH`). `@name()` where an operand starts is a function reference, a
value anywhere (`RefExpr` of a `CallExpr` with `reference` set and no arguments; what follows, such as `:Name`, applies
to the function reference, among call arguments too, where the reference implementation calls the function and sends the
message to its result; `@IIF()` is an error): it is lowered to `func.ref` with the function's callee, and the generator
emits its `xb_funcref` ([runtime.md](runtime.md#function-references)). `...` as the last argument of a call or a send or
the last element of an array literal (`SpreadExpr`; once, with nothing after it, not even an empty argument) stands for
the arguments the function received past its parameters, as many as there are: `f( a, b, ... )`, `o:m( ... )`, `{ ...
}`. Sema accepts it only in a function declared with `...`, outside its codeblocks (which have arguments of their own);
a `CHECKED` function's count includes the other arguments only, so only too many of them is an error. Variables,
elements (`a[i] := v`, `a[i] += v`, `a[i]++`), instance variables (`o:x := v`, a message without arguments) and macros
can be assigned. `LOCAL a[n1, n2]` (or `a[n1][n2]`) declares `a := Array( n1, n2 )`.

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
- A macro message: `o:&name`, `o:&name.suffix`, `o:prefix&name.`, `o:&( expr )`, also after `::`, `Super:` and the
  `:` of `WITH OBJECT` (`SendExpr::messageText`), with arguments (`...` too), assigned like any message (see
  [Syntax tree to xIR](#syntax-tree-to-xir)).
- `PRIVATE &name` and `PUBLIC &name` (also `&( expr )`, with an initializer or array dimensions) create the variable
  the text names. `@&name` (also `@&( expr )`, a call argument) passes a reference to what the text designates
  (`macro.ref`), `@&name[ i ]` one to an element of the macro's value.
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
EXTERN FUNCTION SubStr( cString AS STRING, nStart AS NUMERIC, [nCount] AS NUMERIC ) NATIVE RESERVED CHECKED ;
   ABBREV 4 FOLD RESULT STRING TOTAL
EXTERN FUNCTION QOut( ... ) NATIVE
```

Parameters are names, each with an optional `AS type`, the type of what it takes; `[name]` is optional. `[ ]` delimit
strings in xBase, so the parser receives a string and takes any string whose text is a name as an optional parameter (a
`.ppo` listing writes it as `"name"`, which compiles the same). Required parameters come first, and `...` allows any
number of further arguments. In a `NATIVE` declaration, `@` before a parameter (`@name`, `[@name]`, `@...`) means the
function receives a reference passed there (`Eval` passes them on to the block); a native function otherwise receives
the value of an argument written `@x`. An xBase function always receives the reference, so `@` in a declaration without
`NATIVE` is an error. Attributes:

| Attribute | Meaning |
|---|---|
| `RESERVED` | the program cannot define a function of this name (Clipper's predefined functions) |
| `CHECKED` | the argument count is checked at compile time, between the required and all parameters |
| `NATIVE` | written in C to the xBase calling convention: the call goes to `xbf_NAME`; without it the declaration names an xBase function of another module (`xbp_NAME`), or of a library (`LIBRARY`) |
| `ABBREV n` | calls may abbreviate the name to its first `n` or more letters (the keyword rule, `keywordMatches`) |
| `LIBRARY name` | a public function of the xbase library `name` (`ExternDecl::library`, `xir::Callee::library`, `Request::library`): the call, the function reference and the `REQUEST` go to `xbx_NAME$FUNCTION` ([projects.md](projects.md)); the declarations a program gets from the libraries it uses are such; two declarations of one name for different libraries conflict; not with `NATIVE` |
| `FOLD` | calls with constant arguments are evaluated at compile time (see [Constant folding](#constant-folding)) |
| `RESULT type` | the type of what a call that raises no error returns (`ExternDecl::result`, `xir::Callee::result`): type inference counts it ([xir.md](xir.md#types)); a type is `NIL`, `LOGICAL`, `INTEGER`, `DOUBLE`, `NUMERIC` (an integer or a double), `STRING`, `DATE`, `ARRAY`, `HASH`, `BLOCK`, `OBJECT`, `SYMBOL`, or several joined by `\|`, each named once (`NUMERIC` holds `INTEGER`), or `ANY` alone, any value (as no `AS`; `ValueType`, `Parser::parseValueType`); `INTEGER RANGE lo TO hi` (a `RESULT`'s only) holds the integers from lo to hi, which the inference's ranges take where no error substitutes the value |
| `TOTAL` | (with a `RESULT`) a call whose arguments are of their parameters' `AS` types (the required ones passed, none past them, no `...`) raises no error: its result has the `RESULT` type with no `subst.check` (`totalCall` in `xir/Types.cpp`); an error block may substitute any value for another call's. `tests/platform/declarations.c` calls every `NATIVE` function with a `RESULT` over a matrix of values of its parameters' types, where a `TOTAL` one raises no error, then of any type, where every call that raises none gives a value of its `RESULT` type, within its `RANGE` |
| `ARGS` | the function reads the arguments the function calling it received (`PCount()`): that function (or codeblock) keeps them in its frame (`FuncDecl::readsArgs`, `BlockExpr::readsArgs`, `xir::Function::readsArgs`; `args` in the xIR listing), as does one that runs a macro (`Sema::keepsArgs`) |
| `PARAMS` | `ARGS`, and the function reads and writes the parameters of the function calling it (`PValue()`): that function keeps them in its frame slots, as written (no parameter versioning, no SSA values; `FuncDecl::readsParams`, `BlockExpr::readsParams`, `xir::Function::readsParams`); a function or codeblock that runs a macro is taken as calling one (the text may call `PValue()`) |
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
a codeblock captures it or a reference is taken, at run time; a variable its blocks capture by value
([AST passes](#ast-passes)) stays in the frame.

`STATIC`s get slots in the module's static storage (`Module::statics`, `VarWhere::Static`). File-wide statics are
visible in every function; a function's statics are visible in that function only. They are never captured: codeblocks
use the storage directly. A `STATIC` initializer must be a constant after folding: a number, string, logical, `NIL`, an
array or hash literal of constants, or a codeblock whose variables are its own parameters and `STATIC`s (anything may be
called inside it). Anything else, a plain variable included, is "illegal initializer for STATIC". A `STATIC` is visible
in its own initializer, so a codeblock stored in it can call itself; a `LOCAL`'s initializer is resolved before the
`LOCAL` exists.

A name that is not a parameter, `LOCAL`, `STATIC` or capture is a memory variable (`VarWhere::Memvar`), as in Clipper;
read, it is a field of the current work area first (`fieldFirst`, the `mv.field` operation), unless a `MEMVAR` declares
it. A `FIELD` of a frame makes the name a field there (`VarWhere::Field`, with its `IN` alias), as `MEMVAR` does for
memory variables, and the two cannot name the same name. A `MEMVAR` of a frame (the file or a function) makes the name a
memory variable there, before a `STATIC` of an enclosing frame, and cannot name a `LOCAL`, parameter or `STATIC` of the
same frame; a function's `LOCAL` hides a file-wide `MEMVAR`. `PRIVATE`, `PUBLIC` and `PARAMETERS` do not declare (as in
the reference, a `STATIC` of the name stays the `STATIC`), and their names cannot be a `LOCAL` or parameter of the same
function. A name nothing declares (no `MEMVAR`, and no `PRIVATE`, `PUBLIC` or `PARAMETERS` earlier in the function) gets
a warning; `-Wno-undeclared` drops it and `-Werror=undeclared` makes it an error. A memory variable can be the
variable of a `FOR EACH` (a field cannot).

Sema reports: duplicate functions, parameters or locals, `EXIT` outside a loop or `SWITCH`, `LOOP`
outside a loop, a `LOCAL`, `STATIC` or `MEMVAR` that follows an executable statement or appears inside a nested block, a
non-constant `STATIC` initializer, a `SWITCH` `CASE` that is not an integer or string constant ("CASE requires either
numeric or string constant"), and a `CASE` value repeated in a `SWITCH`.

## AST passes

The syntax tree Sema resolved goes through passes of its own before code generation (`opt/AstPasses.*`, from `-O1`;
at `-O0` it is only verified). Each pass rewrites the tree or records facts on it that code generation reads; the
verifier (`verifyAst`) checks the tree after each one that changed it, and an invalid tree is an internal error naming
the pass: every variable reaches a slot of its frame (the function's, or its codeblock's), a capture of its codeblock or
a `STATIC` of the module, every capture of a codeblock one its parent has, the facts recorded fit the frames. Every pass
and the folder and call resolver go through the tree with one traversal, `parser/Walk.h` (the children of each node in
source order, each as the pointer that holds it, which a pass may replace). `--dump-ast=<file>` writes the tree after
the passes (`printAst`: a node per line, its children indented, a function's shared slots and facts on its line).
The carried runtime sources go through them too.

- `value-captures` (`opt/ValueCaptures.cpp`): a function's parameter or `LOCAL` that its codeblocks capture and that
  nothing assigns after its declaration (no assignment, `++`/`--`, reference `@x`, `FOR` or `FOR EACH`, `RECOVER USING`,
  in the function or in its codeblocks, nor `PValue()`: not a parameter of a function that calls a `PARAMS` function or
  a macro, `readsParams`) holds one value as long as a block can see it (a block captures only variables declared
  before it, whose initializers have run): `FuncDecl::capturedByValue`. A block made where the variable holds no
  reference gets a cell of its own holding the value (`block` with value cells, [xir.md](xir.md)): a `LOCAL`, a
  resumable function's parameter, or a parameter of the body versioned for arguments without references (whose
  `LOCAL` stands for it); the function keeps the variable in its frame (not shared), read directly, which
  `promote-variables` may then take. Not a codeblock's own variables (`Eval()` may pass references) nor the hidden ones
  of `WITH OBJECT`, `USING` and `AWAIT FOR EACH`, which their statements assign. A function returning `{| x | x + k }`,
  inlined into a loop that evaluates the block, runs in place (`inline-blocks` gives the value to the block's body): 12
  times fewer instructions than with the cell shared.

## Code generation

Code generation has two steps: the syntax tree becomes xIR ([xir.md](xir.md)), which becomes an LLVM module.

### Syntax tree to xIR

`xir/Gen*.cpp` (`xir::Generator`) emits each function's xIR in Clipper's evaluation order; `xir/Verifier.*` checks
the result before it goes further (an invalid xIR is an internal error). From `-O1` on the xIR passes run
([xir.md](xir.md#passes)); `--dump-xir=<file>` writes the xIR that is lowered to LLVM, with the inferred types of its
item values and block parameters when `--dump-xir-types` is given too ([xir.md](xir.md#types)).

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
  passed by reference. A macro message (`o:&name`, `o:&name.suffix`, `o:prefix&name.`, `o:&( expr )`, also after
  `::`, `Super:` and the `:` of `WITH OBJECT`; `SendExpr::messageText`, the text as a macro's) is `send.macro`: the
  text (once, also for `o:&c += v`), then the object, then the arguments, as in the reference; its text names the
  message when it runs.
- Variables: frame variables (parameters and `LOCAL`s), a codeblock's captures, and the module's `STATIC`s. A frame
  variable moves into a cell when a codeblock captures it (`block`) or a reference is taken (`ref`), as in Clipper, and
  a `STATIC` when a reference is taken or it is a `FOR EACH` variable; its own item then holds the cell (`Var::shared`
  marks the frame variables and the `STATIC`s that may: Sema records the `STATIC`s in `Module::staticsInCell`). A `FOR
  EACH` variable of a loop being generated is accessed through its reference; such a `STATIC`, a shared variable, a
  captured variable or a parameter is tested at run time for a cell or a reference (an argument passed by reference);
  other `LOCAL`s and `STATIC`s never hold one. An argument `@x` is `ref` (the variable's cell), `@a[i]` is `ref.index`.
  A store into a variable that may live in the heap (a cell, an element through a reference) is followed by `barrier`.
- Parameter versioning: a parameter may hold a reference only if its caller passed one, and one that is never shared
  cannot become one. A function with such parameters is generated twice: its own body starts with `params.ref`, which
  tests them; when none holds a reference, each is moved once into a `LOCAL` standing for it (which mem2reg makes an SSA
  value; the load is a `move`: the parameter's slot, which nothing writes, serves the value and holds it while the
  function runs, as its caller's argument did, so that an object is not destroyed while a function it was passed to, or
  its method, runs; `x := NIL` to the parameter lets go of it there, or, when a copy of the value is still live there,
  at a later release where it is dead (one that release is on every path to; where it is not, the parameter holds the
  value until the function returns): `frame.release param k`, `FramePlan::pinMovedParams`), and otherwise `call.forward`
  passes the arguments as received to the other body, `NAME.byref` (internal), which reaches the parameters through
  their references. Codeblocks inside are generated with each body.
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
  and the loop follows its cell from the moment one is made. A memory variable is a cell, its binding: the loop takes
  the one visible when it starts (`mv.ref`, error 1003 for none) into a frame variable of its own and works inside it,
  so every function, macro and codeblock that reads or assigns the name meets the element, a `PRIVATE` of the name made
  during the loop hides it as any other, and the variable is restored on every way out, as one others can see. In `FOR
  EACH k => v`, `k` is an ordinary variable, other than `v` (the parser rejects `FOR EACH x => x`): before each
  iteration `each.key` stores into it the key of the current hash entry, or the index for an array or a string, so
  assigning it changes nothing else, and after the loop it keeps the last key stored (the one `EXIT` left). A `k` that
  is the variable of an enclosing `FOR EACH` receives the keys as any assignment would: in that loop's element.
- `BEGIN SEQUENCE` gives the blocks of its body an unwind block, the handler: a `BREAK` (pending after an operation
  of the body, or a call) is taken there and `RECOVER` follows, with the `BREAK` value stored in the `USING`
  variable; other requests go on unwinding (see [xir.md](xir.md#control-flow-for-errors-and-exceptions)). `WITH`
  starts with `seq.with` and puts the error block back (`seq.restore`) at the end of the body and in the handler.
  `ALWAYS` code is generated where the body and `RECOVER` join, in an unwind block of its own (`seq.suspend` and
  `seq.value` before it, `seq.resume` after it, then onward), and before each `RETURN`, `EXIT` or `LOOP` that leaves
  the sequence (`Generator::exitCleanups`, which also puts the error block back), in a block that unwinds where the
  sequence does. `TRY` is the parser's `SequenceStmt` with the handler block built in; `USING` is a `SequenceStmt` too
  (the one statement of `UsingStmt::seq`) that the parser builds: the assignment of `v` from the hidden variable, then
  the body, leavable (`SequenceStmt::leavable`, as `TRY`'s), no `RECOVER`, and `__Dispose` of the hidden variable as
  its `ALWAYS` code.
- A generator (`FuncDecl::generator`, set by the parser; Sema adds the enumerator as parameter 0) becomes two xIR
  functions (`Generator::genGenerator`): the function itself, which returns `gen.new` of its resume function with its
  arguments, and `NAME.resume`, the body: it starts with `gen.restore` and a dispatch on the state it returns (0: the
  start; k: after the k-th `YIELD`), `YIELD` is the terminator `gen.yield` (the frame and the element saved, `.T.`
  returned) whose target is where state k goes on, and `RETURN` and the end are `gen.done` and `.F.`. Code generation
  (`FastPaths.cpp`) does `gen.restore` and `gen.yield` in place for a local enumerator whose frame holds every saved
  slot (its length at least the saved slots: every step after the first `YIELD`, and a first step whose arguments fill
  them): the saved slots (a resumable function's variables: its temporaries are dead across a suspension) copied item by
  item, inline, up to 16 of them, and with a constant length past that, the state read and set, the element stored, with
  the barriers (`xb_gc_write_items` for an old frame not recorded, the enumerator's state byte); a shared enumerator or
  a shorter frame calls `xb_gen_restore` and `xb_gen_yield`. `enum.len` of a local generator's enumerator that has not
  ended calls its resume function itself (`emitEnumLen`, as `xb_gen_move_next` does: a step that fails ends it with
  `xb_gen_done`), and `each.move` of a local loop reference over one copies its `Current` into the reference's own item;
  a string's length and a local array's or hash's are read in place; other collections and enumerators take
  `xb_enum_len` and `xb_each_move`. Inside a sequence `WITH` a handler (also `TRY`), the error block it replaced lives
  in a frame variable of its own (`Cleanup::oldBlockVar`, `Generator::hiddenVar`): before `gen.yield` each such handler,
  innermost first, is read (`seq.with` of NIL) and the replaced block put back; after it, outermost first, `seq.with`
  puts the handler back and stores the caller's block of the moment as the one to put back in the end. Then
  `makeResumable` (`xir/passes/Resumable.cpp`) moves every value whose definition does not dominate a use (one that
  lives across a `YIELD`) into a frame variable of its own, stored where it is computed and loaded before each use (a
  `Bool` or `I64` boxed with `box.bool`/`box.i64`); promote-variables and typed continuations leave generators alone.
  The dispatch goes only to the points a step can stop at: those of code nothing reaches (a copy of `ALWAYS` code after
  a `RETURN`) are left out with it.
- An `ASYNC` function is generated the same way (`Generator::genResumable`, `FuncDecl::async`, the task as parameter
  0): its entry returns `async.new` of its resume function with its arguments, its entry named (`Instruction::blockFn`,
  kept when the entry is inlined); the resume function starts with `async.restore` and the same dispatch, `RETURN`
  and the end return the task's result. `AWAIT expr` (`Generator::genAwait`) is `await.ready` of the value: when it
  holds, `await.value` at once; otherwise the handlers set aside as at `YIELD` (`setHandlersAside`), the terminator
  `async.suspend` (the task awaited, the next point), and in the block it resumes at the handlers taken back and the
  same `await.value`. Outside an `ASYNC` function it is `await.wait`. `AWAIT` of a call of one of the module's `ASYNC`
  functions (`fusedCallee`) or of a message evaluates the arguments, then `await.fuse` (the entry, none for a message),
  the call, and `await.fused`: when it holds, the call's value is the result itself; otherwise the `AWAIT` of that
  task. The entry is hidden from the call stack (the resume function shows, with its caller below in the first step).
- An asynchronous generator (`YIELD` in an `ASYNC` function) is generated as an `ASYNC` function but for its entry,
  which returns `agen.new` of its resume function with its arguments (`xb_agen_new`: the enumerator, nothing run; the
  entry is no `AWAIT`'s fused callee and shows on the call stack, as a generator's does), and its `YIELD`, the
  terminator `async.yield` (`xb_async_yield`: the frame saved in the task, the element in the enumerator; point k goes
  on at the next `MoveNextAsync()`). `RETURN` and the end return: the task ends, which ends the enumeration.
- A codeblock becomes an xIR function of its own (`<function>.blockN`) whose frame holds its parameters; `block`
  creates it with the cells of the variables it captures (moving them into cells if they are not yet).
- `STATIC` initializers form the module initialization function.
- Code after `RETURN`, `EXIT` or `LOOP` lands in blocks nothing reaches, removed once the function is complete.

### xIR to LLVM

`codegen/` lowers each xIR function to an LLVM function with the dynamic calling convention described in
[runtime.md](runtime.md#calling-convention), which is, for a function with a direct entry (below), a thunk in front of
it: `CodeGen.cpp` (types, runtime declarations, the module, functions and frames), `FramePlan.*` (where each item
lives), `Instructions.cpp`, `Variables.cpp` (variable addresses, references, write barriers), `Literals.cpp`; `Emit.cpp`
emits the object file. `Branches.h` names the branch instructions for the passes on LLVM 23 (`CondBrInst` and
`UncondBrInst`, `BranchInst` deprecated), which the release packages are built with, and 22 (one `BranchInst`): both
build xbc. A function of a carried runtime source (`Function::carried`) is no function of the module: it is declared by
its library symbol (`functionOf`, `librarySymbol`: `XB_LIBRARY_FUNC_PREFIX`, the module's name and its own), which a
module of the runtime defines with external linkage for each of its other `FUNCTION`s, `PROCEDURE`s and methods (its
codeblocks stay its own); `scalar.plain` reads `xb_scalar_replaced` at the value's type, unordered, and tests the type
against its mask; `object.is` tests the item's type for `XB_T_OBJECT`, then the object's class (`xb_object.cls`) against
the `runtime` field of the class's `xb_class_def` (written once, when the runtime prepares the class, before any object
of it exists: the load is unordered and `!invariant.load`, a value read before, null, only failing the test), and the
branch on it, or on the "or" of several, is likely (`classTest`). `stampSources` stamps those texts
(`Module::sourceHash`, the FNV-1a 64 the driver takes of each): a module of the runtime defines its stamp
(`sourceStamp`), and a module given the carried sources references all of theirs from a table the linker must resolve
(`keepReferences`, as `EXTERNAL`'s requests).

- Frame: `FramePlan` gives every live item value a frame slot (a GC root): each value's liveness is found by walking
  from its uses back to its definition, slots are assigned by linear scan and reused once free. A dead value's slot
  keeps what it held until another value takes it, so where a loop exits, the block it goes to first sets to `NIL` the
  slots of the loop's values that are dead there and that no live value holds (`clearsAtStart`; see `inline.leave` below
  for an inlined body): what only they held can be collected, and its destructor run. When other blocks reach that block
  too (the header of a loop that follows, say), the loop's branches to it clear them on their edge instead
  (`clearsOnEdge`), so the clears run once per exit, not on every pass of the next loop. `x := NIL` to a local or a
  parameter (the program lets go of what x held: an array, an object whose destructor should run) is followed by a
  `frame.release` (`Generator::genAssign`), which sets to `NIL` the slots of every value written before it and dead
  there, by precise liveness (`liveAtRelease_`: a use after it in its block, or the value live into a successor), not by
  the intervals, which span the blocks laid out between a definition and its last use; a slot an argument group holds is
  kept while that group's call is being made, or while its inlined body is open (`walkBodies`), not along the body's
  copies laid out later (`FramePlan::clearsAt`). A direct call (`CodeGen::emitDirectCall`) sets to `NIL` the slot of
  each argument it uses last (`FramePlan::lastUse`) once it has read the argument's words, so the callee alone holds
  what only the argument held, and a group (a call's arguments, an array literal's elements) does so once it has copied
  them all (`CodeGen::operandGroup`). A call or send through the dynamic entry (`argv`: a method, a function of another
  module, the runtime's library) lends its group to the callee for the call (`xb_ctx.lent`, `CodeGen::lend`): when one
  of the arguments past a send's object may refer to a heap object (`CodeGen::lends`), the entry of a function with a
  direct one (`CodeGen::takeLent`) reads the words of its parameters' arguments (Self included), sets those items to
  `NIL` and ends the loan, its parameters then holding them until it returns; a function without one (it reads its
  arguments as received, passes `...` on, or is a `NAME.byref` body) ends it in its prologue and keeps them, as natives
  and codeblocks do (a codeblock's arguments are its caller's: `Eval()` passes them on). At -O0 a caller's `LOCAL` is a
  frame variable and keeps what it holds while the function runs; at -O1 and above one that mem2reg made a value is let
  go once it is dead, as any value (a destructor may then run earlier than at -O0, inside the callee that drops it). The
  parameters take slots 0 to n-1 (the prologue copies them there), but in a typed copy with a direct entry and no loop a
  parameter of types without heap references that nothing writes (`FramePlan::inRegisters`): it has an item of its own
  outside the frame, as a `Local` value (the words its caller passed, in registers), since nothing reads a typed copy's
  parameters in its frame (it reads no arguments as received and forwards none); a function with a loop writes them,
  since there a value held through the loop keeps a register from it (out of the frame, `fib`'s parameter saves 1% of
  its instructions; `fasta`'s, held through its loops, cost 1%); then the frame variables still in memory (those mem2reg
  did not make values), then temporaries. The arguments of a call and the elements of an array literal take consecutive
  slots, and a value used only there (a block parameter too) is computed straight into its slot; a branch argument used
  only there shares the slot of its block parameter (on a loop's back edge, when the jump is the block's only way out
  and nothing after the argument reads the parameter); a `load` whose single use follows with nothing in between that
  can write the variable (for a variable others can reach: nothing that writes, runs code, or goes on after an error
  block ran) uses the variable's own item (no copy), or through a cell or a reference what is in place (a local cell's
  value, a loop variable's element of a local collection or its reference's own item), else a shadow slot of its own
  into which the runtime copies the value (`FramePlan::shadowSlot`: the shadows come after the other slots, and since
  none is live across a call or a yield; a resumable function's step keeps only its variables' slots, `savedSize`, since
  the values that live across a suspension are in its variables and its temporaries are dead there), and so does every
  load of a frame variable nothing writes once the function runs (`FramePlan::findUnwritten`: a parameter the body
  versioned for references leaves to the other body, a `LOCAL` never assigned); a value used only by the `store` right
  after it is computed into the variable when the store writes the variable's own item (a store through a cell or a
  reference may find a shared cell or an element, which the runtime writes whole), and one used only by `ret` into the
  caller's result slot (not in a direct entry that returns its result's words in registers, below: it has no such slot).
  A call's result is computed straight only into a plain frame variable (the callee must not see a `STATIC` cleared),
  and an operation that can fail is not computed in place in a block that unwinds (the handler may read the variable,
  and a call may clear its result first). Two edges from one block to the same block do not share their arguments'
  slots. A value that never refers to a heap object (`xir::TypeInfo`, [xir.md](xir.md#types)) and is not computed in a
  slot as above is `Local`: an item of its own outside the frame (an `alloca` reached only by loads and stores, which
  LLVM keeps in registers). Block parameters are chosen first, other values after the groups and the parameter slots.
  Runtime calls never receive a `Local`'s address: an operand goes through a scratch item, and a result is written there
  and copied back (`callArg`, `callResult`). When type inference finds a single possible type for an operand (`typeOf`),
  the fast paths take its tag as a constant, and LLVM removes the tests and the paths of other types: arithmetic on
  values known to be doubles is plain floating-point code. A fast path the operands' types exclude is not emitted at all
  (`emitBinary`: two integers, two doubles, an integer with a double, two one-byte strings for an equality): an
  operation on strings, or on a function's result of known types, is its runtime call alone.
- Alias information (TBAA): every load and store of generated code whose memory is known is tagged with its class,
  classes that never overlap (`CodeGen::Mem`): items outside the heap (an `alloca` or a global of the module: frames,
  `Local` and scratch items, `STATIC` storage, literals), items in the heap (addresses from `heapItem`: elements, cells,
  reference targets), array headers, codeblocks' cell pointers, the context's head, frame links, the collector's state
  bytes, strings and their buffers (lengths, flags and bytes, which compiled code reads, a view's through its buffer's
  address, and the flags, where it clears `XB_STR_APPEND`), objects' class pointers (`xb_object.cls`, which a send's
  in-place path reads) and a `FOR EACH` reference's position (`xb_ref.index`, which `each.move` writes: a string's
  length and bytes stay in registers across it). An address that may be either kind (a variable reached through a
  possible reference, an argument such as `argv` or the result pointer) is not tagged. LLVM then knows that a store into
  an element changes no frame slot and no array header; by itself this changes little in the code today, and it is the
  ground for moving array headers out of fast loops.
- Items are stored and copied as two 8-byte words, the header (type, width, decimals; `headerWord`, in the target's byte
  order, as `xb_header_word` in `xbrt.h`) and the value, and fields are read out of the header word (their bit positions
  come from the target's layout, `headerShift`): a load is served by the store that wrote it (store forwarding) only
  when that store covers it. Between items in memory the value word is read by an unordered atomic load where 64 bits
  are a native integer of the target (`DataLayout::isLegalInteger`; a plain load there), which code generation does not
  merge with the header's into one 16-byte access spanning the two stores that wrote the item, as it merges two plain
  loads (`qsort`: 4.5 million blocked forwards); on a 32-bit target it is a plain load (two 32-bit ones), where an
  atomic one would call the atomics library (on x86, whose items are 4-byte aligned, a call that locks a mutex) or take
  an exclusive access (ARM). A copy of a literal item stores its two words as constants (`literalWords_`; a string
  literal's only on targets where its pointer fills the value word, a narrower one being widened by an instruction): a
  choice between two literals (`iif( l, "T", "F" )`) is then one between constants, where loads would become branches in
  code generation. An integer literal whose display width is the one a computed integer of its value shows
  (`xb_num_int_width`: 10 from -999999999 to 999999999, 20 below) is stored with width 0, as computed integers are (the
  runtime reads both alike): a loop's variable that starts as `0` and is then computed keeps one header word, whose type
  LLVM then knows.
- The entry block allocates the frame of the planned size on the stack (`pushFrame`: an `alloca` of `xb_frame`, see
  [runtime.md](runtime.md#frames)), copies the parameters into the first slots (in a direct entry, from the words in
  registers; otherwise branch-free from a selected source, `argv[k]` or a `NIL` constant: `argItem`), clears the other
  slots, stores the arguments as received (`argv`, `argc`) when the function reads them (`readsArgs`) and links the
  frame into the context; every `ret` writes the result (`NIL` without a value: a direct caller does not clear it
  first; a direct entry that returns words writes an item of its own, `NIL` from the entry on, whose words the exit
  returns). The exit block unlinks the frame and returns.
  Blocks are laid out in reverse postorder visiting successors last first, so a block's first successor (a loop body, a
  THEN branch) follows it and exits come later.
- Operations call the runtime (`xb_op_*` and the others); the result may alias an operand. `FastPaths.cpp` inlines the
  common cases first: `/` and `%` of two numbers with a divisor other than 0 (a double with SET DECIMALS decimals, read
  from the context's head; `%` of two integers is their remainder, 0 for a divisor of -1, and of doubles that hold
  integers below 2^63 in magnitude (which convert to 64-bit integers exactly) their integer remainder with the
  dividend's sign, fmod's exactly, without calling it: `doubleRemainder`); `+`, `-` and `*` of two integers with LLVM's
  overflow intrinsics (an overflow, like any other operand types, calls the operator, which makes a double); `+`, `-`
  and `*` of two numbers of which one is a double, in doubles (an integer beyond 2^53 in magnitude, which a double
  cannot hold exactly, goes to the runtime, so that every target converts it as the runtime does), with the decimals the
  runtime gives (the larger for `+` and `-`, the sum for `*`); two doubles, the usual case of these paths, take a path
  of their own (their bits and decimals as they are, no test of exactness). Of operands that may be two integers and
  may be two doubles, the path of an integer with a double is cold (`splitColdLoopPaths` leaves the fast loop for it),
  unless strings may come too, an operand is a block parameter (a value carried around a loop changes from an integer
  to a double once, `s := 0` then `s := s + x` of doubles, and each entry of the loop would leave it) or a call's
  result (its loop makes the call: there is no fast loop to keep, only code to add); the comparisons
  of two numbers (integers exactly, others as doubles with IEEE 754's comparisons, as `xb_compare` and `xb_order_is`
  make them); `for.test` with an integer counter, limit and step; `index` and `index.set` of a local array with an index
  within it, an integer or a truncated double below 2^63 (the array's `len` and `data`, `xb_array` in `xbrt.h`). One
  test of the array's state byte (`isLocalItems`: neither shared nor of a compact form, the usual case) leads to the
  path of items, where the store calls `xb_gc_write_item` only for a value of a heap type (`xb_type_is_heap`) into an
  array that is old and not recorded (`needsRecord`, `xb_gc_needs_record` on the state byte, read again once the value's
  type is a heap one: kept live from before the store, it cost loops a register), and makes no test at all when type
  inference finds the value refers to no heap object (`mayReferToHeap`; so do `object.set` and a send's in-place write,
  `storeObjectVariable`); a shared array's items are the runtime's to read and write whole, and an index out of bounds
  calls the runtime, which raises the error. A local array of a
  compact form ([runtime.md](runtime.md#values)) has paths of its own, after a branch that is no cold path (`kFormTest`,
  below), for one form each, which one test of the form (a compact form is a local array's) selects: a read inlines the
  logicals' when a condition or `.NOT.` tests the element (`readAsLogical`), the integers' otherwise, and gives an item
  of that one type (each inline path joins the path of items and costs it registers: three of them made `matmul`'s loop
  16% slower); a store the logicals' for a value that can only be a logical or `NIL`, the integers' for any other, and
  writes its byte or word in place for a value whose header word the form holds. Everything else goes to the runtime, as
  cold paths: another form, an array of `NIL`s (a new one: its first store gives it a form), a `NIL` among logicals or
  integers on a read, a value the form does not hold on a store (the runtime turns the array). So no fast loop changes
  an array's form, and a loop on an array it does not change is versioned on the form (below). A read whose every use
  indexes the element (a row of an array of arrays: an element of a compact form would be an error there, `onlyIndexed`)
  has no compact path at all. A comparison whose only use is the `truth` right after it gives that test its boolean
  directly, without the logical item, and so does an element read so tested on the path of an array of logicals
  (`findFusedComparisons`: its slot is written on the other paths only, which test the item). The bit operators (`&` `|`
  `^^` `<<` `>>` `~`) and the calls to a bit function (`BitAnd`, ..., the table `xb_bit_function` in `xbrt.h`) with
  number operands are the machine operation (`and`, `or`, `xor`, `shl`, `ashr`, `lshr`, a select for counts outside
  0..63), a double taken as an integer by `llvm.fptosi.sat` (truncated, saturated, NaN 0, as `xb_dbl_to_i64`); other
  operands call the runtime operator or function, which raises the error. `=`, `==` and `!=` of two one-character
  strings compare their bytes in place (so `ValType( x ) == "C"` calls nothing). Unary `-` of an integer other than
  INT64_MIN or of a double is done in place too. `Eval( x, ... )` of a value that may be a codeblock calls the block's
  function itself, as `xb_block_call` does (with no test when it surely is one; another value calls `Eval()`).
  `InlineCalls.cpp` does a few runtime functions in place when their arguments allow it, with their rules: `Len()` of a
  string or a local array, `Asc()` of a string, `Chr()` of a number, `SubStr( c, n, 1 )` with `n` within `c` (the
  position and the count any numbers, as `xb_num_to_i64` takes them: those `%` and `/` make are doubles) (the last two
  give the shared one-byte string, `xb_str_bytes`), the other `SubStr()`, `Left()` and `Right()` of a plain string (not
  a memo field's value, whose item the function keeps for a result of all of it) and numbers through their typed entries
  (`xb_str_sub`, `xb_str_left`, `xb_str_right`: no arguments' group, no checks, no error to test for), and an `Asc()`
  whose argument is the `SubStr()` right before it, used nowhere else, reads the byte itself (a view's in its buffer:
  `strData` tests `XB_STR_VIEW` as `xb_str_data` does); `Int()` of an integer is it, of a double its conversion when it
  lies in [-2^63, 2^63) (truncated, as `xb_dbl_fits_i64` of its truncation would allow), with the double's width, its
  own or else the default one (`xb_num_dbl_width`'s rule, from the constants `XB_NUM_FITS_MAX`, `XB_NUM_FITS_MIN`,
  `XB_NUM_WIDTH`, `XB_NUM_WIDE_WIDTH` of `xbrt.h`); the rest (NaN too) calls the runtime; `Int( x / y )`, whose division
  is right before it and used nowhere else, is lowered as one (`emitIntOfQuotient`): two integers in [-2^52, 2^52), `y`
  not 0, give their truncated integer quotient (`sdiv`, which the double quotient truncated equals) with the width
  `Int()` would give that double, unless the quotient is `XB_NUM_FITS_MAX` or `XB_NUM_FITS_MIN`, where the double may
  lie past the bound; anything else is the division (`emitDivMod`) and then `Int()` as above; `Abs()` of an integer not
  below 0 is it, of a negative one (not INT64_MIN) its negation, of a double its magnitude with its decimals; `Max()`
  and `Min()` of two numbers (compared as doubles) or two dates copy the first when it wins or ties, else the second;
  `Round( n, d )` with an integer `d` within 22 places is `xbf_ROUND`'s formula (a table of the exact powers of ten,
  `fma`, `XB_DBL_INTEGRAL`; the truncation `truncDouble`, through an integer below 2^52, since `llvm.trunc` is a call of
  the C library where the target lacks the instruction, as x86-64 without SSE4.1 does, and so is `doubleRemainder`'s
  test of integral operands); `Sqrt()` of a number is `llvm.sqrt` (IEEE's, the same everywhere), `Exp()` and `Log()`
  call the runtime's `xb_exp` and `xb_log` (`xbmath.h`, the same on every system, not the C library's `llvm.exp` and
  `llvm.log`; declared without memory access, so that LLVM still folds repeated calls), each with Clipper's results
  outside the reals and SET DECIMALS decimals; `Mod()` of two numbers and a base other than 0 is `doubleRemainder` moved
  to the base's sign; outside an inlined body, `PCount()` is the function's own `argc` (less `Self` in a method), and
  `PValue( n )` with an integer `n` copies the parameter's slot or the argument as passed (`NIL` outside 1 .. `argc`);
  `ValType()` of a value is the shared one-byte string of its tag's letter (`XB_TYPE_LETTERS`, the runtime's table too;
  a memo field's value, `XB_STR_MEMO`, calls the function); anything else calls the function, which raises the errors,
  and so does a `PValue()` of an argument that is a reference. A call whose only argument is `...`, to a function that
  takes references, in a function with no other `...` and no `PCount()`/`PValue()`, passes the arguments where they are
  instead of copying them (`forwardInPlace_`). `append` copies a string's bytes (a view's from its buffer; the
  variable's growing string is never a view) into the room of the variable's growing string in place (one store for one
  byte; up to 16 bytes two loads and two stores of 8, 4, 2 or 1 bytes overlapping in the middle, `copyBytes`, with no
  call; more through `memcpy`) when they fit, and calls `xb_op_append` otherwise. A `load` that an `append` of its
  variable reaches with no other load in between ends the growth before the value goes on (`shareString`), unless only
  `Len()` and `Asc()` read its value, in place; a forward data flow finds them (`findSharingLoads`), where a block
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
  the caller's frame (the context's chain has it at the call: linking needs no load of it, so a call does not wait for
  the store of the one before) and each parameter's item as two words (header and value) in registers. Calls of the
  module go there (`emitDirectCall`, `xir::isDirectCall`) with no `argc` or `argv` and no argument slots in the frame
  plan: each operand is read where it is and passed as words; a parameter not passed gets `NIL`, and arguments past the
  parameters are evaluated and left out, since nothing reads them. A function whose `RETURN`s give no value that refers
  to a heap object (numbers, logicals, dates, `NIL`: `Function::returnTypes`, [xir.md](xir.md#types); `returnsWords`
  in `FramePlan.h`), and not only `NIL` (a procedure's, which its callers seldom read, gains nothing so), takes no
  result slot: it returns its result's two words in registers (`{i64, i64}`), `NIL` when it
  unwinds, and the caller stores them into the call's home (in registers too when that is `Local`) before its pending
  test. Its call is computed straight into a variable only in a block that does not unwind (above), where nothing reads
  the variable again when the call unwinds. Any other result may refer to an object that only it holds, which the
  collector must see on the way back: the callee writes it into the caller's item. The function's symbol is a thunk
  (`genThunk`) that reads the items from `argv` (`argItem`), calls the direct entry and writes a result returned as
  words to its `ret`: the runtime, `Do()` and other modules call it. The body still tests its parameters for references,
  and its `call.forward` passes its parameters' slots as the arguments. Measured against calls through `argv`
  (callgrind, x86-64): `fib` 8% fewer instructions.
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
  calls `xb_op_send_from` with its own inline cache (`xb.send.cache`, a private zeroed `xb_send_cache`:
  `XB_SEND_CACHE_WORDS` pointer-sized words) and, from a method or one of its codeblocks (`classIndex`, `superSend`),
  its class's descriptor (else a null one). Before that call, a send without `...` performs the cache's newest way in
  place when it holds the class of the object sent to (`sendInPlace`, as `perform` does on a hit): the object as itself
  (type `XB_T_OBJECT`, width 0) and the way read under the cache's sequence lock (an odd version, or one changed
  meanwhile, misses); an instance variable of a local object is read (`XB_SEND_GET`) or, for a send with one argument,
  written (`XB_SEND_SET`, followed by the write barrier on the object's state byte, read once as `xb_object_put` does),
  and a method (`XB_SEND_METHOD`) is called through its function, unless an AWAIT fused the send (the runtime then hands
  the method to the AWAIT). The written value's words are read before the cache's acquiring loads, which would keep them
  from being forwarded from the stores that made them. Anything else, a shared object and every miss call
  `xb_op_send_from`, which fills the cache. `xb_object` is part of the ABI for this (`cls` and `vars`, checked by
  `xb_abi_check`). A send with an argument by reference is not written in place (the runtime assigns the value it
  designates). `object.get` and `object.set` (a send resolved for the object's known class) read and write a local
  object's variable in place (`emitObjectAccess`: the state byte read once for sharing and the barrier, the write
  followed by the barrier), and call `xb_op_object_get` and `xb_op_object_set` for a shared one. Object variables are
  read as `copyItem` copies an item (its value word not merged with the header's into one 16-byte load) and written with
  two 8-byte stores, so that a read after an assignment is forwarded from its stores. `send.macro` calls `xb_macro_send`
  the same way, without a cache. `class.new` calls `xb_class_instance`.
- Memory variables ([runtime.md](runtime.md#memory-variables)): a module emits one `xb_memvar` per name its
  functions use (`xb.module.memvars`, numbered by the runtime when the module registers), and the `mv.*` operations
  call the runtime with it. A function (or codeblock) whose xIR may create a PRIVATE (`mv.set`, `mv.private`,
  `mv.param`, and the `macro.*` operations but `macro.public` and `macro.text`: `xir::createsPrivates`), or that calls
  a `PRIVATES` function (`xir::Function::callsPrivates`), takes the activation's mark in its prologue (`xb_mv_mark`)
  and releases to it in its exit block (`xb_mv_release`), the one returns and unwinding both reach; other functions do
  neither.
- Fields ([runtime.md](runtime.md#work-areas)): `field.get` and `field.set` call `xb_field_get` and `xb_field_set`
  with the alias's item (`( expr )->name`) or its name as a C string (`aliasName`: null for the current work area),
  the field's name and a cache of the reference's own (`fieldCache`, a zeroed private 64-bit global aligned to 8
  bytes); `mv.field` calls
  `xb_mv_field` with the name's `xb_memvar` and a cache.
- Macros ([runtime.md](runtime.md#macro-operator)): `macro.get`, `macro.ref`, `macro.set`, `macro.private`,
  `macro.public` and `macro.text` call `xb_macro_get`, `xb_macro_ref`, `xb_macro_set`, `xb_macro_private` (with the
  activation's mark), `xb_macro_public` and `xb_macro_text` with the text's item; `macro.call` puts the text and the
  arguments in consecutive slots (a list, as a send does) and calls `xb_macro_call`.
- Stack limit ([runtime.md](runtime.md#frames)): once the frame is linked, the prologue compares its address with the
  context's `stack_limit` and, below it (an unlikely branch), calls `xb_stack_deep` with the frame's address as a number
  (a deep stack's mark: it goes on; declared to read and write the context alone, `memory(argmem: readwrite)`, so that
  on the way back into the body only what the function read from the context is read again, and it inlines as without
  the mark) and otherwise `xb_stack_overflow`, and branches to the exit, before the body (`checkStack`); a list built
  for `...` is tested the same way before it is written, restoring the stack and unwinding when it is below the limit.
- Call stack ([runtime.md](runtime.md#frames)): the prologue stores the function's `xb_proc` (its
  `xir::Function::stackName`, its number of parameters, `XB_PROC_ARGS` when it keeps its arguments, `XB_PROC_HIDDEN`
  when it is hidden, `XB_PROC_METHOD` for a method (`xir::Function::method`, `method` in the xIR listing), and its
  source file name without directories, one string per module, none for a function of the runtime: `Function::runtime`,
  of a module of the runtime or a carried source) in the frame. While an xIR instruction is lowered, every call the
  builder creates is marked with the instruction's source line (`IRBuilderCallbackInserter`, metadata `xb.line`); once
  the function is complete, `placeLines` stores that line in the frame right before each marked call that can see the
  call stack (any call but intrinsics and the runtime entry points that never raise an error or run code,
  `quietCalls_`), once until another line is needed: a forward data flow over the function's blocks finds where every
  way in stored the same line into the same frame last, and a block that starts so stores none for its calls of that
  line (`Fib( n - 1 ) + Fib( n - 2 )` stores its line once; only the function's own code writes its frames' lines, and a
  frame's link, which writes line 0, counts as a store); not in the frame of a function of the runtime, which shows line
  0 (`showsLines`: the innermost frame's function). The fast paths of operations thus store nothing; their slow paths
  and calls do. Before `call.forward`, the function versioned for references clears its own `proc`: the other body shows
  instead. `inline.enter` links a frame without slots that shows the inlined function (an `alloca` of the entry block,
  one per nesting depth: bodies open at once are at different depths, and the copies of a body share it), after storing
  the line of the call it replaces in the frame below (unless that frame is a function of the runtime's); until the
  matching `inline.leave`, which links the frame below again, calls take it as the caller's frame and their lines are
  stored in it. A body that reads the arguments it received gets them in consecutive slots of the frame (a group, as a
  call's: `inline.enter`'s operands, kept until the body's last instruction, `FramePlan::extendArgumentGroups`), which
  the inlined frame's `argv` and `argc` give; `PCount()` and `PValue()` inside an inlined body call the runtime, which
  reads that frame. Any other body holds its arguments in place: each argument's value that has a slot of its own lives
  to the body's last instruction (`FramePlan::heldArgs_`, extended with the groups), as the callee's parameters would
  hold it until it returns; a `frame.release` of a parameter's `LOCAL` in the body lets go of that argument there unless
  the caller still uses it or another parameter, of that body or of an outer one, holds it too (a value so held keeps a
  slot of its own: no block parameter shares it, no direct call or group it is passed to while the body runs lets go of
  it, and a load of it is not served by its variable, which the body may write); a codeblock literal is passed as `NIL`
  (folding may leave it unmade). Once that release is on every path, later ones let go of the argument where it is dead
  (a copy kept it). A body that reads its arguments as received keeps them as received, as the function called would. A
  `frame.release` of the function's own parameter that no load reads any more (its `move` gone) clears the parameter's
  slot. The bodies open where each block starts are those of the `inline.enter`s on the way to it (`openInlineBodies` in
  `xir/Analysis`: an inlined body is entered at its start and left at its end). That store of the line counts as one
  `placeLines` made. `inline.leave` first sets to `NIL` the slots of the body's values that are dead there and that no
  live value holds (`FramePlan::clearsAt`: the units of each body's values, argument lists and received arguments, found
  by a forward walk with one set per open body, `walkBodies`): as when a callee returns, an object only they held can be
  collected (and its destructor run). A body left without a frame (nothing in it can see the call stack) has no
  `inline.leave` and clears nothing there: its values are like the function's own.
- After every operation that can fail, and on every loop's back edge (a branch to a block no later in reverse postorder,
  `pollOnBackEdge`), the generated code loads `ctx->pending` (an atomic monotonic load: other threads set it, and the
  load stays in a loop without calls instead of leaving it; the loads carry the metadata `xb.poll`, on a back edge, or
  `xb.safepoint`, after an operation, for the loop polls pass below); when it is set (marked unlikely, so the loop
  splitting below moves the call to the generic copy) it calls `xb_pending_gc` (which may park the thread for another
  thread's collection, or run destructors, and so collect) or, right after an operation whose error may substitute its
  result while that result has a `Local` home the collector does not see (`checkPending`, from the instruction being
  lowered), `xb_pending`, which never collects nor parks; if that asks to unwind, it goes to the LLVM block of the xIR
  block's unwind block, or leaves through the function's exit. A runtime operator may also return a value the error
  block substituted, of any type: the code after it reads the result's type instead of assuming one, unless
  `subst.check` follows the operation (its types then hold; the check reads the item's own tag and leaves for the
  generic copy on an unlikely edge, see [xir.md](xir.md#passes)), and a comparison that feeds a condition tests the
  truth of what its slow path returned. `truth` tests the item type inline; a non-logical value calls `xb_cond_error`
  and leaves.
- Variables: a captured variable's cell is loaded from `self->cells[]` (the `xb_block` layout is mirrored with the
  target `DataLayout`); a `STATIC` first tests `xb_storage_shared` (`storageLocal`: shared storage once a second thread
  runs, read and written then through `xb_ref_load` and `xb_ref_store` with the type `XB_T_NIL`; its write barrier and
  `each.move` leave it to the runtime too); a `STATIC`, a shared or captured variable or a parameter tests its type
  inline (one unsigned comparison covers `XB_T_CELL` and `XB_T_REF`) and reaches a local cell's value in place
  (`varTarget`: the cell's header state byte tested, `isLocal`, then its value's type). A read of a `FOR EACH` variable
  reads in place what its local loop reference designates: an element of a local collection (`inlineRefTarget`), or the
  reference's own item (a string's character, an enumerator's `Current`). The rest (a shared cell, a shared object,
  another element, a store into a `FOR EACH` variable's element) goes to the runtime with the item's type and pointer:
  `xb_ref_load` copies the value into the load's own item (or its shadow slot), `xb_ref_store` writes it and makes its
  write barrier; they read and write the whole item under the object's counter or lock when it is shared, and raise the
  bound error of an element gone.
- `block` creates the block first into an item of its own (`block.tmp`), kept as a C root (`xb_gc_root_push`) while it
  obtains the cells (`xb_cell_of`: the variable's cell, made now if it has none, or a new cell holding the current
  element of a `FOR EACH` variable, which only the block refers to; `xb_cell_new` for a value captured) and stores each
  one, then passes the barrier test for the block (a collection meanwhile may have aged it) and copies it to its home,
  which may be the slot of a variable that moved into its cell meanwhile. `ref` calls `xb_ref_of` (or builds the
  `XB_T_CELL` item of a captured variable's cell); `ref.index` calls `xb_ref_elem`. `each.enter` and `each.leave` call
  `xb_each_enter` and `xb_each_leave`; `each.move` stores the position in the loop's reference (a local one: another
  goes to the runtime), taking the collection's type from the loop's source (its operand, whose type a loop that
  `version-each` versioned knows: then only that type's path is emitted), and calls `xb_each_move` only when the element
  is not one it finds in place (an element of a local array of any form within its length, the entry at a position of a
  local hash without holes; a string's byte goes into the reference's own item as the shared one-byte string, nothing
  past its end; not an enumerator's element, a shared collection, the bound error), or when the variable is in a shared
  cell (whose reference only the runtime moves); `each.key` stores the position for anything but a hash, reads the key
  of a local hash without holes in place, and calls `xb_each_key` for the rest. A read of a `FOR EACH` variable of the
  frame alone (not shared, not a parameter), whose last change on every path to it is the loop's `each.move`
  (`FramePlan::findEachLoads`, a forward dataflow of each such variable's last change: no assignment between on any
  path, nor an inner loop's restore, and none known where an unwinding enters; the plan keeps the source live at those
  reads and at their uses in their block), takes its element from the loop's source at the step's position
  (`loadEachElement`), by the source's type (known in a loop `version-each` versioned; tested there in a resumable
  function, which it leaves alone): a string's byte as the shared one-byte string, a local array's element within it (an
  item in place; an integer or a logical of a compact form into the read's item, a `NIL` of one through the reference),
  the value of the entry at the position of a local hash without holes; anything else through the reference. `Asc()` of
  such a read, and its comparison with a one-character literal (`=`, `==`, `!=`), in the read's own block read the byte
  from the string (`elementByte`). Over a string whose every read is such (`FramePlan::readsInPlace`), `each.move` does
  nothing: no one reads the reference's item.
- `barrier` ([runtime.md](runtime.md#garbage-collector)): after a store into a local cell that holds no element (a
  captured variable's, or the one a variable holds), the cell's GC state byte is tested inline and `xb_gc_remember`
  called for an old cell (`cellStoreWritten`); a shared cell and an element took the value through `xb_ref_store`,
  which made the barrier. Frame slots and `STATIC` storage are roots and need nothing.
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
folded result's type and decimals follow Clipper's compile-time rules, which differ from the run-time operators. The
same functions (`foldNumbers`, `foldNumber`, `compareNumbers`, `stringJoin`, `stringTest`) also fold the xIR's
constants, which are values the program computes at run time, with the run-time operators' rules instead (`runtime`:
[xir.md](xir.md#passes)). The rules are those of `xbrt.h`: `xb_fold_numbers` and `xb_fold_negate` for numbers (on
`xb_folded`, with `XB_DEC_AT_RUN` for decimals SET DECIMALS gives when the code runs), `xb_fold_compare` and
`xb_folded_order` for comparisons, `xb_fold_strings` for strings; the macro compiler folds with them too
([runtime.md](runtime.md#macro-operator)): a text gives the same value compiled or as a macro.

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
| comparisons of numbers or logicals; `=`, `==`, `!=` of `NIL` and a literal (`xb_fold_nil`: equal when both are `NIL`) | `.T.`/`.F.` |
| strings `+`, `-`, `==`, `$`; `=`/`!=` of two strings of one length | the result (`=` of strings of different lengths depends on SET EXACT and is not folded; `stringJoin`, `stringTest`) |
| `.NOT.`, `.AND.`/`.OR.` with a logical literal on the left, `IIF` with a logical literal condition | the result or the remaining operand |
| a call the language evaluates when compiling, of a `FOLD` function with literal arguments | its result, a literal that folds further (`Len( Chr( 65 ) ) / 1` is the integer 1, while `Len( SubStr( "ab", 2 ) ) / 1` is `1.00`): `Len` of a string literal (its length) or of an array literal of constants (its element count); `Chr`; `Asc` and `At` (integers with a program constant's width, as `Len`'s); `Upper` of a text of ASCII letters, digits and spaces only (`xb_upper_folds`); `Empty` of a constant, of a codeblock literal, of an array or hash literal of such literals (one that evaluates anything else, such as a call, is left to run time, which keeps its effects); the bit functions on number constants (an integer without a display width of its own, as the runtime makes it; arithmetic on such an integer gives one too) |

The evaluators of the `FOLD` functions are one table in `opt/Fold.cpp` (`evaluateCall`), on the rules `xbrt.h` shares
with the runtime functions (`xb_bytes_find`, `xb_substr_range`, `xb_trim_range`, `xb_take_count`, `xb_count_arg`,
`xb_chr_byte`, `xb_asc_code`, `xb_is_blank`, `xb_upper_folds`, `XB_TRIM_*`). On the syntax tree (`literal`) only the
calls above are evaluated; the xIR's `fold-constants` evaluates every call of a `FOLD` function on constants
([xir.md](xir.md#passes)), the others too (`Lower`, `Upper` of any ASCII text, `SubStr`, `Left`, `Right`, `Trim`,
`RTrim`, `LTrim`, `AllTrim`, `RAt`, `Replicate` and `Space` up to 4096 bytes), with the value the function returns at
run time (an integer is a computed one), so that what follows keeps the run-time rules. A call with arguments of another
type, too few or too many, is left to run time, with its error.

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
otherwise its own digit count. Decimal literals too large for 64-bit integers become doubles; a hexadecimal one is
an integer of up to 64 bits, read as two's complement, with the integer literal's width (`0xFFFFFFFF` takes 11
columns, `0xFFFFFFFFFFFFFFFF`, -1, 10), and one past 64 bits is an error.

## Object emission

`codegen/Emit.*` initializes every target LLVM was built with, creates the `TargetMachine` for `--target` (default: the
host), runs LLVM's default optimization pipeline for the `-O` level (new pass manager), then, from `-O1` on, splits the
loops whose fast paths have cold edges (`LoopSplit.*`, below), places the polls of the loops' back edges (`LoopPolls.*`,
below), moves loops' invariant tests before them (`LoopGuards.*`, below; three rounds, each followed by the versioning
of loops on their arrays' forms, `unswitchFormTests`; one for a function past 20000 instructions as the default pipeline
left it, `kOneRound`, which each round simplifies whole again), versions the fast loops on their other invariant tests
(`versionInvariantColdExits`, `LoopSplit.*`, below; then the forms and the guards once more), sets frames up where they
are needed (`FrameSetup.*`, below, whose small entries are then inlined: `AlwaysInlinerPass`), drops the tests of
pending requests that repeat one made with no call since (`skipRepeatedTests`, below), each followed by the function
simplification pipeline on the functions it changed, and writes the object with the target's object writer (code
generation runs on the legacy pass manager: in LLVM 22 and 23 it is the one every target implements). At `-O0` (no xIR
passes, LLVM's `-O0` pipeline) every function first gets the passes whose time follows its size (`cleanUpFunctions`,
below: SROA, early CSE, instruction combining, CFG simplification), and code generation allocates registers
(`CodeGenOptLevel::Less`, as at `-O1`): the fast allocator of `None` gives every value a stack slot of its own, so that
a function of inline fast paths took kilobytes of stack per call (the `recursion_deep` test's function 3.8 KB on 32-bit
x86 and 2.4 KB on x86-64; 0.7 KB and 0.2 KB so), and `xbc` emits no debugging information that such code would serve. It
costs compile time (the `large_function` test's `Main`, below: 15 s at `-O0`, 39 s at `-O2`).

A function of more than 60000 instructions as code generation makes it (`kLargeFunction`: a procedure of a few thousand
statements) makes the compilation slow: several passes of the pipeline and of code generation take time that grows
faster than a function's size. It is optimized as any other, speed first, and from `-O1` on `xbc` warns which function
it is, where it starts, with its size, so that its author may split it (`optimizeModule` returns those functions,
`CodeGen::sourceOf` names them; the `warn.optimize_limit` tests). Measured (x86-64, a Release `xbc`): the
`large_function` test's `Main` of 391000 instructions compiles in 39 s at `-O2` and 15 s at `-O0`. At `-O0` such a
function is marked `optnone` and `noinline` after the passes whose time follows its size (`cleanUpFunctions`: SROA,
which keeps its `Local` items in registers, early CSE, instruction combining, CFG simplification): the pipeline skips it
(`OptNoneInstrumentation`) and nothing is inlined into it nor it into its caller. The size is the one before LLVM
inlines anything.

Code is position independent unless `--static` is given. The CPU is `generic` unless `--cpu` names one, so programs run
on any CPU of the target, with one feature added: 32-bit x86 code computes doubles with SSE2 (`+sse2`, every x86 since
the Pentium 4; the runtime is built with `-msse2 -mfpmath=sse` there), not the x87 unit, whose registers keep more
precision between operations, so that a double is rounded after each operation as on every other target; `--features`
given explicitly comes after. `--cpu=native` (host target only) uses the CPU and features of the machine xbc runs on.

Loops without slow paths (`codegen/LoopSplit.*`): every inline fast path keeps a call to the runtime for the other
cases, behind an edge weighted unlikely (`branchLikely`: edges to runtime calls, and to the rare double index; a choice
between two inline paths, such as an integer or a double operand, or a variable that holds a cell, is a plain branch).
The test of an array's form (`kFormTest`) is weighted for the path of items too, but is no cold edge: an array of a
compact form takes its other side on every pass.
The loops with such cold edges, found once per function, are put in simplified and LCSSA form and cloned twice: a
generic copy and the loop again. A nest is split whole, from its outermost loop that fits the limits below and whose
fast copy would make no call (`hotCall`: a call reached from the header without a cold edge; the write barriers, whose
declarations carry `kBarrierAttr`, do not count), so that its inner loops' slow paths leave the nest's fast copies too;
a nest that does not fit, or calls a function of the program anyway, is not, and its inner loops are looked at in turn.
In the original, every cold edge goes to the same block of the generic copy, which runs the rest of the iteration
generically; each back edge of the generic copy (the loop's, and in a nest each inner loop's with a single latch) goes
to the same header of the loop again, through a preheader of its own, whose cold edges go to the generic copy as well
(the values of the loop again that the generic copy may now bring are joined too). So one slow path (an error, a value
of another type, a destructor or a collection's safepoint served by a poll) costs one generic iteration, and the next
ones run fast paths again: the loop again knows less of the values that enter it than the original (their types and
ranges come from the generic copy), so the original runs as long as no slow path is taken. `SSAUpdater` joins the values
of the copies where several may arrive, and the exits receive the copies' values. The original and the loop again keep
only the calls on their own paths (a `Len()` in a `FOR` limit stays), so the function simplification pipeline, run again
on the functions that changed, optimizes them as fast loops: values keep the types their fast paths give them, and loads
can leave them (the loop again computes what it hoisted in its preheader, after the generic iteration's calls). A loop
of more than 3000 instructions or 256 cold edges is left as it is, and the copies grow a function by at most twice its
own size (`kMaxLoopInstructions`, `kMaxColdEdges`; the `pi` benchmark's function, a nest of Brainfuck loops, then
compiles in 5.9 s instead of 3.6 s). The module is verified after the split (an invalid module is an internal error).

Loop polls (`codegen/LoopPolls.*`), on the loops the splitting left (its fast loops among them): a back edge's poll is a
load and a branch on every pass, which in a loop of a few instructions costs as much as its work. A loop where a test
that serves a safepoint (`xb.safepoint`, after a call) runs on every pass (its block dominates the latches) needs no
poll: its polls go. Of the others, only an innermost loop polls less than once a pass (an outer loop's pass runs its
inner loops, whose polls start anew on each entry, so its own poll bounds the passes between two of them). A counted
loop, whose exit test compares an induction variable of constant step (scalar evolution) with a value the loop does not
change (`ne`, or `<`, `<=` going up, `>`, `>=` going down, signed or not), polls in that test's branch (strip mining)
when the test is the header's, where the poll goes on to, or the one of the block the poll goes on to, which the poll's
block dominates (a loop LLVM rotated, the test after the body; the poll's slow path then receives that pass's own values
and the test's block, not the header, runs again after a poll, with the same conditions): the test compares with a stop,
the exit's value or the variable's value `kPassesPerPoll` (1024) passes ahead (the stride must fit the variable's type;
for `ne`, the exit's value only when the step divides the distance to it, or the variable would step over it and never
poll again), kept in a PHI of the header; reaching it, a rare block tells the exit from a poll, which loads the pending
requests and, when one is set, enters the old poll's slow path (in the generic copy) as if the previous pass's back edge
had polled, with the header's values (the values computed after the header and used on that path must be ones the
header's PHIs carry over; their uses are rewritten); then a new stop is computed and the header runs again (it must
compute nothing that running again would change: no call, no atomic access, stores only of what it does not load; the
slow path must be outside the loop, and the header's own values must not be used on it, being the next pass's there; the
search for such uses stops at the header, past which a pass computes its values anew). A loop whose count is known and
below the stride ends up with no poll at all (the stop is the exit's value). Any other loop (its poll going on to any
block of the loop) counts its passes down in a register (`poll.left`, joined on the ways back to the header) and loads
the pending requests once every 1024 passes. Every loop is planned first, while the analyses describe the function (a
counted loop gets a preheader then), and changed after: each poll's branch is read again then (a preheader inserted for
another loop may have split the edge to where it goes on), and a counted loop whose preheader another loop's change took
(an edge from its slow path into the header) keeps polling on every pass. So a thread in loops without calls reaches a
safepoint within 1024 passes of an innermost loop or one pass of another, at the cost of the counted loop's own test, or
a decrement and a branch. Measured against the code before stop-the-world collections (perf stat, x86-64, the best of 5
runs): `loops` 2% more cycles (9% with a load on each pass), `pi` 2% (8%), `qsort`, `sieve` and `asort` 1 to 2%, the
others within 1%; `crc32` and `mixed` are 29% and 7% faster (their loops back on fast paths after a slow one), `index`
8% (the index page cache, [rdd.md](rdd.md#performance)).

Loop guards (`codegen/LoopGuards.*`), on the loops the simplification rotated after the polls: a loop's header that
computes nothing but its PHIs and a branch on a condition the loop does not change (LLVM moved what it reads out of the
loop), one side of which leaves the loop (in a fast loop, into the generic copy: the test of a fast path, such as an
array and an index the loop does not change), branches in the preheader instead (one is made only for such a loop);
when the test fails there, the first pass goes where it would have gone, with the values the header's PHIs have on
entry (the exit's PHIs receive those, and the header's values used past it are rewritten), so the generic copy runs it
as before. What the test guarded (an element's loads) then runs on every pass, and the simplification moves it out of
the loop; its own test may then be the header's next invariant branch (one round per level of an array of arrays).
LLVM's own unswitching does not do it: the exit receives values the loop changes. `matmul` takes 12% fewer cycles,
`pi` 3%; the other benchmarks are unchanged.

Loop versioning (`versionInvariantColdExits` in `codegen/LoopSplit.*`), after the guards: in an innermost loop, the
branches whose condition can be computed on entry and whose cold side leaves the loop (into the generic copy) are tested
once in the preheader, together (each condition frozen first: one the loop never reaches may be poison on entry). A
condition can be computed on entry from what LLVM moved out of the loop (an array's type, an invariant index's bounds)
and, in a loop that makes no call (the write barriers aside), from the sharing state of an object an item holds that the
loop does not change (an array indexed by the loop's counter): the state load carries the item's layout (`kStateLoad`
metadata, put by `isLocal`), and the preheader reads the item's type and, when it holds an array, a hash, a cell or a
reference, the object's state byte, and computes the condition from them (an item in `STATIC` storage, which another
thread may write, is not read so) (without a call, no object this thread alone reaches becomes shared). When any would
leave, or an item holds no such object, an exact copy of the loop runs, which leaves where the loop would have;
otherwise the loop runs with those branches gone. Values of the loop used past it become the join of the loop's and the
copy's (`SSAUpdater`); a loop is versioned once, its copy never, and the copies grow a function by at most its own size.
Unlike the guards, the branch need not be the header's: the copy, not the generic loop, takes the first pass. A form
test whose condition can be computed on entry counts as such a branch, its compact side as the one that leaves, and the
exits on that side count for nothing (the loop that runs when none leaves takes the path of items). With the
sharing tests of compiled code (step 3 of [concurrency.md](concurrency.md#shared-objects)), nests split whole and the
versioning, measured against the code before them (the best of 7 runs, x86-64): `pi` 16 to 18% faster, `matmul` 6 to
11%, `qsort` 7%, the others within their noise (`generator`, `sieve` and `fib` within 2%).

Form tests (`unswitchFormTests` in `codegen/LoopSplit.*`), with each round of guards and after the versioning: an
innermost loop whose header is the form test of an array the loop does not change (its condition moved out of the loop,
`makeLoopInvariant`) is versioned on it the same way: the loop runs with the path of items alone when the array holds
items on entry, its copy otherwise, which takes the path of the compact form alone (no fast loop changes a form: what
the test reads on entry it reads on every pass), its other form tests plain branches (the copy is versioned no more;
each pass grows a function by at most its own size). The test, which is no
guard (both its sides stay in the loop), no longer stands between the guards before it and those after it, so they leave
the loop in turn: a loop over an invariant row of an array of arrays (`matmul`'s) reads the row in its preheader again.
The versioning makes a preheader for a loop entered from several blocks. A loop of `pi`'s kind (a tape of integers
indexed by a moving cell, read and written) runs 56% faster than with items, in a function of its own (perf stat,
x86-64).

Memory classes of `memset`s (`codegen/MemoryTags.*`): every access the lowering makes carries its memory class
(`Mem`, TBAA), so that a loop's loads of what it does not change (an array's header, an object's state byte) leave
it; but MemCpyOpt merges the two stores of a constant item (a `NIL`) into a `memset` that has none, which may write
anything, and the loads stay. `TagItemMemsets`, run where the pipeline's peephole and late scalar extension points
are, gives a `memset` within the items of an alloca (a frame's slots, a local item, an array of items) the class of
items again; the lowering names the class in the module (`xb.item.tbaa`).

Frames set up where they are needed (`sinkFrameSetup` in `codegen/FrameSetup.*`), last: the entry of a direct entry
writes its frame's slots (the parameters, `NIL` in the others) and header, links it into the context's chain and tests
the stack's limit, but nothing needs that before the first instruction that touches the frame, touches the chain (an
inlined body links a frame of its own) or calls a function (anything may collect, raise an error or walk the chain). The
setup moves to that instruction on every path (the first of a block that no other such block dominates), behind a flag
the entry makes false and a setup true (the simplification turns it into PHIs and folds the tests a setup dominates): it
writes the slots with the values they had on entry, which nothing touched before, and the unlink at the exit stores the
caller's frame, which the chain still holds when nothing was linked. When some way returns without a setup (an early
return), the exit unlinks only a frame linked (behind the same flag), storing the caller's frame read back from the
frame's link: that way leaves the chain as it found it and keeps nothing for the way out (no saved register, no stack),
and no register keeps the caller's frame while the body runs. (Where every way sets the frame up, the flag would only
stay live to the exit, through the function's loops.) An entry that needs the frame itself (a mark of PRIVATEs) is left
as it is. Then,
when the part of the function that runs before any setup on its ways to a return reads memory but writes only its
result and its own locals and can return (at most `kMaxLeafEntry` instructions), the function becomes that part, a small
entry whose ways into a setup, or into code that returns only after one, call a copy of the whole function with the
same arguments (the copy does the part again: nothing it did was seen), marked `alwaysinline`: its callers then take a
leaf's early return in place, without a call. `fib`'s `n < 2` is one: its calls take 47% fewer cycles and 26% fewer
instructions (perf stat, x86-64); so is `Ack`'s `m == 0`, whose other ways (the recursive calls) go to the copy at
once: `ackermann` 19% fewer instructions.

Repeated tests of the pending requests (`skipRepeatedTests` in `codegen/LoopPolls.*`), after the small entries are
inlined: a test that serves a safepoint (`xb.safepoint`) where no call ran since the last test that found nothing to
unwind for (its way on, or its slow path's when the call there returned 0), on every way to it, tests nothing (only the
runtime's calls make the thread's own requests; another thread's wait for the next test, which every caller makes after
its call), and goes; when only some ways into its block are such (an inlined leaf's early return, which calls nothing,
next to the call of the whole copy), the load becomes a PHI of 0 from those and of a load at the end of the others, and
the simplification takes the clean ways past the test. A way back (to a block no later in reverse postorder) counts as
one where a call ran: a loop whose only test comes after a call that its passes take in place (`x += Leaf( i % 2 )`,
whose back edge's poll went since that test runs on every pass) keeps it, so no cycle runs without a test, and a test
that goes is always preceded by a kept one (`platform.gc_loops` spins in such a loop while another thread collects).
`ackermann` 4% fewer instructions, `fib` 4%.
