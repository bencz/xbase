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
- Missing: Clipper's 10 significant characters of names.

### Variables

- Done: `MEMVAR`, `PRIVATE` (and `DECLARE`), `PUBLIC`, `PARAMETERS`, `M->name`, `RELEASE` (`ALL`, `LIKE`, `EXCEPT`),
  `CLEAR MEMORY`, `SAVE`/`RESTORE` (`.mem` files), and Clipper's rule for undeclared names (a memory variable at run
  time, with a warning; `-Wno-undeclared`, `-Werror=undeclared`) ([runtime.md](runtime.md#memory-variables),
  [compiler.md](compiler.md#semantic-checks)).
- Missing: a memory variable as the variable of `FOR EACH`, and the field first in the lookup of an undeclared name
  once work areas exist.

### Fields and work areas

`FIELD`, `alias->field`, `FIELD->name`, `(alias)->( expr )`, `SELECT`, and the work areas themselves (the RDD
layer: DBF, FPT/DBT, NTX, CDX, SDF, DELIM).

### Macro operator

- Done: `&var`, `&var.`, `&var.suffix`, `prefix&var.`, `&( expr )`, assignment, `&name( args )`,
  `PRIVATE`/`PUBLIC &name`, blocks built from text (`&( "{|| ... }" )`), Clipper's text substitution in strings and
  `Type()`, compiled by the runtime's own expression compiler and cached
  ([runtime.md](runtime.md#macro-operator), [compiler.md](compiler.md#grammar)).
- Missing: `o:&message`, `@&name`, aliased macros (`&cAlias->field`, with the work areas), `PCount()`/`PValue()`
  inside a macro, and the reference's macro lists (`&c` spread into arguments, its own extension).

### Errors

Done: objects of runtime classes, messages (`o:x`, `o:x := v`, `o:m( args )`), runtime errors as `Error` objects,
`ErrorBlock()`, substitution, `ErrorSys()` and the default handler in xBase, `BREAK`/`Break()`, `BEGIN SEQUENCE` /
`RECOVER [USING x]` on xIR unwind edges ([runtime.md](runtime.md#errors-and-pending-requests),
[xir.md](xir.md#control-flow-for-errors-and-exceptions)). Missing:

- Retrying (`canRetry`, `tries`) and defaults (`canDefault`) once file and database errors exist, with the default
  handler's choice between them (`Alert()`, the terminal layer).

### Procedures, parameters and the call stack

Skipped arguments and `PARAMETERS`; `DO <proc> WITH` by name at run time.

### Commands, statements and headers

- `std.ch` has 16 of 155 command families ([coverage.md](coverage.md#commands-stdch)); most of the rest need the
  runtime parts below (screen, GET/READ, files, RDD).
- `TEXT ... ENDTEXT` is not a command yet and its lines are compiled as statements.
- Standard headers still to provide: `common.ch`, `inkey.ch`, `error.ch`, `fileio.ch`, `box.ch`, `color.ch`,
  `dbstruct.ch`, `directry.ch`, `getexit.ch`, `memoedit.ch`, `achoice.ch`, `dbedit.ch`, `ord.ch`, `assert.ch`,
  `simpleio.ch`, `blob.ch`.

### Runtime library

105 of 551 functions are done ([coverage.md](coverage.md#functions)). By area, in the planned order:

1. Files and directories: `FOpen` and the low-level file functions, `File`, `Directory`, `MemoRead`/`MemoWrit`,
   `GetEnv`.
2. The terminal layer: screen, cursor, colors and keyboard, with drivers for plain standard output, ncurses and the
   Windows console; then `@ ... SAY/GET`, `READ`, `AChoice`, `MemoEdit`, `TBrowse`, `dbEdit`, menus.
3. The RDD layer and the database functions and commands.

The SIx Driver and HiPer-SEEK functions (`sx_*`, `hs_*`, `cfts*`) belong to add-on database drivers; whether xbase
provides them is decided when the RDD layer exists.

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
Destructors are done.
Missing, from the plan below: the compiled fast paths.

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
- `...` as the indexes of an element (`a[ ... ]`) and in codeblocks (`{| a, ... | ... }`); variadic functions and `...`
  as the last argument of calls and sends and the last element of array literals are done
  ([compiler.md](compiler.md#grammar)).
- Timestamps (date and time in one value) with their arithmetic and comparisons, date and timestamp literals,
  strings with escapes.
- Function references `@Name()` and symbol values; `Do()` and `Eval()` on them.
- Strings indexed like arrays (`s[ i ]`) and negative indexes counting from the end, if they fit Clipper's rules.

### Enumeration

Collections follow one enumerator model, like the .NET enumerable and enumerator interfaces: `FOR EACH` asks a
collection for an enumerator and moves it until it ends, and the collection functions walk collections the same way.
Arrays, strings and hashes keep their inline paths by position (what `FOR EACH` compiles to today); objects take part
by providing an enumerator (method names settled when classes are designed), and generators produce enumerators.
Positions, keys and values come from the loop itself (`FOR EACH k => v`), never from messages sent to the loop
variable.

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
- FFI: `EXTERN C FUNCTION` with types and structures, functions of libraries loaded at run time (`LIBRARY`, `NAME`,
  calling conventions; [architecture.md](architecture.md#where-a-c-function-comes-from)), and runtime parts written in
  xBase.
- Libraries written in xBase ([architecture.md](architecture.md#libraries-written-in-xbase)): `xbc --library` (a
  static library and its generated header), a link step in `xbc` through the system's linker, the ABI checked by every
  module; then shared libraries and project files.
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
- Messages show names as written and the text of tokens; aliases and macros give one "not supported yet" error each.

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
- Typed continuations ([xir.md](xir.md#passes)) check operations in loops of functions of at most 500 instructions;
  elsewhere, and in functions with `BEGIN SEQUENCE` or `FOR EACH` cleanups, an operation that may fail still gives
  any type. A copy per checked loop nest instead of per function, compiled for size (outlined as a cold function),
  would lift the limit.
- `promote-variables` in functions with `BEGIN SEQUENCE`: the variables the handlers do not read.
- Verifier: no parameters on the entry block and no branches to it, the access kind of a variable, the captures of a
  `block`; linear cost.
- Interned constants; moving, splitting and cloning instructions; cached analyses (CFG, dominators, loops) with
  invalidation; module passes; running single passes for tests (`--xir-passes`).
- Measured plan (done: items stored and copied as two 8-byte words; inline `index` and `index.set`, also with a
  double index; inline double and mixed `+`, `-`, `*`, `/`, `%` and comparisons; type sets, with values that hold no
  heap pointer kept in registers; parameter versioning). Next, in order:
  1. Loops without slow paths: done at the LLVM level for innermost loops (`codegen/LoopSplit.*`,
     [compiler.md](compiler.md#object-emission)), with alias information (TBAA) on the accesses whose memory is known.
     Still to do: outer loops; loads of array headers moved out of fast loops (they follow a test that can leave the
     loop: loop rotation or dereferenceability facts); frame slot stores of values only an inline operation uses (they
     are there for the collector: statepoints); typed operations in xIR with sparse conditional propagation over the
     type sets, so that xIR passes see the fast cases too.
  2. Value numbering, loop-invariant code motion, range analysis and bounds-check elimination on the typed
     operations.
  3. Runtime functions as xIR operations, inlining of xBase functions, string appends in place, direct calls with
     fixed arity.

### Runtime

- A stack limit checked in function prologues (deep recursion is a crash today).
- The ABI check for every module, not only the start module.
- One table of type properties (heap object, `ValType` letter, sort order, `Empty`, display), with an abort on an
  unknown tag while marking.
- Number display: the cut to 16 significant digits rounds half to even (the C library's), where Clipper rounds half
  up (`1286742750677284.5` shows `...284`); integer products beyond 2^53 are exact in xbase, where the reference
  rounds them through a double (to decide).
- `Array()` with more than 64 dimensions; one substring search and one byte comparison helper; shared one-byte and
  empty strings instead of allocating them.
- GC pauses: mark epochs instead of clearing every mark, a stack watermark for young collections.

## Order of the work

1. Classes (multiple inheritance, scopes) and the enumerator model; `BEGIN SEQUENCE WITH`, `ALWAYS`,
   `TRY`/`CATCH`/`FINALLY`; `WITH OBJECT`, extended codeblocks.
2. Files and directories; the terminal layer; GET/READ and TBrowse.
3. The RDD layer.
4. Generators, async/await and threads, sockets and the other libraries.

Alongside, the compiler's own track ([xir.md](xir.md#phases)): type inference and unboxed numbers, value numbering,
statepoints and a moving collector, inlining, loop optimizations, measured with the benchmarks
([testing.md](testing.md#benchmarks)).
