# Roadmap

What xbase still lacks to compile Clipper programs, the modern extensions planned beyond Clipper, and the order of the
work.
Everything here is planned unless it says otherwise. [coverage.md](coverage.md) lists every command and runtime
function with its status; [architecture.md](architecture.md#planned-design) and [xir.md](xir.md) hold the planned
design of the larger parts.

## Gaps with Clipper

### Program structure and linking

- Done: several sources compile in one run into one object (`xbc *.prg`), a call to a function of another module
  needs no declaration (it is found by name when linking, as in Clipper), and the program starts at a public `MAIN`
  or else at the first function of the first source ([compiler.md](compiler.md#sources-and-modules)). An unknown name
  is reported by the linker, as in Clipper; reporting it in `xbc` waits for `xbc` to link programs itself.
- Already as in Clipper: `STATIC FUNCTION`/`STATIC PROCEDURE` are visible only in their module, and a file-wide
  `STATIC` has one value per module.
- Missing: `INIT PROCEDURE` and `EXIT PROCEDURE` (run before and after `Main`), `ANNOUNCE`, `REQUEST`/`EXTERNAL`
  (symbols linked in for macros and `Do()`), a dynamic symbol table (name to function, for `&`, `Do()`, `Eval` of
  macro-built blocks), and Clipper's 10 significant characters of names.

### Variables

- `MEMVAR`, `PRIVATE`, `PUBLIC`, `PARAMETERS`, `M->name`, `RELEASE`, `CLEAR MEMORY`, `SAVE`/`RESTORE` (`.mem`
  files), `Type()`. Memory variables are dynamically scoped across modules: a `PRIVATE` created in `Main` is seen and
  changed by a function of another module it calls, and is released when the function that created it returns;
  `PUBLIC`s live until the program ends.
- An undeclared name is an error in xbase today. In Clipper it is a field or a memory variable, looked up at run time
  (a field first): reading one that does not exist is error `BASE/1003` ("Variable does not exist"), and assigning
  one creates a `PRIVATE` of the current function. The compiler warns about the ambiguity only when warnings are on.
  Planned: Clipper's behavior, with a warning; an option makes it an error.
- `DECLARE` (a synonym of `PRIVATE` with array dimensions).

### Fields and work areas

`FIELD`, `alias->field`, `FIELD->name`, `(alias)->( expr )`, `SELECT`, and the work areas themselves (the RDD
layer: DBF, FPT/DBT, NTX, CDX, SDF, DELIM).

### Macro operator

`&var`, `&var.suffix`, `&( expr )`, macros inside commands (`USE &cFile`), and blocks built from text
(`&( "{|| ... }" )`). The runtime evaluates them with an expression compiler of its own
([architecture.md](architecture.md#decisions)); names resolve through the dynamic symbol table and the memory
variables.

### Errors

Runtime errors end the program today, with the message and the call stack. Missing:

- `BEGIN SEQUENCE` / `RECOVER [USING x]` / `BREAK [x]` / `END SEQUENCE`, `Break()`.
- The `Error` object (`ErrorNew()`, its exported variables: `args`, `canDefault`, `canRetry`, `canSubstitute`,
  `cargo`, `description`, `filename`, `genCode`, `operation`, `osCode`, `severity`, `subCode`, `subSystem`,
  `tries`), `ErrorBlock()`, the default handler (`ErrorSys`), `error.ch`, and the retry, default and substitute
  protocol (the handler's return value decides).
- The compiler side is the unwind edges of xIR phase 5 ([xir.md](xir.md#control-flow-for-errors-and-exceptions));
  the runtime keeps propagating through the pending flag.

### Procedures, parameters and the call stack

`PCount()`, `ProcName()`, `ProcLine()`, `ProcFile()`, `PValue()`; skipped arguments and `PARAMETERS`; `DO <proc>
WITH` by name at run time.

### Commands, statements and headers

- `std.ch` has 16 of 155 command families ([coverage.md](coverage.md#commands-stdch)); most of the rest need the
  runtime parts below (screen, GET/READ, files, RDD).
- `TEXT ... ENDTEXT` is not a command yet and its lines are compiled as statements.
- Standard headers still to provide: `common.ch`, `inkey.ch`, `error.ch`, `fileio.ch`, `box.ch`, `color.ch`,
  `dbstruct.ch`, `directry.ch`, `getexit.ch`, `memoedit.ch`, `achoice.ch`, `dbedit.ch`, `ord.ch`, `assert.ch`,
  `simpleio.ch`, `blob.ch`.

### Runtime library

58 of 551 functions are done ([coverage.md](coverage.md#functions)). By area, in the planned order:

1. Arrays, strings and numbers: `AScan`, `AEval`, `AIns`, `ADel`, `AFill`, `ACopy`; `PadL`/`PadR`/`PadC`,
   `Transform`, `SoundEx`, the memo-line functions; `Mod`, `Descend`, `Bin2I` and the other binary conversions.
2. Errors and the call stack (above).
3. Files and directories: `FOpen` and the low-level file functions, `File`, `Directory`, `MemoRead`/`MemoWrit`,
   `GetEnv`.
4. The terminal layer: screen, cursor, colors and keyboard, with drivers for plain standard output, ncurses and the
   Windows console; then `@ ... SAY/GET`, `READ`, `AChoice`, `MemoEdit`, `TBrowse`, `dbEdit`, menus.
5. The RDD layer and the database functions and commands.

The SIx Driver and HiPer-SEEK functions (`sx_*`, `hs_*`, `cfts*`) belong to add-on database drivers; whether xbase
provides them is decided when the RDD layer exists.

## Extensions

xbase is Clipper plus the modern features that later xBase dialects introduced and that still make sense today, and
some of its own. Their names and syntax are xbase's own: other dialects' library prefixes are never used, and the
function names below are proposals, settled when each feature is designed in detail. Behavior is checked against the
reference implementation where it has the feature.

Already done: `FOR EACH`, `SWITCH`, `++`/`--` and the compound assignments, references to array elements (`@a[i]`),
the bit functions, `ElapsedMs()`.

### Classes

- Syntax: `CLASS Name [INHERIT Parent1, Parent2, ...]`, `VAR`/`DATA name [INIT value]`, `METHOD name( params )
  [CONSTRUCTOR | INLINE expr]`, `ACCESS`/`ASSIGN`, `ENDCLASS`; a method's body is `METHOD name( params ) CLASS Name`,
  with `Self` and `::name`. `STATIC CLASS` keeps the class inside its module, like `STATIC FUNCTION`.
- `Name()` returns the class object and `Name():New( ... )` creates an instance (`:Init()` is the user's
  constructor). Being a function, a class is used from any module with no declaration (with the link-time resolution
  above), as in the other dialects.
- Multiple inheritance with one instance area per ancestor: when two ancestors have a variable of the same name,
  `o:Parent1:name` and `o:Parent2:name` are different variables; `Super` and casting to an ancestor select the area.
- Scopes checked at run time: `EXPORTED`, `PROTECTED`, `HIDDEN`, `READONLY`, and `FRIEND CLASS`/`FRIEND FUNCTION`;
  a codeblock has the access rights of the function that created it.
- Shared (class) variables, destructors (run by the collector), scalar classes (methods on numbers, strings, arrays,
  ...), operator overloading.
- Compiled natively: dispatch by interned selector with inline caches, devirtualization when the class is known,
  object layouts from the target `DataLayout` ([xir.md](xir.md#classes)).
- Reflection with xbase names (for example `o:ClassName()`, `ObjIsKindOf( o, "Name" )`, `ObjHasVar( o, "name" )`).

### Hashes

- Literals `{ "a" => 1, "b" => 2 }` and `{ => }`; `h[ key ]` reads and assigns; keys are strings, numbers, dates or
  logicals; insertion order is kept; `Len( h )` counts the pairs; `FOR EACH` walks the values in order.
- Functions: `HSet( h, key, value )`, `HGet( h, key [, default] )`, `HHas( h, key )`, `HDel( h, key )`,
  `HKeys( h )`, `HValues( h )`, `HEval( h, bBlock )`, `HClone( h )`. Reading a missing key without a default is an
  error.
- Runtime: an open-addressing table with insertion order, a GC object like arrays.

### Exceptions

- `BEGIN SEQUENCE WITH bErrorHandler` (the handler applies inside the sequence only) and an `ALWAYS` clause that runs
  on every exit from the sequence.
- `TRY` / `CATCH [oErr]` / `FINALLY` / `END`, and `Throw( x )`: `TRY` is a sequence whose handler breaks with the
  error, so the error reaches `CATCH` as its `Error` object; `FINALLY` runs on every exit (normal end, error,
  `RETURN`, `EXIT`, `LOOP`).
- Built on the unwind edges and the pending flag, like `BEGIN SEQUENCE`: no `setjmp`/`longjmp` and no C++
  exceptions.

### Other language features

- `WITH OBJECT o ... END`: `:name` inside means `o:name`.
- Extended codeblocks: a block with statements, `{| a, b | <statements> RETURN x }`, compiled as a nested function
  with its own declarations.
- Variadic functions: `FUNCTION F( a, ... )`; `...` passes the unnamed arguments on to a call, an array literal or
  an index.
- Timestamps (date and time in one value) with their arithmetic and comparisons, date and timestamp literals,
  strings with escapes.
- Function references `@Name()` and symbol values; `Do()` and `Eval()` on them.
- Strings indexed like arrays (`s[ i ]`) and negative indexes counting from the end, if they fit Clipper's rules.

### Generators

A function that contains `YIELD expr` returns an enumerator; `FOR EACH x IN Gen( ... )` runs it step by step. It is a
resumable function ([xir.md](xir.md#resumable-functions)): its frame is a GC object, the same machinery as `ASYNC`.

### async/await and threads

`ASYNC FUNCTION`, `AWAIT`, tasks and a thread pool ([architecture.md](architecture.md#asyncawait)); threads with
per-thread `SET` values, memory variables and work areas, and synchronization (mutexes, conditions).

### Libraries

- Sockets: a `Socket` class over the asynchronous I/O layer (io_uring or epoll, kqueue, IOCP): TCP and UDP, client
  and server, IPv4 and IPv6, with blocking methods and awaitable ones (`AWAIT oSocket:ReadAsync( nBytes )`); TLS
  after it. It needs classes and the async runtime.
- Regular expressions, JSON, compression, serialization of values, translation of messages (i18n).

## Compiler and runtime

- Preprocessor: macro tokens (`&var.suffix`), `__FILE__` and `__LINE__`, `#pragma` options, precompiled `std.ch`
  rules.
- Separate compilation: a declaration file generated per module, so modules check each other's calls.
- Runtime entry points with LLVM attributes (`nounwind`, `willreturn`, memory effects, `nocapture`), so that LLVM's
  own passes see through them.
- FFI: `EXTERN C FUNCTION` with types and structures ([architecture.md](architecture.md#runtime-declarations)), and
  runtime parts written in xBase.
- GC: pages returned to the operating system; statepoints and a moving collector ([xir.md](xir.md#phases)).

### Diagnostics

- One table of messages (identifier, default severity, group, text) with options: `-w`, `-W<group>`,
  `-Wno-<group>`, `-Werror`, an error limit; warnings counted and summed up; documented exit codes (errors, usage,
  internal errors).
- Locations as ranges, with the source line and a caret; tokens a `#command` or `#define` produced carry where they
  were expanded from, and errors in them add "in expansion of" notes.
- Recovery: a stack of open constructs, so a closer of an outer construct (`NEXT` with an `IF` still open) closes the
  inner ones with one error; an error node in the tree, so the semantic checks run after a syntax error; an undeclared
  name reported once per function.
- A statement that is an expression without effect (`x == 3`) is an error in Clipper; xbase warns.
- Messages show names as written and the text of tokens; Clipper's `EXTERNAL name` and `REQUEST name`, sends, aliases,
  macros and `BEGIN SEQUENCE` give one "not supported yet" error each.

### Front end

- A precedence-climbing expression parser driven by a table, in place of one function per level.
- One child visitor over the syntax tree for the walkers (call resolution, folding, semantic checks, xIR
  generation), and one table of block constructs (opener, middle keywords, closers) for the parser.
- Quadratic scans in the preprocessor (`[` strings, translate rescans); a statement-size limit relative to the source
  instead of a fixed token count.
- Numeric literals: a limit on decimals, and the default-decimals marker as a flag rather than the value 255.
- The back end's memory per operation (a statement of 30000 `+` takes 1 GB to compile).

### xIR and code generation

- Before `BEGIN SEQUENCE`: a call writes its destination only once it succeeds (today the destination is cleared
  first); operations that fail may run code (an error handler), which the effects catalog and every pass that moves
  or merges operations must respect; a store and its write barrier move together.
- Verifier: no parameters on the entry block and no branches to it, the access kind of a variable, the captures of a
  `block`; linear cost.
- Interned constants; moving, splitting and cloning instructions; cached analyses (CFG, dominators, loops) with
  invalidation; module passes; running single passes for tests (`--xir-passes`).
- Measured plan, in order: items stored and copied as two 8-byte words (store forwarding); inline `index` and
  `index.set`; inline double and mixed arithmetic and comparisons; a type lattice with sparse conditional constant
  propagation; typed operations (boxing, unboxing, overflow checks) with the fast paths as their lowering; values that
  hold no heap pointer kept in registers; parameter versioning; value numbering, loop-invariant code motion, range
  analysis and bounds-check elimination; runtime functions as xIR operations, inlining, string appends in place,
  direct calls with fixed arity.

### Runtime

- The error subsystem in one ABI change: a frame descriptor (function name, file) and the current line in each frame,
  error state owned by the context and marked by the collector, a table of errors with their generic codes and
  abilities (retry, default, substitute), `ErrorBlock`, `BREAK`/`RECOVER`, a default handler written in xBase that
  substitutes 0 for a division by zero and prints the `Called from` lines.
- A stack limit checked in function prologues (deep recursion is a crash today).
- The ABI check for every module, not only the start module.
- One table of type properties (heap object, `ValType` letter, sort order, `Empty`, display), with an abort on an
  unknown tag while marking.
- `Array()` with more than 64 dimensions; one substring search and one byte comparison helper; shared one-byte and
  empty strings instead of allocating them.
- GC pauses: mark epochs instead of clearing every mark, a stack watermark for young collections.

## Order of the work

1. The dynamic symbol table; `INIT`/`EXIT PROCEDURE`.
2. Errors: `BEGIN SEQUENCE`, `Break`, the `Error` object, `ErrorBlock`, the default handler; xIR unwind edges.
3. Memory variables and Clipper's rule for undeclared names; `Type()`.
4. The macro operator.
5. The rest of the array, string and number functions; the call-stack functions.
6. Hashes; classes (multiple inheritance, scopes); `BEGIN SEQUENCE WITH`, `ALWAYS`, `TRY`/`CATCH`/`FINALLY`;
   `WITH OBJECT`, extended codeblocks, variadic functions.
7. Files and directories; the terminal layer; GET/READ and TBrowse.
8. The RDD layer.
9. Generators, async/await and threads, sockets and the other libraries.

Alongside, the compiler's own track ([xir.md](xir.md#phases)): type inference and unboxed numbers, value numbering,
statepoints and a moving collector, inlining, loop optimizations, measured with the benchmarks
([testing.md](testing.md#benchmarks)).
