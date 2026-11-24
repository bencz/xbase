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
- Done: `INIT PROCEDURE` and `EXIT PROCEDURE`, the symbol table of public and runtime functions by name, `Do()`
  ([runtime.md](runtime.md#program-startup)).
- Done: `ANNOUNCE`, `REQUEST`/`EXTERNAL` ([compiler.md](compiler.md#grammar)).
- Done: the macro operator's calls by name ([runtime.md](runtime.md#macro-operator)).
- By design, not as in Clipper: every character of a name counts (Clipper keeps 10), so long names never clash.

### Variables

- Done: `MEMVAR`, `PRIVATE` (and `DECLARE`), `PUBLIC`, `PARAMETERS`, `M->name`, `RELEASE` (`ALL`, `LIKE`, `EXCEPT`),
  `CLEAR MEMORY`, `SAVE`/`RESTORE` (`.mem` files), and Clipper's rule for undeclared names (a memory variable at run
  time, with a warning; `-Wno-undeclared`, `-Werror=undeclared`) ([runtime.md](runtime.md#memory-variables),
  [compiler.md](compiler.md#semantic-checks)); a memory variable as the variable of `FOR EACH`.

### Fields and work areas

- Done: `FIELD` (with `IN`), `alias->field`, `FIELD->name`, `(expr)->name`, `alias->( expr )`, undeclared names read
  as a field first, `SELECT`, the work areas, `DBF` tables, memos (DBT, FPT), indexes (NTX, CDX), sharing and locks,
  relations, SDF and DELIM ([rdd.md](rdd.md)).
- Missing: `LIST` and `DISPLAY`, with the terminal's redirections, and the SQL engines, as
  [rdd.md](rdd.md#order-of-the-work) orders them.

### Macro operator

- Done: `&var`, `&var.`, `&var.suffix`, `prefix&var.`, `&( expr )`, assignment, `&name( args )`,
  `PRIVATE`/`PUBLIC &name`, blocks built from text (`&( "{|| ... }" )`), Clipper's text substitution in strings and
  `Type()`, compiled by the runtime's own expression compiler and cached
  ([runtime.md](runtime.md#macro-operator), [compiler.md](compiler.md#grammar)).
- Done also: aliases in and before macros (`&cAlias->field`, `( expr )->( ... )` inside the text).
- Done also: macro messages (`o:&message`), macros passed by reference (`@&name`) and `PCount()`/`PValue()` inside a
  macro ([runtime.md](runtime.md#macro-operator)).
- Missing: the reference's macro lists (`&c` spread into arguments, its own extension).

### Errors

Done: objects of runtime classes, messages (`o:x`, `o:x := v`, `o:m( args )`), runtime errors as `Error` objects,
`ErrorBlock()`, substitution, `ErrorSys()` and the default handler in xBase, `BREAK`/`Break()`, `BEGIN SEQUENCE` /
`RECOVER [USING x]` on xIR unwind edges ([runtime.md](runtime.md#errors-and-pending-requests),
[xir.md](xir.md#control-flow-for-errors-and-exceptions)), retrying (`canRetry`, `tries`) and defaults
(`canDefault`) for file and database errors. Missing: the default handler's choice between quitting, retrying and the
default (`Alert()`, the terminal layer).

### Procedures, parameters and the call stack

Done: skipped arguments, `PARAMETERS`, and `DO &name [WITH args]`, the procedure named at run time
([compiler.md](compiler.md#grammar)).

### Commands, statements and headers

- `std.ch` has 74 of 157 command families ([coverage.md](coverage.md#commands-stdch)); most of the rest need the
  terminal layer below (screen, GET/READ, printing).
- `TEXT ... ENDTEXT` is done ([preprocessor.md](preprocessor.md#text-blocks)); `TEXT TO FILE` and `TEXT TO PRINTER`
  come with the console's redirections ([terminal.md](terminal.md#order-of-the-work)).
- Standard headers still to provide: `inkey.ch`, `box.ch`, `color.ch`, `getexit.ch`, `memoedit.ch`, `achoice.ch`,
  `dbedit.ch`, `ord.ch`, `blob.ch`.

### Runtime library

216 of 551 functions are done ([coverage.md](coverage.md#functions)). By area, in the planned order:

1. Files and directories: done ([runtime.md](runtime.md#files)) but the drive functions (`DiskChange`, `DiskName`,
   `IsDisk`) and `FSetDevMod`.
2. The terminal layer ([terminal.md](terminal.md)): screen, cursor, colors and keyboard, with drivers for plain
   standard output, terminals (xbase's own, not ncurses) and the Windows console; then `@ ... SAY/GET`, `READ`,
   `AChoice`, `MemoEdit`, `TBrowse`, `dbEdit`, menus.
3. The RDD layer and the database functions and commands ([rdd.md](rdd.md)): done but `LIST`/`DISPLAY` (with the
   terminal) and the SQL engines.

The SIx Driver and HiPer-SEEK functions (`sx_*`, `hs_*`, `cfts*`) belong to add-on database drivers; whether xbase
provides them is still to be decided.

## Extensions

xbase is Clipper plus the modern features that later xBase dialects introduced and that still make sense today, and
some of its own. Their names and syntax are xbase's own: other dialects' library prefixes are never used, and the
function names below are proposals, settled when each feature is designed in detail. Behavior is checked against the
reference implementation where it has the feature.

Already done: `FOR EACH` (also `FOR EACH k => v`), `SWITCH`, `++`/`--` and the compound assignments, references to array
elements (`@a[i]`), the bit functions and operators (`& | ^^ << >> ~` and their compound assignments), `ElapsedMs()`,
hashes (literals, the `H*` functions, see [runtime.md](runtime.md#hashes)).

### Classes

Done: `CLASS ... ENDCLASS` with `INHERIT` (multiple, an area of variables per ancestor), `VAR`/`DATA [INIT]`,
`METHOD` (implemented, `INLINE`, `CONSTRUCTOR`), `ACCESS`/`ASSIGN`, `Self`, `::`, `Super`, `New`/`Init`, the scopes
`EXPORTED`, `PROTECTED`, `HIDDEN`, `READONLY`, `STATIC CLASS`, classes across modules, casts to an ancestor
(`o:Parent1:name`, `o:Super`), reflection, class variables (`CLASSVAR`, `SHARED`), `FRIEND CLASS`/`FRIEND FUNCTION`
([runtime.md](runtime.md#classes)).
Operator overloading and scalar classes (`ASSOCIATE CLASS`) are done too.
Destructors and polymorphic inline caches for sends are done.
Sends to objects whose class the compiler knows are resolved when compiling (calls, inlined methods, variables at
known slots), by the runtime's own rules.

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
- Reflection with xbase names (for example `ObjIsKindOf( o, "Name" )`, `ObjHasVar( o, "name" )`; `ClassName` exists).

### Exceptions

Done ([compiler.md](compiler.md#grammar)): `BEGIN SEQUENCE WITH bErrorHandler` (the handler is the error block while
the body runs) and `ALWAYS`; `TRY` / `CATCH [oErr]` / `FINALLY` / `END` and `Throw( x )`, where `FINALLY` runs on
every exit (normal end, error, `RETURN`, `EXIT`, `LOOP`). Built on the unwind edges and the pending flag, like
`BEGIN SEQUENCE`: no `setjmp`/`longjmp` and no C++ exceptions.

### Other language features

- Done: `WITH OBJECT o ... END` (`:name` inside means `o:name`) and extended codeblocks (`{| a, b |`, statements,
  `}`), see [compiler.md](compiler.md#grammar).
- `...` as the indexes of an element (`a[ ... ]`) and in codeblocks (`{| a, ... | ... }`); variadic functions and `...`
  as the last argument of calls and sends and the last element of array literals are done
  ([compiler.md](compiler.md#grammar)).
- Timestamps (date and time in one value) with their arithmetic and comparisons, date and timestamp literals,
  strings with escapes.
- Done: function references `@Name()` (symbol values), run by `Eval()`, `Do()` and `:Exec()`
  ([runtime.md](runtime.md#function-references)).
- Strings indexed like arrays (`s[ i ]`) and negative indexes counting from the end, if they fit Clipper's rules.
- Done: `SET LANGUAGE TO <id>` ([runtime.md](runtime.md#languages)) and `SET CODEPAGE TO <cp>` with UTF-8 the
  default and the character functions ([runtime.md](runtime.md#codepages)). Planned: comparisons and sorting by a
  codepage's order, and the database codepage with the RDD.

### Enumeration

Collections follow one enumerator model, like the .NET enumerable and enumerator interfaces: `FOR EACH` asks a
collection for an enumerator and moves it until it ends. Arrays, strings and hashes keep their paths by position;
objects take part with `GetEnumerator()`, `MoveNext()`, `Current` and `Reset()` (done, see
[runtime.md](runtime.md#objects-and-messages)). Positions, keys and values come from the loop itself (`FOR EACH k =>
v`), never from messages sent to the loop variable. Generators produce enumerators, the built-in collections are
classes that inherit `Enumerable` (LINQ-like methods, the lazy ones written with `YIELD`), and `AEval` and `AScan` go
through collection objects.

Queries (LINQ), in layers. Done: the interfaces `IEnumerable`, `IEnumerator`, `IOrderedEnumerable`, and ordered
sequences (`OrderBy` with `ThenBy`). *(Planned)*: `IQueryable` and `IQueryProvider`, a `Queryable` whose operators
add nodes to an expression tree that its provider runs; the compiler capturing a codeblock literal as an expression
tree (from its syntax tree, its captured variables as parameters) where a parameter declared `AS EXPRESSION` takes it;
providers that run a tree in memory, over a table of the RDDs (an index's `SEEK` and scope, a filter, the rest in
memory) and in SQL with bound parameters (after the SQLite RDD and FFI); and a query syntax as a `#command` of
`std.ch` (`FROM x IN source WHERE ... ORDER BY ... SELECT ...`) that becomes the chain of methods.

### Generators

Done: a function that contains `YIELD expr` returns an enumerator; `FOR EACH x IN Gen( ... )` runs it step by step.
It is a resumable function ([xir.md](xir.md#resumable-functions)) whose frame lives in the enumerator between steps,
the machinery `ASYNC` will use.

### async/await and threads

`ASYNC FUNCTION`, `AWAIT`, asynchronous generators and `AWAIT FOR EACH`, `USING`/`AWAIT USING`, tasks on an M:N
pool, threads sharing memory safely, channels and synchronization, asynchronous I/O and awaitable database operations,
in the phases of [concurrency.md](concurrency.md#phases).

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
- FFI: `EXTERN C FUNCTION` with types and structures, functions of libraries loaded at run time (`LIBRARY`, `NAME`,
  calling conventions; [architecture.md](architecture.md#where-a-c-function-comes-from)), and runtime parts written in
  xBase.
- Libraries written in xBase ([architecture.md](architecture.md#libraries-written-in-xbase)): project files, libraries
  with module files, stamps and the link step are done ([projects.md](projects.md)); next, bodies of a library that
  programs run in place (serialized xIR in its module file), guarded calls into replaceable drivers, qualified names,
  C sources in a library, then shared libraries.
- GC: free blocks returned to the operating system; statepoints and evacuation in the Immix heap
  ([xir.md](xir.md#phases)).

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
- Messages show names as written and the text of tokens.

### Front end

- A precedence-climbing expression parser driven by a table, in place of one function per level.
- One child visitor over the syntax tree for the walkers (call resolution, folding, semantic checks, xIR
  generation), and one table of block constructs (opener, middle keywords, closers) for the parser.
- Quadratic scans in the preprocessor (`[` strings, translate rescans); a statement-size limit relative to the source
  instead of a fixed token count.
- Numeric literals: a limit on decimals, and the default-decimals marker as a flag rather than the value 255.
- The back end's memory per operation (a statement of 30000 `+` takes 1 GB to compile).

### xIR and code generation

- A call writes its destination only once it succeeds (today the destination is cleared first, so under a
  `BEGIN SEQUENCE` handler its result is not computed into a variable); a pass that moves or merges operations must
  respect that an operation that fails runs the error block; a store and its write barrier move together.
- Typed continuations ([xir.md](xir.md#passes)) copy a loop nest of more than 500 instructions, or one with a
  `BEGIN SEQUENCE` or `FOR EACH` cleanup inside, nowhere: its operations that may fail still give any type. A copy
  compiled for size (outlined as a cold function) would lift the limit.
- `promote-variables` in functions with `BEGIN SEQUENCE`: the variables the handlers do not read.
- Verifier: no parameters on the entry block and no branches to it, the access kind of a variable, the captures of a
  `block`; linear cost.
- Interned constants; moving, splitting and cloning instructions; cached analyses (CFG, dominators, loops) with
  invalidation; module passes; running single passes for tests (`--xir-passes`).
- Measured plan (done: items stored and copied as two 8-byte words; inline `index` and `index.set`, also with a
  double index; inline double and mixed `+`, `-`, `*`, `/`, `%` and comparisons; type sets, with values that hold no
  heap pointer kept in registers; parameter versioning). Next, in order:
  1. Loops without slow paths: done at the LLVM level for innermost loops, and for nests whose fast copy makes no call
     (`codegen/LoopSplit.*`, [compiler.md](compiler.md#object-emission)), with alias information (TBAA) on the accesses
     whose memory is known, and their safepoint polls placed once every 1024 passes (`codegen/LoopPolls.*`). Their
     invariant tests (an array and an index the loop does not change) move before them, and what they guarded with them
     (`codegen/LoopGuards.*`); the other invariant tests of a fast loop are tested once before it, which chooses the
     loop without them or an exact copy (`versionInvariantColdExits`). Still to do: outer loops that call functions of
     the program; frame slot stores of values only an inline operation uses (they are there for the collector:
     statepoints); typed operations in xIR with sparse conditional propagation over the type sets, so that xIR passes
     see the fast cases too.
  2. Value numbering, loop-invariant code motion, range analysis and bounds-check elimination on the typed
     operations.
  3. Runtime functions as xIR operations, inlining of xBase functions, string appends in place, direct calls with
     fixed arity.

### Typed values, calls, frames and allocation (planned, next)

The open items of the measured plan cross: a value's home (an item in a frame slot, or a machine word in a register),
the calling convention, when a frame exists, what the collector sees and how objects are allocated. They are done as
one design, in this order, each step measured over every benchmark before it is kept.

1. Typed values. An operation is typed before it runs, not checked after: a guard (`subst.check` with a mask, as the
   guards of values entering loops are now) tests the types its operands must have for its fast form, right where each
   operand is made, or before the loop for one the loop does not change; a guard that fails enters a generic copy of
   the region alone (the innermost loop nest that holds the guard, or the stretch of straight code between two loops),
   not of the whole function, its live values joined there by block parameters, and both copies leave the region by
   the same exits, where the values that live on are items again. With its operands typed, an operation cannot fail
   with an error a handler substitutes (but for the errors that remain: a zero divisor, an index out of bounds, which
   keep a check after them into the same region copy), so the checks after operations, the rounds that choose them
   (which can check an operation a later guard makes safe: a `FOR` counter that starts from a guarded value) and the
   limits of 500 instructions and 64 checks a function go. The typed copy's values are machine words: integers,
   doubles (with their decimals), logicals; the frame holds items that may refer to heap objects only, so a value the
   loop does not use takes no register from it and fewer slots are cleared at the entry. Done: copies by regions in
   functions too large to copy whole or with unwind blocks, joined at their ways out and guarded again after them
   (`typed-continuations` in [xir.md](xir.md#passes)); the rest of this step is to do.
2. One calling convention. Functions, methods and codeblocks all get the direct entry functions have now (the arguments'
   words in registers, the caller's frame passed), and the generic entry becomes a thunk that reads `argv`; the
   parameters are values, not frame slots, so the frame is set up only on the paths that call or allocate (as for a leaf
   function today), also for the frames of inlined bodies; the send cache keeps the method's direct entry and a
   codeblock its own (`xb_block`), so compiled sends and `Eval`s call them; methods and codeblocks get a body for
   arguments passed by reference, as functions have. Done: inlined bodies link no frame (their positions in the
   caller's, [compiler.md](compiler.md#xir-to-llvm)); codeblocks' direct entries, with a body for arguments without
   references, which compiled `Eval`s and `xb_block_call` take ([runtime.md](runtime.md#codeblocks)); the send cache
   keeps a method's direct entry when it takes the send's arguments, which compiled sends call
   ([compiler.md](compiler.md#xir-to-llvm)); methods have their body for references as functions do. To do: frames
   set up only where needed.
3. Inline allocation: a context reaches its thread's allocation buffer, and compiled code bumps it for array and hash
   literals, objects of a known class, cells and codeblocks, the frame set up only on the slow path. Done: objects of a
   class whose objects take nothing but memory (empty arrays for `INIT {}` included), array literals whose form is
   known; the rest (hash literals, other values an object copies, cells and codeblocks) to do.
4. Dispatch: messages interned to numbers when classes are registered, one class table for the runtime's classes and
   the program's, sends to values of the built-in types cached as those to objects. Done: compiled sends perform every
   way of their cache in place, not only the newest (`virtual_dispatch` 0.77 of the cycles).

### Runtime

- The ABI check for every module, not only the start module.
- One table of type properties (heap object, `ValType` letter, sort order, `Empty`, display), with an abort on an
  unknown tag while marking.
- Number display: the cut to 16 significant digits rounds half to even (the C library's), where Clipper rounds half
  up (`1286742750677284.5` shows `...284`); integer products beyond 2^53 are exact in xbase, where the reference
  rounds them through a double (to decide).
- `Array()` with more than 64 dimensions; one substring search and one byte comparison helper; shared one-byte and
  empty strings instead of allocating them.
- GC pauses: a stack watermark for young collections (full collections clear no marks any more: their polarity
  turns around, [runtime.md](runtime.md#garbage-collector)).

## Order of the work

Each stream below is done whole before the next starts: its design, the compiler and the runtime parts, its tests and
the benchmarks it moves, so that each one is of use on its own and no step waits for a later one.

1. Strings and the runtime's primitives (planned, next). A review of every string function and operator measured
   against C, then the representation, the compiler and the runtime together: the operations the compiler knows
   (length, a byte, a prefix, a character set, appends and concatenations, iteration over a string's bytes)
   recognized on the syntax tree and lowered inline, with no call and no allocation where the value is not kept;
   shared one-byte and empty strings; allocations bumped inline in compiled code; the runtime's string functions
   written word at a time, without the C library where it costs (copies, searches, case); the codepage's paths only
   where the codepage needs them. Then the same for arrays, hashes, numbers and dates. A benchmark per primitive
   keeps each one measured.
2. Calls and objects: the cost of a call and of a send (frames, arguments, the send cache), `FINAL` classes and
   methods that the compiler calls directly, interfaces checked when the class is compiled, `ABSTRACT` methods and
   classes, and `USING` over an interface of disposal, as in .NET. Done: `ABSTRACT`, `FINAL`, `INTERFACE` and
   `IMPLEMENTS` with their checks (compiled and at run time), `IDisposable`, direct calls of a method's sends to its
   own `Self` in a `FINAL` class or of `FINAL` methods.
3. Machine types, shared by FFI and the optimizer: signed and unsigned integers of 8 to 64 bits, pointer-sized
   integers, `FLOAT`, `DOUBLE`, `BOOL`, pointers to a type, structures and unions with nested structures, arrays
   inside them, pointer fields, arrays of structures and function pointers, laid out by the target's data layout;
   memory owned by C or by the program; typed locals, parameters and runtime entry points that the compiler keeps in
   registers without checks.
4. JSON in the runtime, written in C, and the serialization of objects to JSON and back through their classes'
   reflection.
5. Asynchronous I/O and sockets, then the libraries, each a library of classes outside the runtime (`libs/`):
   `libs/xbws` (the web server), `libs/regex`, `libs/rdd/sqlite`, `libs/rdd/pgsql`, `libs/gt`.

Alongside, the benchmarks in every language follow the same code ([testing.md](testing.md#benchmarks)): a function
the runtime gains that other languages have natively is used in each port alike.
