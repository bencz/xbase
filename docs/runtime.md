# Runtime (`xbrt`)

Code: `runtime/include/xbrt.h` (the contract with the compiler), `runtime/src/`.

`xbrt.h` is the ABI shared with `xbc`. The compiler includes it for constants only (type tags, ABI version, flags,
shared formulas) and mirrors the structures as LLVM types laid out by the target `DataLayout`; it never relies on the
host's layout. It also compiles `types/members.c` (`xbclass.h`: the rules of classes) and applies it to `xb_class_def`
descriptions it makes in memory, so that it resolves sends exactly as the runtime dispatches them. Any change to a
structure or an entry point used by generated code updates `CodeGen` and bumps `XB_ABI_VERSION`.

## Values

```c
typedef struct xb_item {
  uint32_t type;        /* xb_type */
  uint16_t width, dec;  /* numeric display width of the integer part, decimals */
  union { int32_t l; int64_t i; double d; xb_str *s; void *p; } v;
} xb_item;
```

- Types: `NIL`, `LOGICAL`, `INT` (64-bit), `DOUBLE`, `STRING`, `ARRAY`, `BLOCK`, `OBJECT`, `HASH`, `DATE`, `SYMBOL` (a
  function reference, see [Function references](#function-references)), and two kinds
  the language never shows, which designate another item (see [References](#references)): `CELL`, a variable in a cell,
  and `REF`, an element. An all-zero item is `NIL`.
- Copying an item is a plain memory copy; there is no reference counting. Items are written as two 8-byte words, the
  header and the value (`xb_put` with `xb_header_word` and the value word in `xbrt.h`), by the runtime's setters
  (`xb_set_int`, ...) and by compiled code alike, never field by field: compiled code reads an item by words, and a
  load is served by the store that wrote it only when that store covers it. The value bytes a member does not use are
  zero.
- The heap types (`STRING` to `HASH`) are one range of tags (`XB_T_HEAP_FIRST`, `XB_T_HEAP_LAST`, `xb_type_is_heap`):
  an item of another type refers to no object, so the collector need not see it.
- Strings (`xb_str`: flags, hash, length, bytes) are immutable, so they can be shared between items and threads. A plain
  string's bytes follow its header (the member `bytes`) and are NUL-terminated. A view (the flag `XB_STR_VIEW`) holds
  there instead the address of a buffer (`xb_strbuf` in `xbrt.h`: its room `cap`, its tip `used`, its bytes), a heap
  object of its own: the view's bytes are the buffer's first `len` ones, which a longer view of the same buffer may go
  on past, so they are not NUL-terminated. C code never names the member: it reads a string's bytes through
  `xb_str_data` (a test of the flag, then the header's bytes or the buffer's) and writes those of a string it has just
  allocated and not yet shared, or of one only its variable sees (`XB_STR_APPEND`, below), through `xb_str_buffer` (both
  in `xbrt.h`); code that hands them to something that reads up to a NUL (a path for the system, `GetEnv()`'s name,
  `RUN`'s command line, `Do()`'s function name, a name in an error's text, `strlen`) takes them through `xb_item_cstr`
  (`types/types.h`), which first replaces a view in the item, a root, with a plain copy of the same text. Literals
  emitted by the compiler carry `XB_STR_STATIC` and are not heap objects, nor are the empty string and the 256 strings
  of one byte (`xb_str_bytes` in `xbrt.h`): `xb_str_new` and `xb_str_concat` return those for results of up to one byte
  (`Chr()`, `SubStr( c, n, 1 )`, ...), so making one allocates nothing, and compiled code takes them from the table too.
  `Left()`, `Right()` and `SubStr()` in bytes are `xb_str_left`, `xb_str_right` and `xb_str_sub` (`rtl/strings.c`), of a
  string and integers, which raise no error: the functions call them, and so does compiled code that knows its
  arguments' types.
- A chain of `+` whose pieces may be strings (`a + "-" + b + ...`, `join-strings` in [xir.md](xir.md#passes)) makes
  one string: each `+` is a step of `xb_op_join` in its own place, which gives the pending mark (a `NIL` of width
  `XB_JOIN_PENDING`, only ever a step's operand) while the pieces so far are strings, and at the last step one new
  string of all of them; a step whose piece is no string is the `+` of the sum so far (the strings before it joined)
  and that piece, its error included, and so is every step after it. The result is the pairwise `+`s' to the byte.
- A string `+` gives a plain string of the exact size (`xb_str_concat`), unless it appends: its result goes back where
  its left operand came from, `x += y` or `x := x + y + ...` written out on the same variable, element (an index that is
  the same variable or literal) or instance variable, which the compiler marks (`grows` in
  [xir.md](xir.md#phase-1-in-detail)) and calls `xb_op_add_grow` for. An append (`xb_str_add` in `types/string.c`) gives
  a plain string when the result is shorter than 512 bytes (`XB_STR_VIEW_MIN`: up to there a copy costs no more time
  than a view and takes less memory than a view and its buffer's room). From 512 bytes on it gives a view: when the left
  operand is a view that ends at its buffer's tip (its length is `used`) and the buffer has room for the right operand's
  bytes and a NUL, they are written there, the tip moves past them and the result is a new view of the same buffer; else
  both operands' bytes go into a new buffer with room for twice the result (made after the view, which a root holds
  meanwhile). A view never changes (its length and its buffer are set as it is made) and bytes below a tip never change,
  so every copy taken before an append keeps its value; an append to a view that is no longer at the tip (another append
  from it came first) copies. Two threads may append to one shared view at once: each claims the bytes past the tip with
  an atomic compare-and-swap of `used` from the view's length, and the one that loses copies. So `x := x + y` on an
  array element, a hash value, an object's variable, a memory variable, a `STATIC` or a `LOCAL` a codeblock sees costs
  amortized constant time, as it does for a plain `LOCAL` (below): 80,000 one-byte appends to an element take about 1 ms
  where copying the string at each one took 95, and doubling their number doubles the time instead of quadrupling it. A
  view is never shorter than 512 bytes (never empty: compiled `Asc()` reads its first byte) and never has
  `XB_STR_APPEND` or `XB_STR_STATIC`; its `hash` caches its hash as a plain string's does. Its buffer stays alive while
  any view of it does, with its spare room; a large buffer gives that room back once its tip stops moving ([Garbage
  collector](#garbage-collector)).
- One exception to immutability is invisible to programs: `xb_op_append( ctx, var, b
  )`, which compiled code calls for `x := x + y` on a `LOCAL` only its function sees (`append-in-place` in
  [xir.md](xir.md#passes)), makes a string result with room to grow (twice its length, at least 32 bytes;
  `xb_str_growing`) with the flag `XB_STR_APPEND` and the room in its `hash` field (at most 4 GB; a string past that has
  no room and no flag), and while the variable still holds it with the flag, a later append copies the bytes into the
  room instead of making a new string; compiled code does that case in place and calls the function for the others.
  Compiled code ends the growth (clears the flag and sets `hash` back to 0, not computed, as `xb_str_share` does)
  whenever such a variable is read otherwise (`Len()` and `Asc()` read it in place), before the value goes anywhere, so
  a string seen by anything but its variable never changes and a hash key's hash is never the room. Other types are `+`,
  and the variable changes only when `+` gives a value.
- A string item whose `width` is `XB_STR_MEMO` is a memo field's value ([rdd.md](rdd.md#files)): `ValType()` and
  `Type()` give `M` for it (`xb_type_letter`), and every copy of the item keeps that (a variable, an element, an
  argument, a return value, and a function that gives its argument back unchanged, such as `AllTrim()` of a string
  with no blanks); any string made from it (`+`, `Upper()`, `HardCR()`, ...) is a plain `C` one. Other strings have
  `width` 0.
- Arrays (`xb_array` in `xbrt.h`: `len`, `cap`, `data`; `types/array.c`, functions in `rtl/arrays.c`) are 1-based.
  Their elements start in the array's own allocation and move to a separate heap object when the array grows past it.
  A local array that `ASize` leaves with a quarter of its room or less (of 1024 items or more) gives a peak's room
  back: elements in storage of their own move into storage twice as large as they need; elements in the array's own
  allocation stay, and the collector trims the rest of it (`xb_gc_trim`).
- An array's storage always has room for `cap` items, and the form of its elements, two bits of its state byte
  (`XB_GC_STATE_ELEMS`), says how they are stored there: as items (`XB_ELEMS_ITEMS`), not at all (`XB_ELEMS_NIL`: every
  element is `NIL`), one byte each (`XB_ELEMS_LOGICAL`: 0 `NIL`, 1 `.F.`, 2 `.T.`) or an `int64_t` each (`XB_ELEMS_INT`,
  `XB_ELEMS_INT_NIL` for `NIL`). The compact forms hold only values whose header word is their type's with no width or
  decimals: `NIL`, logicals, and integers other than `XB_ELEMS_INT_NIL`. A loop over a large array of logicals or
  integers then reads and writes a byte or a word per element where an item takes 16, which the caches hold eight or two
  times as long (`sieve` takes 59% fewer cycles). An array of the program starts as `XB_ELEMS_NIL`: `Array( n )` of one
  dimension (with more, every level holds items: the rows' first stores, one per row, would each go to the runtime, and
  the loops that fill them row by row on to their generic copies), and a literal, which takes the form that holds its
  elements (`{}` that of `NIL`s, a literal of `NIL`s alone items: placeholders, read on the path of items); the first
  value stored makes it logical, integer or items. Storing a value the form does not hold turns the array into items
  first, in place, from its last element down (an item covers only the places of the elements at or past its own): no
  allocation, so it may happen anywhere; nothing turns an array back. The runtime's own arrays (frames, task lists,
  `Directory()`'s rows, ...) are made as items (`xb_array_alloc`), and so is every array other threads may reach:
  `xb_share` turns one before it can be reached, so only a local array changes form, and only its own thread changes its
  state byte while it runs (a load and a store; a publication, which turns the arrays of the program's storage, runs
  while the other threads are stopped). C code reads and writes elements in place through `xb_array_items`, which turns
  an array of another form first, or handles the forms itself: `xb_array_get`, `xb_array_put`, `xb_array_fill`,
  `xb_array_last`, `xb_array_append`, `AIns`/`ADel`, resizing and `xb_array_snapshot` (whose copy keeps the form) in
  `types/array.c`, and the collector. `AClone` copies the form; the other array functions (`ASort`, `AScan`, `AEval`,
  ...) read through `xb_array_items` or `xb_array_get`.
- Compiled code reads `len`, `data` and the state byte to access elements in place: one test of the byte tells a
  local array of items, the usual one, from the others, and the paths of the compact forms follow
  ([compiler.md](compiler.md#xir-to-llvm)); a store whose value the form does not hold, and the first one into an
  array of `NIL`s, go through `xb_op_index_set`, which gives the array the form that holds it. `AAdd()` into a local
  array also reads `cap`: within the room, compiled code writes the element past `len`, then `len`, as
  `xb_array_append` does; past it, or with a value the form does not hold, it calls `AAdd()`.
- Hashes (`xb_hash` in `types/types.h`) map keys to values in insertion order; see [Hashes](#hashes).
- Codeblocks (`xb_block`: function, direct entry, captured cells) and cells (`xb_cell`: one item at offset 0) are
  described in [Codeblocks](#codeblocks), objects in [Objects and messages](#objects-and-messages).

## Frames

Parameters, `LOCAL`s and temporaries of a compiled function live in its item frame (`xb_frame`: link, what the call
stack shows, slot count, current line, the arguments as received, items), an `alloca` on the machine stack. The function
links it into its thread context's chain (`xb_ctx.frames`) at entry, with the parameters copied into the first slots
(from `argv`, `NIL` past `argc`, or from registers in a direct entry, [compiler.md](compiler.md#xir-to-llvm); a typed
copy keeps a parameter that holds no heap reference out of the frame) and the other slots `NIL`, and unlinks it at exit;
every exit goes through one block, and errors return normally (the pending flag), so the chain is always right. The
collector scans the frames of every context's chain. A frame dies with its function: nothing keeps a pointer to a slot
(a variable that outlives it moves into a heap cell first). The program runs on a thread whose stack is reserved at 1 GB
on 64-bit targets and 384 MB on 32-bit ones (`XB_PROGRAM_STACK_SIZE` in `core/main.c`; pages are committed as used), so
deep recursion has room.

Running out of it is an error, not the end of the process: the context knows the lowest address of its thread's stack
the program may use (`xb_ctx_set_stack`, from `xb_platform_stack_bounds`: the system's bounds of the thread's stack,
above its guard pages; none on a system that does not tell, where the error is never raised), and its stack's error
limit lies 1 MB above it. Every place where xBase code goes deeper tests `stack_limit`: a compiled function (a
codeblock's too) compares its frame's address with it after linking the frame, and below it, before its body runs, calls
`xb_stack_deep`, then unless that lets it go on `xb_stack_overflow`, and returns through its exit, unwinding; the macro
evaluator does the same for each evaluation (a codeblock made from text), and a list of arguments forwarded with `...`
is tested before it is written. That raises `EG_LIMIT` (`Stack overflow`, the function's name as the operation; it
cannot be substituted), whose handler runs in the reserve, the limit lowered to 64 KB above the lowest address
meanwhile: a `BEGIN SEQUENCE` or `TRY` recovers from it and the program goes on, the default handler reports it and ends
the program; a handler that runs out of the reserve too ends the process with `xbrt: stack overflow` (the reference's
program crashes).

A stack that goes deep gives its pages back: on a stack with room for it (the context knows its end too), `stack_limit`
is first a mark 1 MB below the end (`XB_STACK_KEEP`); the frame that passes it calls `xb_stack_deep`, which notes the
stack as deep, puts the error's limit in `stack_limit` and lets the function go on (no error, one call per descent).
Once the thread is 64 KB above the mark again (`XB_STACK_GIVE_ROOM`), at the places where it often is (a sequence that
recovers, `xb_seq_catch`; a task's step that ended; a blocking operation, a wait, once the thread counts as blocked;
after a collection's safepoint: neither delays a collection), `xb_stack_give_back` gives the pages below the mark back
to the system and watches the mark again, at most once every 100 ms per thread (`XB_STACK_GIVE_EVERY_NS`: a loop that
crosses the mark again and again stays deep in between instead of paying the system's call and the pages' faults each
time). `xb_platform_stack_give_back` frees them with `madvise(MADV_DONTNEED)` on Linux, maps a new anonymous range over
them elsewhere on POSIX systems (where the advice may only mark them), and on Windows decommits them under a new guard
page, the thread's stack limit (`NT_TIB.StackLimit`) moved up. A recursion of 120000 levels, or one the stack error
ended, leaves the resident memory where it was. The test costs a load, a comparison and a branch per call: on the `fib`
benchmark, which does little but call, 2.9% more instructions and 1.3% more cycles (perf stat, x86-64).

The frames also make the call stack. A frame's `proc` (an `xb_proc` constant the compiler emits per function: its name,
`(b)NAME` for a codeblock written in the function NAME, also inside another codeblock, where Harbour names one run by
`Eval` from a codeblock `(b)EVAL`, and its number of parameters) says what it shows, and `line` the line where the
statement running ends (a statement continued over several lines shows its last one, a codeblock the line of the
statement defining it), which compiled code stores right before each call that can see the stack (see
[compiler.md](compiler.md#xir-to-llvm)). Frames without `proc` (the command-line arguments, a module initialization, the
body of a function versioned for references while the other body runs, the arguments of a list with `...`) are not
shown. A runtime function shows only while what it does can be seen, with a frame without slots on the C stack and line
0 (`xb_frame_push`, `xb_frame_pop` in `core/context.h`): `AScan` and `ASort` while they call blocks (one written in
xBase, such as `AEval`, shows its own frame, also with line 0: [Functions written in
xBase](#functions-written-in-xbase)), and a function or message whose error raises the error block (`xb_error_in` names
it: `SUBSTR`, `ERROR:FOO` for a message to an object, `FOO` without an object, `(b)EVAL` for `Eval` of a non-block). The
body of a function or codeblock compiled code runs in place (inlined, [xir.md](xir.md#passes)) shows as its call would,
without a frame: where it runs, the frame's `line` is `XB_LINE_INLINE` with the index of a position in `xb_proc.inlines`
(`xb_inline_pos`: the inlined function's `xb_proc`, its line, and `caller`, the line of the code it was inlined into,
which may be another position); the walks of the stack go through the activations (`xb_activation_first` and
`xb_activation_next` in `core/context.h`: each position of a frame, innermost first, then the frame's function). A body
that reads its arguments as received, or an `ASYNC` function's entry, links a frame without slots of its own. Operators
and statements show nothing of their own. `ProcName( [nLevel] )`, `ProcLine( [nLevel] )` and `ProcFile( [nLevel] )` read
it: level 0 is the function calling them, a non-numeric level is 0, a negative one gives `PROCNAME`, line 0 and `""`, a
level past the end `""`, 0 and `""`. In a task the stack ends with the task's own calls (`task_base`, the frames when
its step began): the same whether the step runs on a worker or inline, on the thread of its caller (an asynchronous
function's first step) or of a waiter, so an error a task fails with keeps its own calls only. `ProcFile` gives the name
of the function's source file (`xb_proc.file`; a codeblock's is its function's), without the directories: the reference
keeps the path given to the compiler, which would put build directories into programs; a runtime function, in C or in
xBase, has none (`""`).

`argc` and `argv` in the frame are the arguments as the function received them, stored only by a function or codeblock
that calls `PCount()` or `PValue()` itself, or runs a macro, whose text may call them (`XB_PROC_ARGS` in its
`xb_proc`; nothing else can read them, and the other calls cost nothing more; a codeblock the macro evaluator made
keeps its own); they belong to its caller (slots of the caller's frame, or items the runtime keeps rooted),
which keeps them during the call. The function may replace them (`PValue()`), so a caller does not read an argument
again after the call: `HMerge` merges the entry as it was before its filter ran, and each `INIT` procedure receives its
own copy of the command-line arguments. `PCount()` and `PValue( nArg, [xNew] )` (`rtl/lang.c`) read the innermost frame
with a `proc` (a codeblock's own, also a hidden one of the runtime's xBase code; `Do`'s, with `Do`'s own arguments,
while `Do` runs; past a macro expression's, `XB_PROC_TRANSPARENT`, which is no activation of its own; a frame without
`XB_PROC_ARGS` counts no arguments): `PCount()` is `argc` (in a method's frame,
`XB_PROC_METHOD`, the arguments after `Self`, which `PValue()` numbers from 1 too); `PValue()` gives argument
`nArg` (a fraction truncated) for `1 <= nArg <= argc`, `NIL` otherwise: a parameter's current value (the first
`xb_proc.nparams` slots; a function calling `PValue()` keeps its parameters there, see
[compiler.md](compiler.md#calls)), or past the parameters the argument as passed. With `xNew` it stores `xNew` there,
through the reference when one was passed, and returns it.

## Garbage collector

`runtime/src/gc/gc.c` is a precise, generational mark-region collector with an Immix heap layout, which moves objects
out of fragmented blocks in full collections (evacuation). Strings,
string views and their buffers, arrays, array item storage, hashes, hash entry stores, codeblocks, cells, reference
targets and objects (instances of classes) are its objects (a view refers to its buffer, which refers to nothing; a
buffer's bytes past its tip change as views of it are appended to, never while a collection runs); an 8-byte header
(size, kind, state, flags) precedes each one, so object pointers are what the rest of the runtime and compiled code use.
The compiler's mirror of the layouts, a buffer's among them, is checked as the program starts (`xb_abi_check`).

- Heap: objects up to 256 bytes (header included) live in blocks of 32 KB, aligned to their size and divided into lines
  of 128 bytes. A block's first lines hold its metadata: per line, the epoch of the last collection that reached an
  object covering it and whether an old object covers it (`xb_hblock`). Larger objects, up to the largest size class
  (four slots to a block, close to 8 KB), live in blocks of one size class each: the classes are computed at start-up,
  each the largest slot that fits its number of slots in a block, and each at least an eighth larger than the one
  before. A class's block takes its objects' slots from a free list, and a dead object's slot serves the next object of
  its class, where holes of many lines would fragment the heap; a class's block left empty becomes a free block of
  lines. Blocks come from chunks of 32 blocks, each a mapping of its own from the system (`xb_platform_pages_map`,
  aligned to a block); a chunk's blocks are in use, never handed out yet (their pages untouched, so not resident), or
  given back (below), as two bit masks of the chunk say (`xb_chunk`), never the block's own metadata. A new block is a
  free one, else the first spare block of the lowest chunk that has one (made usable again,
  `xb_platform_pages_recommit`, when it was given back), else one of a new chunk: the heap's chunks stay few and low.
- Large objects (`XB_GC_HDR_LARGE`), above the largest size class, take whole units of 4 KB: a run of them in chunks of
  large objects (1 MB mappings, first fit from the lowest address, `xb_lchunk` with its bit maps of units taken and
  given back), or, from 512 KB, a mapping of their own (`xb_large`: the bytes it takes, whether it is its own mapping,
  its list's link, its header). They never move. A large object is in the young list until it is old, then in the old
  list, so a young collection sweeps the young list only. A dead one's units are free at once (its mapping unmapped).
  One that uses only its first bytes now gives the rest back in place (`xb_gc_trim`: its last units free, or its
  mapping's last pages unmapped): a string that no longer grows in place (no `XB_STR_APPEND`: its room given up once
  it was read) at the sweep that finds it alive, a string buffer whose tip has not moved since the last sweep that
  found it alive (its `cap` lowered to the room left: a later append to its tip that does not fit copies), an array
  that `ASize` cut down at once.
- Memory given back: after a collection, the free blocks beyond those the next allocations need (the young budget and an
  eighth of the blocks in use, at least 1 MB, and half as much again before any goes, so a heap near the line does not
  give blocks back and take them again at each collection) go back to the system (`xb_platform_pages_decommit`): the
  highest chunks' first, a whole page of the system at a time (blocks of a page all free or spare: one block on 4 KB and
  16 KB pages, two on 64 KB ones). They leave the blocks in use, and a chunk left with none in use is unmapped. The free
  units of large objects beyond 1 MB and the young budget go back likewise (runs of whole pages), and a chunk of large
  objects with none in use is unmapped. So the process's memory comes down after a peak: the program keeps about the
  live data and the next young budget, whatever it held before. Since collections follow allocation, a program that
  stops allocating after a peak (waiting for input, a connection, a timer) would keep it: the trimmer, a thread of the
  collector's (256 KB of stack, unattached) started when a collection first leaves a heap of 32 MB or more, runs a full
  collection once the heap has seen none for 5 seconds, and once more only after the program's next collection; its
  delay doubles after a trim that gave back less than a quarter of the heap (up to 320 seconds) and comes back to 5
  seconds after one that gave back more, so an idle program whose heap is live is not collected over and over
  (`xb_gc_trim_end` stops it as the program ends). A 1.6 GB peak dropped by a program that then only waits goes back to
  the system within about 5 seconds (resident memory 1645 MB to 8 MB), with no `__GcCollect()`.
- Allocation: every thread fills a hole, a run of free lines, by bumping a cursor, without locking (and takes the slots
  of a size class from a block of its own). That bump is `xb_gc_alloc`'s only inline path, which makes no call (no
  frame, no saved registers) and writes the header as one store; everything else is `alloc_other`'s. The hole is an ABI
  struct (`xb_tlab` in `xbrt.h`: `cursor`, `limit`, the bytes `allocated` since the thread last reported them, and
  `birth`, the flags of a new object), which the context's head points to (`tlab`, set when the context registers, NULL
  once its thread detaches): compiled code makes objects in it too, with the same rule and header (`xb_gc_kind`,
  `xb_gc_hdr`, `XB_GC_ALIGN` and `XB_GC_SMALL_MAX` are in `xbrt.h`), and calls the runtime when the object does not fit.
  When the object does not fit, it takes the block's next hole, then a recyclable block (free lines among live ones),
  then a free block. An object above one line that does not fit the hole goes to an overflow run of the thread instead
  (holes large enough for it, from the same recyclable blocks, then free ones; a recyclable block without one leaves the
  list until the next sweep), so no hole is given up for it and the free lines of recyclable blocks serve objects of
  every size.
- Roots: the frames and temporary C roots (`xb_gc_root_push`, a vector of the context that grows as C calls nest, and
  halves at a collection while it is past 1024 entries and three quarters empty, so a deep nesting's peak does not
  stay; also compiled code's, while it makes a codeblock's cells) of every registered context, and global item arrays
  registered with `xb_gc_add_root` (the modules' `STATIC`s).
- Generations: marks are sticky. An object marked by a collection stays marked and is *old*; new objects are young. A
  young collection marks from the roots and the remembered set, stops at old objects instead of traversing them, and
  frees only the young objects it did not reach. A full collection traces the whole heap without clearing any mark
  first: it marks what it reaches with a flag of the header (`XB_GC_HDR_FULL`) whose meaning each full collection turns
  around (a polarity: what the last one reached, and what was made since, has the old one), and sets the state of each
  object it reaches anew (old, in no remembered set nor aged list); a dead object keeps a stale mark no one looks at,
  and a size class's slot or a large object that a full collection does not reach goes with it. It runs when the old
  generation has grown by half the live data the previous full one found (at least 8 MB), and also when eight times that
  live data (at least 8 MB) was allocated since, when 30 seconds passed with a young budget allocated, or when 1024
  objects with destructors became old (or half as many as were old already, when more): old objects that died, and the
  resources their destructors free (files, connections), are never kept long by a program whose old generation stopped
  growing.
- Aging: a young object reached by a young collection ages, and is promoted by the next one that reaches it, so
  objects that die soon after a collection (a table whose entries are replaced) die young. An object that is old
  after a young collection and still refers to a young one stays in the remembered set (an array keeps the card
  dirty) until that one is promoted or dies. Aging adapts: it turns off when the bytes promoted at a young collection
  exceed four times the aged bytes that died (aging then only doubles their tracing; reached objects are promoted at
  once), and back on when a full collection finds that more than half of the old bytes had died.
- Young budget: a young collection runs when the allocation since the last collection reaches the budget, at least 4 MB.
  The budget doubles after a collection where more than half of the allocation survived as objects in blocks (collecting
  such objects early only promotes them; a large object's bytes do not count: marked whole and never moved, it costs a
  young collection little) and halves when less than an eighth did; it never exceeds the live data the last full
  collection found (the old generation grows to half again the live data, the young one to as much again): a program
  whose young objects live a while is collected less often (`object_churn`, `memory_footprint`, `linked_list`, `trie`,
  `csv_parse`: 8 to 20% faster, up to 16% more memory). A young collection also scans old objects, the recorded ones and
  the dirty cards of large arrays, loading what they refer to however little survives: after one, the budget is at least
  128 bytes (`XB_GC_SCAN_BYTES`) for each old reference it scanned, up to the live data, so that a program that keeps
  storing into a large old structure does not rescan it every few megabytes (`linked_list`: 17% faster, 27% more memory;
  `object_churn` 4% faster, 11% more).
- Write barrier: storing a pointer into an old object must record that object in the remembered set, because a young
  collection does not scan old objects. The state byte sits `XB_GC_STATE_BACK` bytes before the object; its bits
  `XB_GC_STATE_OLD` and `XB_GC_STATE_RECORDED` tell whether a store must record it (`xb_gc_write` in `xbrt.h`). C code
  calls `xb_gc_write(obj)` after storing into a heap object (a grown array's item storage, a reference's character) and
  `xb_gc_write_item(array, index)` after storing an array element (it returns at once for a young array, an array of a
  compact form and a value of a type that refers to no object, `xb_type_is_heap`; compiled code that stores an element
  in place calls it only for such values);
  compiled code tests the byte inline after storing into a local cell and calls `xb_gc_remember`; a store through
  anything else goes through the runtime (`xb_ref_store`). The
  barrier comes after the store, with no allocation in between, and an item a call produces never goes straight into a
  heap object: it goes through a frame slot first (a root). A recorded object is traced by the next young collection and
  leaves the set. Threads record without a lock: the thread that sets an object's bit first (atomically) lists it in a
  list of its own, which a collection merges into the remembered set.
- Card marking: array storage for 1024 items or more carries one card byte per 64 items after its last item. A store
  into such an old array marks the element's card and records the array for its cards only; the young collection
  scans the marked cards (eight card bytes per step) instead of the whole array. Storage that is replaced (the array
  grew) records the whole array. The barrier after a run of stores (`xb_gc_write_items`: a frame saved between steps,
  copied elements) records each card the run covers by an item of the run that refers to an object, whichever of
  them does.
- Collection: marking uses an explicit stack; an array of more than 4096 items is traced a slice of 4096 at a time
  (`xb_slice`), so the stack holds a slice's children, not a long array's all. An array of a compact form refers to no
  object: its storage is marked, its elements are not read. A collection keeps an object's form bits, as it keeps its
  sharing bit. The collector's vectors (the stack, the remembered set, the lists of survivors, aged and retained
  objects, the threads' lists) give back what they grew past 64 K entries when a collection uses a quarter of it at
  most. Marking an object gives the lines it covers the collection's epoch, and making it old (a full collection, a
  promotion) makes them old lines. A run of items (an array's, a slice's, a dirty card's) prefetches the object an item
  refers to eight items before marking it (the compiler's `__builtin_prefetch`, nothing where it has none): its header
  is a cache miss most of the time. A line is then free when it is not old and no object reached by this collection
  covers it: sweeping reads a block's line marks, never its objects, and makes the lists of free and recyclable blocks.
  A young collection sweeps only the blocks that received objects or hold aged ones since their last sweep; old lines
  stay until a full collection, which clears every block's (a free block has none). When the epoch (8 bits) wraps, every
  line mark is cleared. The blocks of size classes are swept slot by slot (their dead objects' slots return to the free
  list). The system's lack of memory for a block or a large object makes a full collection first (once for that
  allocation), and ends the program only when it still has none, or, for a size the program chose, raises `BASE/9032
  Memory low` (see the rules below).
- Evacuation: dead objects spread over many blocks leave holes that objects of several lines may not fit, and the heap
  would grow instead. A full collection defragments when it can: it picks the emptiest blocks of lines (at most three
  quarters of their bytes live), as many as the free blocks and a reserve of new ones (a sixteenth of the heap's blocks)
  can take, and moves every object it reaches in them into free blocks, where the copies are packed. A block's live
  bytes are those the last full collection reached in it (each full collection counts them as it marks); a block
  allocated in since then counts its lines in use instead. Lines alone would overstate what lives: an object covers
  every line it straddles, so 128-byte strings left at random offsets among dead ones keep about one and a half lines
  each, and a block whose lines are nearly all in use may hold little more than half its bytes alive. The old place
  keeps the address of the copy (`XB_GC_HDR_MOVED`; the address is stored past a string's flags, which stay readable,
  since a string is tested for `XB_STR_STATIC` before its header: a literal has none), every reference the collection
  sees is set to the copy (roots, fields, an array's own elements, a hash's entry store (its other tables follow the
  entries in it), a codeblock's cells, the collector's own lists), and the emptied blocks become free. When the heap is
  about to grow (no free or recyclable block left, no block a chunk never handed out: taking a given-back block from the
  system again is growing too), at least a young budget was allocated since the last full collection, and a quarter of
  the heap is free bytes of blocks such a collection may empty, it runs first instead, on a thread that may move
  objects. A size class's blocks are compacted too: for each class, its blocks by how full they are, the sparsest ones
  (at most half their slots taken) whose objects all fit the free slots of the fuller ones are emptied into them (no new
  memory, and only when at least 16 blocks are emptied: fewer would not pay for the move's stack scan), so a class whose
  objects died but one in many gives its blocks back. Large objects and objects too small to hold the copy's address (on
  32-bit targets, a codeblock without cells or an object without variables) never move.
- Pinning: compiled code and C code keep object addresses in registers and on the machine stack, where the collector
  cannot update them. Before moving anything, the collector saves its registers on its stack
  (`xb_platform_with_registers`) and scans every thread's stack up to its end (the system's bounds of the stack,
  `xb_platform_stack_bounds`, else the address the thread attached with): its own from there, a parked thread's from
  where it parked or entered its blocking operation (its registers saved on the stack above that point); the slots of
  the compiled code's frames are left out (they are precise roots). A block being emptied that a word points into
  (anywhere past its metadata, or just past its end: the end of its last object) stays whole, none of its objects moved
  (the pin costs that block's evacuation, and no one needs to know where objects start). The block a C root's address
  lies in stays too. A collection moves nothing when a thread's stack is unknown, when the collecting thread is not
  attached, while a thread is in a blocking operation it entered alone (below), or while C code holds the collector. A
  pointer below an object's start (such as `items - 1` for 1-based indexing) may lie in its block's metadata, which
  keeps nothing in place: C code keeps the object's address, or one inside it, across a call that may allocate.
- Frame temporaries are not cleared after each statement, so a value a temporary held stays alive until its slot is
  reused or the function returns.
- A size the program chose (`Space`, `Replicate`, `PadL`/`PadR`/`PadC`, `StrTran`'s result, `Array`, `ASize`,
  `HAllocate`, `HCopy`, `MemoLine`, `Str` with a width) that no memory holds, even after a full collection, is the error
  `BASE/9032 Memory low` (`EG_MEM`), which the program may handle (`xb_gc_try_alloc`, `xb_str_try_alloc`,
  `xb_array_try_alloc`, `xb_array_resize_try`, `xb_hash_reserve_try`: for a large object); the reference ends the
  program (`xb_error_memory` raises it). A string asked longer than any the runtime makes (past `XB_STR_MAX_LEN`, or a
  `Str`/`StrZero` width past `INT32_MAX`, the widest field the formatter takes) is `BASE/1234 String overflow`
  (`EG_STROVERFLOW`, `xb_error_overflow`) instead, whatever memory there is; the reference falls back to the default
  width for such a `Str` width. Other allocations that find no memory (the runtime's own, and objects up to the largest
  size class), a lock that cannot be created and an ABI mismatch end the program through `xb_gc_fatal`, the runtime's
  single fatal exit, with a message on standard error and exit code 1.
- Rules for C code: a collection can run inside any allocation, so every object still needed must be reachable from a
  frame or a C root before the next allocation, and a store into a heap object that may have aged since it was allocated
  (any allocation in between) is followed by `xb_gc_write` or `xb_gc_write_item`. An object address in a C variable
  stays valid across a collection (the stack scan pins the object), but an address kept anywhere the scan does not see
  (in `malloc`'d memory, as a map's key) is valid only between `xb_gc_hold()` and `xb_gc_release()`, during which no
  collection moves anything (`xb_clone_deep` keeps such a map). What may block the thread (a file's I/O, a write to or
  flush of the standard output or error) runs through `xb_blocking_run`, a wait through `xb_sleeping_run`: it allocates
  nothing and changes no reference, and the objects it reads or writes are rooted (`FRead()` roots the string a read
  fills, the console the text it writes), since another thread may collect meanwhile. A runtime lock held across a
  safepoint or a blocking operation (the registry of shared files', whose holder may park on its way back from a file
  operation; the lock of class preparations, whose holder runs xBase code) is waited for in a blocking operation; others
  (the heap's, the memory variable names') are never held across one.
- Threads: a thread that runs xBase code attaches (`xb_gc_thread_attach`, by its first function) and detaches before it
  ends; the second to attach publishes the program's storage first (`publish`: the world stopped as for a collection,
  the items of the roots outside frames, the `STATIC`s and class variables, shared with `xb_share`, every context's
  memory variables with `xb_memvar_share` and the PUBLICs with `xb_memvar_share_publics`, then `xb_storage_shared` set;
  a root added later is shared as it is added, and so is a PUBLIC made later).
  From then on `xb_ref_load` and `xb_ref_store` with the type `XB_T_NIL` read and write a `STATIC`'s own item under the
  stripe of its address (compiled code tells the runtime a `STATIC` is one: `is_static` of `xb_cell_of`, `xb_ref_of` and
  `xb_each_shared`, `XB_EACH_STATIC` in the `how` of the other `FOR EACH` functions; `xb_cell_move` moves one into a
  cell under that stripe, the cell shared first, and `xb_each_shared` makes a `FOR EACH` over one go through its cell;
  `xb_each_move` leaves alone a variable that no longer holds its loop's reference, which another thread's loop over the
  same variable replaced), class variables are read and written under the stripes of their items, and a class prepared
  later shares its initial values and class variables before it is ready. Publication stops a thread at any safepoint,
  an allocation's among them: a local path that allocates (an array's or a hash's growth, a generator's frame, a
  `STATIC`'s cell) tests the object's bit again after the allocation and goes on as for a shared object when it was
  shared meanwhile. Memory variables made after it stay the environment's (shared environments come with `ThreadStart`).
  An attached thread that waits for another waits in a blocking operation, as for a collection; the allocation fast path
  takes no lock, the global structures one (`xb_mutex`). A collection stops the world: the collecting thread sets
  `stopping` and `XB_PENDING_SAFEPOINT` on the contexts of the other threads still running (each context knows the
  thread that registered it; a request served for nothing would send that thread's loops to their generic copy for an
  iteration, [compiler.md](compiler.md#object-emission)), then waits (on a condition) until no other attached thread is
  running. A running thread parks at its next safepoint (a poll of compiled code that serves the request,
  `xb_pending_gc`; the slow path of an allocation; a change of the contexts), its registers saved on its stack, until
  the collection ends. The request is set with release and read with acquire, and the collector raises it again each
  millisecond it waits, so a thread that served it before `stopping` reached it parks at its next poll. A thread in a
  blocking operation counts as stopped: the operation runs inside `xb_platform_with_registers`, so the registers of
  every caller stay saved on the stack while it runs (a thread that is the only one attached skips that, nothing else
  collecting then, and marks itself `unsaved`: a thread that attaches meanwhile and collects moves nothing until the
  operation ends); the thread publishes where its stack is in use and its state (sequentially consistent, as the
  collector's `stopping` is, so neither misses the other), and on its way back it parks if a collection runs. A second
  thread that wants to collect parks until the first one's collection ends, then sees whether its own is still needed.
  A collection starts only once the threads the last one parked have gone on (each tells the collector as it leaves
  its park): a thread that collects again and again (`__GcCollect()` in a loop) never keeps the others parked from one
  collection to the next.
  What holds a collection up is code that reaches no safepoint: a runtime loop of C alone (a sort without a block) for
  as long as it runs.

`__GcCollect( [lYoung] )` runs a full collection (a young one with `.T.`), then the destructors of the objects it
found unreachable ([Classes](#classes)), and returns the number of live objects the last full collection found (tests
use it). `__GcMemory()` returns `{ live objects, the heap's memory in bytes (blocks in use, large objects' units and
mappings: what was given back is not counted), the process's resident bytes (0 where the system does not tell), the
heap's memory without the free blocks it keeps for the next allocations, the bytes allocated since the program started
(`xb_gc_allocated_bytes`: what an operation costs in allocation, the same on every run; another thread's bytes count
once it reports them, at a collection, a slow allocation or its detach), the young budget (`xb_gc_young_budget`), the
bytes of the live objects the last full collection found (`xb_gc_live_bytes`) }`.

Finalization: an object registered with `xb_gc_finalizable` (an object whose compiled class has a destructor, or of a
runtime class that has one, `xb_class.destructor`: a failed task nothing observed) that a collection does not reach
moves, after marking, to a queue of finalized objects, and is marked with what it refers to (the queue is a root until
`xb_gc_next_finalized` hands it out); the collection then raises `XB_PENDING_FINALIZE` in the context that runs the
destructors: the program's (the first one registered, then another when it goes), or, while its thread is in a blocking
operation (a `Join()`, a wait, where it would test nothing for as long), the collecting thread's, the one whose
allocation found them. One context runs destructors at a time (`xb_gc_finalizing`); objects queued while it ends its
turn make it run again at its next test. The objects with destructors are listed young and old: a young collection looks
at the young ones only.

## Hashes

`types/hash.c` holds the hash itself, `rtl/hashes.c` the `H*` functions (`HEval`, `HScan` and `HMerge` are in
`rtl/arrays.prg`), and `rtl/collections.c` what they share with the array functions: argument ranges, block calls
from C, the order of values, the stable merge sort and deep copies. A hash other threads may reach is used through
the functions that take its lock when it is shared, but for a lookup by key and its length, read under its sequence
counter ([Threads and synchronization](#threads-and-synchronization)).

- Keys are strings, numbers and dates; values are anything. A number that equals an integer is that integer as a key,
  so `1`, `1.0` and `-0.0` against `0` are each one key, also in a literal (`{ 1 => "a", 1.0 => "b" }` has one entry,
  `"b"`); other doubles are equal when their bits are. Strings compare byte for byte, or with ASCII letter case aside
  when the hash ignores case. A date never equals a number.
- Layout: one store (`XB_GC_HSTORE`) holds the entries (key and value items) in insertion order, then their 32-bit key
  hashes, then a Fenwick tree with a counter per entry, then, for a store of more than `XB_HASH_SMALL` (8) entries, an
  index of twice as many slots holding entry numbers plus one (0 is free); the hash finds those tables from `entries`
  and `cap` (`imask`, the index's slots less one, is 0 without one). Lookups probe the index linearly from the key
  hash, comparing the dense array of hashes (an eighth of the entries' size, which caches keep when they cannot keep
  the entries) before an entry's key; the index is at most half full. A store without an index is searched in order:
  first for the same key item (the same string, as a literal key is, or an equal number of the same type), then by
  hash. A local hash keeps the entry a lookup found last (`hint`, plus one), and its lookups and assignments try it
  first, so `k $ h` followed by `h[k] += 1` finds the key once; a deletion clears it, and it is compared by key, so a
  store moved or compacted meanwhile only makes it miss (a shared hash's lock-free lookups neither read nor set it; a
  deletion searches by hash). An insertion takes the free slot the lookup before it found. Deleting an
  entry frees its index slot by moving back the slots after it that probing would no longer reach (no tombstones) and
  leaves a hole (a `NIL` key) in the store. A full store whose holes are a quarter of it is compacted (the live
  entries move down in order); otherwise it doubles. A new hash has no store; the first holds 8 entries, the largest
  `XB_HASH_MAX_LEN` (2^30). `HAllocate` past it, or past what memory holds, is `BASE/9032 Memory low`. A cleared
  hash (`HClear`) of more than 64 entries' room drops its store (O(1), the room given back; the next key makes a small
  one); a local hash that a deletion leaves with an eighth of its room or less (above 64) moves into a store twice as
  large as it needs, so a cache whose keys came and went gives its peak's room back.
- Positions (`xb_hash_at`, `xb_hash_pos`: `HKeyAt`, `HPos`, `FOR EACH`, ...) count live entries in order. Without
  holes position k is entry k. With holes the Fenwick tree, built over the entries when a position is first needed
  and updated by every insertion and deletion until the next compaction, finds the entry of a position, or the
  position of an entry, in O(log n); so deleting from the front, or while walking the hash, stays cheap. The functions
  that go over every entry (`HKeys`, `HValues`, `HFill`, `HSort`, `HScan` with a value, `HCopy`) compact first
  (`xb_hash_compact`), which keeps positions as they were.
- Key hashes: a string's is `xb_hash_bytes` (`xbrt.h`), which takes its bytes eight at a time as a little-endian word
  (`xb_hash_word`: a pattern compilers make one load, with a byte swap on a big-endian target), so every target, little
  or big endian, 32 or 64 bits, computes the same value. The compiler stores it in the `hash` field of the strings it
  emits, and the runtime caches it there for heap strings (0 means not computed). A hash that ignores case hashes the
  upper-cased letters. Numbers and dates are scrambled by `xb_hash_mix64`. Two string keys that are the same string (a
  literal key, a key read back from the hash) are equal without comparing their bytes.
- Flooding: those hashes are fixed, so anyone can choose keys whose hashes share their low bits (integers through the
  inverse of `xb_hash_mix64`), and each probe would walk them all: n insertions in n^2 / 2 steps. An insertion whose
  probe passes 128 slots, which keys that do not collide on purpose never make (the index is at most half full),
  turns that hash keyed for good (`XB_HASH_KEYED`, `types/hash.c`): every key's hash is computed again by SipHash-1-3
  under a key the process draws from the system's randomness when the first hash needs it (`xb_platform_random`:
  `/dev/urandom`, `rand_s` on Windows; the clock and addresses of the process where neither can be read), and the
  hash cached in a string is not used. Its keys then take as long as any others (50,000 integers that all hash to one
  value: 3.4 s before, a few milliseconds keyed, `tests/run/hash_flooding`); a key costs SipHash's few nanoseconds
  more.
- `HCaseMatch( h, .F. )` rebuilds the index ignoring case. Keys that then become equal merge into the first of them,
  which keeps its spelling and position and takes the value of the last, so a key always names one entry.
- Elements: `h[k]` reads the value; a missing key is `Bound error: array access` (1132), a value that cannot be a key
  `Argument error: array access` (1068, whose handler may substitute the value; also for a literal such as `h[ .T. ]`,
  in compiled code and in a macro, where the reference rejects it when compiling). `h[k] := v` replaces the value in
  place or adds the key last; a value that cannot be a key is `Argument error: array assign` (1069). A reference to
  `h[k]` (`@h[k]`) designates the entry of key `k` (`xb_ref.key`, the entry's own key as it is spelled): it finds it
  again at every use, wherever deletions move it, and using it once the key is gone is the bound error (1132). The `FOR
  EACH` reference designates a position; a codeblock or a reference that takes the loop's current element gets the
  entry's key instead. A literal `{ k => v, ... }` is built by `xb_hash_new` from frame slots; a value that cannot be a
  key raises `Bound error: array assign` (1133) with the hash built so far, the key and the value as arguments.
- The `H*` functions raise `Argument error` (1123) for a first argument that is not a hash or a key that cannot be one,
  and `Bound error` (1187, named after the function, `HDelAt` included, where the reference implementation gives 1133)
  for a position outside `1 .. Len( h )`. `Hash( k1, v1, ... )` with a key and no value is `Bound error: array
  dimension` (1131), and a value that cannot be a key the error of a literal with one (1133). `HCopy( hSource, hDest,
  [nStart], [nCount] )` takes the source first and returns `hDest`; `HMerge( hDest, hSource, [bFilter] )` takes the
  destination first. `HAllocate` returns the hash.
- Walking a hash that changes: every walk reads the entry at a position when its turn comes, so the entries after a
  deleted one move down a position. `FOR EACH` and `HMerge` check each position against the current length, so they
  reach the entries a block adds; `HEval` and `HScan` walk the range fixed when they start, and stop at a position that
  no longer exists. `HSort` without a block orders keys as the hash knows them (`xb_hash_key_order`): numbers by value,
  then dates, then strings byte by byte (a prefix first; ASCII letter case aside when the hash ignores it), a total
  order, so equal-looking keys such as `"a"` and `"a "` keep no insertion order (the reference ignores trailing blanks
  there). It sorts positions and puts the entries back only when its block left the number of entries alone (otherwise
  the hash stays as the block left it). No change made by a block leaves the hash inconsistent.
- Copies: `HClone` and `AClone` copy the arrays and hashes inside too (`xb_clone_deep`), each once, so shared and
  cyclic parts stay shared and cyclic in the copy; objects and codeblocks are shared. The copy works through a list
  of copies to visit, not recursion, so the depth of the structure has no limit, under a map of the copies made; a local
  array or hash that holds no array or hash (most of them, and the usual `INIT {}` of a class) is copied whole instead,
  without the map (`clone_flat`). `HCopy` and `HMerge` copy entries only.
- Collector: a hash (`XB_GC_HASH`) is traced by marking its store and the keys and values of its entries; the store
  holds no pointers of its own to trace. C code calls `xb_gc_write(hash)` after storing into an entry.

## Codeblocks

A variable that a codeblock captures, or that is passed by reference, moves into a cell when that first happens: its
value moves into a new cell and an `XB_T_CELL` item is left in its slot or `STATIC` storage (a parameter passed by
reference already holds its caller's). `xb_cell_of` returns the cell a codeblock captures; for a `LOCAL` or parameter
that holds the reference of a `FOR EACH` loop it returns a new cell holding the current element, and the variable stays
the loop's. The function and every block that captured it share the cell, which outlives the function. `xb_cell_new`
makes a cell holding a copy of a value: a block's capture of a variable by its value (one nothing assigns after the
block is made, [compiler.md](compiler.md#ast-passes)), which the block alone refers to. A codeblock is a compiled
function `(ctx, ret, argc, argv, self)` plus the captured cells (`self->cells[]`); `xb_block_new` creates it and the
compiled code stores the cells. A compiled block with parameters also has a direct entry (`xb_block.direct`, `NULL` for
a macro's block, a `MemVarBlock()` one, a block without parameters and one that reads its arguments as received) taking
the context, the result's item, the two words of each of exactly `nparams` arguments, the caller's frame (the context's
chain head) and the block (`xbrt.h`); none is a reference when the block has a body for arguments without references
([compiler.md](compiler.md#syntax-tree-to-xir)), whose entry it is then. `Eval( bBlock, ... )` and `xb_block_call` run
the block's function, the caller having set the result to `NIL`. That function, for a compiled block with a direct
entry, is a thunk: it reads the arguments (`NIL` past `argc`; those past the parameters left out) and calls the direct
entry, or, for a block with a body for arguments without references when one of them is a reference, the block's own
body. Compiled code calls the block of an `Eval` itself: its direct entry when no operand is a reference and the block
takes as many parameters as arguments, or fewer when at most four arguments are given, else its function; `Eval` for
anything else.

## Function references

`@Name()`, in compiled code or in a macro, is a value that designates a function: an `XB_T_SYMBOL` item whose `v.p` is a
static `xb_funcref` (`xbrt.h`: the upper-cased name, the function's entry (`xb_func`), whether it is a runtime function,
and the mask of the parameters that take references, as for `Do()`). It is not a heap object, so the collector ignores
it. Compiled code emits one `xb_funcref` per function a module references (`.xb.funcref`), for its `STATIC` functions
too; a macro uses the one in the symbol table's entry (`xb_symbol_entry`, see [Program startup](#program-startup)), and
a name that is no function there is `BASE/1001` where the macro's code takes the reference, as for a call to it
(`Type()` gives `U`; in compiled code the link fails, as for a call). The reference implementation makes a symbol of a
missing name, which fails only when it is called; xbase reports the missing function where the reference is taken.

- `ValType()` and `Type()` give `S`, `ClassName()` gives `SYMBOL` (a `SCALAR CLASS SYMBOL` gives it methods), `?`
  shows `@NAME()`, and `Empty()` is `.F.`.
- `==` is true for two references to the same function (so is `$` of a pair whose values they are); the other
  comparisons, `$` and arithmetic are the operators' argument errors (`=` 1071, `!=` 1072, `+` 1081, `$` 1109, ...), a
  reference as a hash key is `BASE/1133`, and `AScan()` by value never matches one.
- `Eval( p, ... )`, `Do( p, ... )` and `p:Exec( ... )` call the function with the arguments (`xb_funcref_call`: a
  reference goes on to a parameter that takes one, as in `Do()`); `p:Name` gives its name.

## Objects and messages

An object (`xb_object` in `xbrt.h`, part of the ABI; `types/object.c`) is an instance of a class: its class and one item
per instance variable. The runtime defines classes of its own in C (`xb_class`: name, variables and methods), and
programs define theirs in xBase ([Classes](#classes)). `xb_op_send(ctx, ret, message, argc, argv)` sends a message to
`argv[0]` with the arguments `argv[1 .. argc-1]`; the compiler passes the message upper-cased, and an assignment `o:x :=
v` sends `_X` with `v`. An argument written `@x` reaches a method compiled from xBase as the reference; an instance or
class variable assigned, and a method of a class of the runtime's own, take the value it designates (`xb_args_values`).
A send the compiler resolved for the object's known class reads and writes the variable itself, or calls the method
(`xb_op_object_get` and `xb_op_object_set` for a shared object). Compiled code sends through `xb_op_send_from`, with the
send's inline cache and the class of the method it is in ([Classes](#classes)), after performing in place what the
cache's newest way holds for the object's class (an instance variable read or written, a method called:
[compiler.md](compiler.md#xir-to-llvm)).

- An object's variables: `X` returns the variable (arguments are ignored), `_X` assigns it (without an argument, `NIL`).
  A variable's rule (`xb_var_rule`) says what it accepts: any value; only an array, a logical or a string (a value of
  another type is ignored); or a number, kept as an integer truncated toward zero and wrapped to 32 bits signed or 16
  bits unsigned (0 for a double beyond 64 bits), where a value of another type raises `BASE/0` `Argument error` with
  the variable's name as the operation. `_X` returns the value kept, except that an ignored array or logical gives
  `NIL`, a string variable returns its argument even when it ignores it, and a 16-bit variable returns the number
  wrapped to 32 bits only. That result is the value of `o:x := v` (see [compiler.md](compiler.md#syntax-tree-to-xir)).
  A new object holds its rules' empty values (`NIL`, `.F.`, `""`, 0). A variable whose name starts with `(` is the
  runtime's own: no message reaches it, and `ObjVars` leaves it out.
- A runtime class's methods (`xb_class_method`: an upper-cased name and a C function that receives the object as
  `argv[0]`), found after its variables (`xb_class_method_of`): `X` calls the method named `X`, an assignment the one
  named `_X`, which `ObjMethods` leaves out (`ObjHasMethod` finds the method `X`). Generators (`MoveNext`) and the
  classes of [threads](#threads-and-synchronization) (`Thread`, `Mutex`, `MutexHolder`, `Condition`, `Semaphore`,
  `AtomicInt`) answer their messages so.
- `ClassName` works on every value: an object's class name, else `NUMERIC`, `CHARACTER`, `ARRAY`, `BLOCK`, `NIL`,
  `LOGICAL` or `DATE`.
- Any other message raises `BASE/1004` `No exported method` (`BASE/1005` `No exported variable` for an assignment)
  with the message name (without `_`) as the operation, also when the receiver is not an object.

`ValType` of an object is `O`; `?` shows it as nothing, `Empty` is `.F.`, `==` tests identity and the other
comparisons are argument errors; `ASort` ranks objects with arrays, and `AScan` never matches one. An object is not an
array: `Len( o )` and `o[ i ]` are argument errors (Harbour, whose objects are arrays inside, gives the number of its
items and an item).

The `Error` class (`core/error.c`, `ErrorNew()`) has the variables `args` (an array), `canDefault`, `canRetry`,
`canSubstitute` (logicals), `cargo` (any value), `description`, `filename`, `operation`, `subSystem` (strings),
`genCode`, `osCode`, `subCode` (32-bit numbers), and `severity` and `tries` (16-bit numbers); a hidden one (no message
reaches it) holds the lines of the calls where the tasks it failed raised it (`xb_error_add_calls`, `__ErrorCalls()`):
`XB_CALLS_SHOWN` lines at most, the last one `XB_CALLS_NOT_SHOWN` when more were left out, each addition made only on
the text it read (`xb_object_replace`), so tasks that fail with one error at once lose none.

### Enumerators

`FOR EACH x IN o`, for an object of a compiled class, goes through an enumerator (`core/control.c`): once before the
loop, `xb_each_source` takes `o:GetEnumerator()`, or `o` itself when it answers `MoveNext` (calling its `Reset()` first
when it has one); the enumerator answers `MoveNext()` (`.T.` while there is an element) and `Current` (a method, an
`ACCESS` or a variable). Each iteration's length test (`xb_enum_len`) sends `MoveNext()` (a length as large as can be
while it gives `.T.`, else 0), and `xb_each_move` puts `Current` in the loop reference's own item: the variable reads
it, and assigning the variable changes nothing in the collection. A generator's enumerator is stepped directly
(`xb_gen_move_next`, its `Current` copied); compiled code does both in place for a local one (docs/compiler.md). `k` in
`FOR EACH k => x` is the position. With `DESCEND` the enumerator's elements are gathered into an array first. An object
without an enumerator is the error of any other value that is no collection (`BASE/1068`). The paths of arrays, strings
and hashes come first and do not change. `AWAIT FOR EACH` (`core/control.c`) takes its asynchronous enumerator from
`__AwaitEach`: the object itself when it answers `MoveNextAsync`, what `GetAsyncEnumerator()` gives, else what `FOR
EACH` would go through (`xb_each_source`), itself when that answers `MoveNextAsync` (a channel's enumerator), else an
`AwaitEach` object whose `MoveNextAsync` takes the next element at once (a task completed with `.T.`, `.F.` at the end)
into its `Current`, keeping a hash's key (`__AwaitEachKey`); the loop awaits that `Current` (`__AwaitEachAwaits`), so a
collection of tasks gives their results in order.

### Generators

`types/generator.c`. A generator's enumerator is an object of the runtime class `GENERATOR`: `Current` (a variable no
message assigns, `XB_VAR_READ`) is the last element, `MoveNext()` (a method of the class) runs one step
(`xb_gen_move_next`), its other messages are the `Enumerable` class's (`xb_gen_message`), and its own variables, whose
names no message reaches (in parentheses: `xb_var_answers`, whatever text a message comes from, `ObjSend` or a macro),
hold the state, the resume function (its address as an integer) and the frame between steps (an array). Their indexes
(`XB_GEN_CURRENT`, `XB_GEN_STATE`, `XB_GEN_FRAME`, `XB_GEN_RESUME`), the ended state (`XB_GEN_ENDED`) and the class
(`xb_generator_class`) are in `xbrt.h`: compiled code steps a local enumerator and saves and restores its frame in
place, with the same rules. `xb_gen_new` makes it with the function's arguments in frame slots 1 .. n (slot 0 is the
enumerator, the resume function's argument); at each step `xb_gen_restore` copies the saved slots into the resume
function's frame and returns the state, marking the enumerator ended until the step yields (a step that fails, or a
`MoveNext` inside the step, ends it); a shared enumerator's step is claimed first, under its lock (its state `s` becomes
`-(s + 2)` until the step yields), so a `MoveNext` from another thread meanwhile answers `.F.` and touches nothing, and
one thread at a time touches the frame (a `MoveNext` and the `Current` after it are two operations: another thread may
step between them); `xb_gen_yield` saves the frame (the barrier for its run of items, `xb_gc_write_items`), the state
and the element; `xb_gen_done` ends it and drops the frame, at the end of the body and after a step that fails
(`Current` is then NIL). `FOR EACH` calls the step and reads `Current` (`xb_gen_current`, whole); its other messages are
those of the runtime's `Enumerable` (`g:Where( ... )`), the class its own class function makes (`xbf_ENUMERABLE`,
`xb_class_send_as`): a program's class named `Enumerable` is another one. A message neither answers is the usual
`GENERATOR:message` error. The `generator` benchmark (3,000,000 steps of one generator feeding another) takes about 60
ms through the enumerators; a `FOR EACH` over a generator of the module it calls itself runs the generator in place
([xir.md](xir.md), `fuse-generators`): 2 ms. The frame between steps is saved and loaded by `xb_frame_store` and
`xb_frame_load` (`types/types.h`), which asynchronous functions use too: in place, each value shared first when the
holder is shared (what a shared object holds is shared), and with no lock on either side: one step runs at a time, and
the next one runs after the handoff that publishes it (a scheduling, a claim), so no thread reads the frame while
another writes it (storing the slots under the array's stripe would cost an `ASYNC` function with twelve locals,
suspended a million times, 24% more instructions); `tests/run/async_frames_threads.prg` checks frames handed between
workers.

### Asynchronous functions

`rtl/tasks.c` ([concurrency.md](concurrency.md#asynchronous-functions)). A step runs from a record on the C stack
(`struct xb_async_step`, `ctx->async_step` while it runs, its items rooted): the task (NIL in the first step until a
suspension makes it), the task it suspends for, the resume function, the environment, and at the first step the caller's
arguments. `xb_async_new` (the function's entry) runs the first step at once: an environment of its own
(`xb_env_inherit`), the resume function with NIL as its argument, `xb_async_restore` copying the arguments into frame
slots 1 .. n. `xb_async_suspend` makes the task at the first suspension (kind `K_ASYNC`, running; the resume function,
the environment's address), saves the frame into it (`V_WORK`) with the point (`V_POINT`, an integer only the next step
reads) and hands the awaited task to the step record; the step returns, its environment is parked (`xb_env_park`), the
task shared and made `created` and registered as the awaited task's continuation, last: that task's end schedules it
(`fire`, as a `TaskRun` continuation), and `run_step` runs the next step on a worker: the environment unparked, the
resume function with the task, `xb_async_restore` giving the frame back. A step that ends completes the task (one made
ended, for a first step that made none) or fails or cancels it as a block's task (`step_ended`); its environment ends
(`xb_env_leave`, its tables closed). `xb_await_ready` tells an `AWAIT` whether it goes on at once (no task, or one that
ended), `xb_await_result` gives the value (`outcome`, as `Result`), `xb_await_wait` waits first (outside an `ASYNC`
function). A fused `AWAIT` (`xb_await_fuse` before the call, `xb_await_fused` after it) names the entry and the frame
that calls it in the context (`async_fuse`, `async_fuse_frame`; for a message, `async_fuse_send`, which the dispatch
turns into the compiled method it reaches when that runs right from the frame, `xb_fuse_target`, and ends for code a
message to a value runs, `xb_object_message`); `xb_async_new` answers when it is that entry, its frame right above that
one: a first step that ends gives its result itself (`async_done`), no task made. A method of a runtime class reached
right from that frame is told so while it runs (`async_fuse_method`, set by `xb_runtime_perform`, taken by the method
first thing with `xb_await_method`, so that what it runs after sees none): `Channel:ReceiveAsync` with a value there
answers with the value itself (`xb_task_answer`), no task made (a `TrySend` and an `AWAIT ch:ReceiveAsync()` of its
value: 860 instructions, 1650 with a task completed already). A request ends at an error (`raise`), where an unwinding
stops (`xb_seq_catch`) and before a task's step, so none outlives the `AWAIT` that made it. A step runs as its
function's flow (`xb_flow_enter`; the number in the task, `V_FLOW`), so a lock it holds goes with it across `AWAIT`.

### Asynchronous generators

`rtl/agen.c` ([concurrency.md](concurrency.md#language)). An asynchronous generator's enumerator is an object of the
runtime class `ASYNCGENERATOR` (`xb_agen_new`): `Current` (read only), a state word (idle, running, yielded, done), its
resume function, its arguments until the body starts, the task of the `MoveNextAsync()` that waits, the body's task once
it parked at a `YIELD`, and an element kept apart. The body runs as an asynchronous function's steps (`rtl/tasks.c`)
whose step record knows the enumerator (`xb_async_step.gen`; the body's task keeps it in its `V_TASKS` variable, which
no combinator uses there). At a `YIELD`, `xb_async_yield` saves the frame and parks the task: it waits for nothing (no
continuation, and it is not shared: only the enumerator reaches it, which shares it as it takes it when it is shared
itself), and the enumerator takes the element and, the first time, the task, the same to the body's end
(`xb_agen_yield`): the element is `Current` at once, or, when a token may cancel that `MoveNextAsync()` (a flag in the
state word), kept apart until the step's end makes it `Current` (dropped when it was canceled). Each variable is written
only when it changes, since a shared enumerator's writes take a stripe's lock. `MoveNextAsync( [oToken] )` takes the
state to running with one compare-and-swap; from idle it runs the first step on the caller's thread
(`xb_async_gen_start`, the arguments copied where nothing moves them, any fused `AWAIT` request ended), from yielded it
resumes the parked task (`xb_task_resume`: its step right there when the stack has room, else the task shared and
scheduled on a worker). Its task is made before the step only when a token may cancel it or the step goes to a worker;
otherwise the step tells how it ended to that `MoveNextAsync()` on its thread (a thread-local request): a step that
yielded or ended gives the task completed with `.T.` (the one task all share) or `.F.`, one that failed a task failed
with the error, and one that suspends at an `AWAIT` makes the waiting task then, before anything can resume it
(`xb_agen_step_awaits`). After every step's own end, `xb_agen_step_ended` settles the waiting task (claimed, unless its
token canceled it): a step that yielded leaves the state yielded and completes it with `.T.`; one that ended completes
it with `.F.`, one that failed fails it with the error, a `QUIT` cancels it, and those leave the state done (`.F.` from
then on). The state is stored before the task completes, so its waiter finds it. A body's task canceled before it ran
again (queued at the program's end, `xb_task_cancel`) cancels that waiting task too. A `MoveNextAsync()` decides once
whether the step runs on its thread (`xb_stack_room`), and passes that decision on. A canceled task ends the
enumeration: while the body runs, a `MoveNextAsync()` that finds its waiting task ended answers `.F.`, and the step that
then yields finds it canceled and makes the state done. Another `MoveNextAsync()` while one waits raises `BASE/9035`.
`MoveNext()` is `MoveNextAsync()` waited for (`xb_await_wait`: a failure raised as `Result` raises it). A million
elements of a body that does not suspend, through `AWAIT FOR EACH`: about 85 ms (half of it the parking of the body's
environment).

## Classes

Classes compiled from xBase (`types/class.c`, `xb_class_def` in `xbrt.h`) are described by their module and
registered with it by name (a `STATIC CLASS` is not: it is known to its module only); each method by its message, its
function and, when it has one, its direct entry with the parameters it takes and how it returns
(`xb_class_method_def`). The runtime prepares a class the first time it is used (its class function, or a subclass):

- A parent is the module's own class of that name when there is one, else the class registered under the name (an
  unknown one is error 1001, as its class function would give). The class's line is the class, then each parent's
  line, depth first and left to right, each class once. Each class of the line has an area of the object's variables
  of its own: two ancestors' variables of the same name are two variables, each seen by its class's methods (the
  reference keeps one, the first ancestor's). A class met again while its line is built, with no INIT values
  evaluated meanwhile, inherits from itself (`BASE/0` `Class inherits from itself`, the class's name as the operation;
  the compiler reports it within a module).
- `ABSTRACT`, `FINAL` and interfaces (`xb_class_check`, one of those shared rules): when its tables are made, a class
  is checked against them, and one that breaks a rule makes no objects (its class function gives `BASE/0` with the
  rule as the description and the class's name as the operation; an `ABSTRACT CLASS` the same, `an ABSTRACT class
  makes no objects`; a `SCALAR CLASS` that breaks one, its values' messages): a class of its line after it that is
  `FINAL`, or an interface (`XB_CLASS_INTERFACE`: a def whose methods, without functions, are the messages it
  requires, never tabled, and whose parents are the interfaces it inherits); a `FINAL` method (`XB_MEMBER_FINAL`) a
  class before it in the line declares again (a method, a variable or a class variable answering its message); and,
  for a class that is neither `ABSTRACT` nor `SCALAR`, a message its line answers with an `ABSTRACT` method
  (`XB_MEMBER_ABSTRACT`) or one an interface of a class of its line requires (with those that interface inherits, each
  once: interfaces may inherit from one another) that its line answers with nothing or an `ABSTRACT` method (`New`,
  `Init` and `ClassName`, which the runtime answers by itself, count as answered: `xb_member_by_runtime`); an
  interface not registered under the name it `IMPLEMENTS`, or one inheriting from a class, is the error too. A class
  whose line holds a class that breaks one breaks it as well (a `FINAL` method declared again by a parent after the
  `FINAL` one's class, which the class's own line orders after it). The compiler applies the same check to the classes
  whose line and interfaces its module holds whole, as errors (`ClassModel::check`); the rest the runtime finds. An
  `ABSTRACT` method's function raises `ABSTRACT method called` (`__AbstractMethod`, `BASE/0`, `CLASS:MESSAGE` the
  operation): reached through `Super:` only, since an object of a class answering with one is never made.
  `ObjImplements( o, cInterface )` tells whether a class of the object's line implements the interface, or one
  inheriting it. The runtime declares `IEnumerable` (`GetEnumerator`), `IEnumerator` (`MoveNext`, `Current`, `Reset`)
  and `IDisposable` (`Dispose`): what `FOR EACH` and `USING` send, for classes to name with `IMPLEMENTS`.
- The messages it answers are tabled: for each class of the line in order, its methods, then its variables (`NAME`
  reads, `_NAME` assigns), then its class variables; the first class that has a message takes it. These rules (the line,
  the table's order, what a message from a method or `Super:` reaches, the scope a sender needs) are one implementation,
  `xbclass.h` (`types/members.c`), which the compiler applies too when it resolves a send to an object whose class it
  knows ([xir.md](xir.md#classes)). The INIT values are evaluated then, once per class (`CLS:(INIT)`); each new object
  gets them, an array or a hash copied for each object (`xb_clone_deep`): once they are known the class keeps a new
  object's variables in the object's order (`model`, a root of the collector, and the slots to copy, `deep`), which
  `xb_class_instance` copies whole into the object it allocates. Once the class is ready, when a new object takes
  nothing but memory (no destructor in its line, not `ABSTRACT`, no rule broken, and every value to copy for each object
  an empty array), the class publishes how one is made (`xb_class_make`: its `xb_class`, the model, the number of
  variables and the slots that each take a new empty array while the model's is empty still, since an INIT value the
  program holds may be filled later; a release store into `xb_class_def.make`), and compiled code makes its objects
  itself ([compiler.md](compiler.md#xir-to-llvm)). When evaluating them fails, the class is not usable yet and they are
  evaluated again the next time it is used; INIT values that need an object of their own class, or of a subclass being
  prepared, are `BASE/0` `Class used by its own initial values`.
- Threads prepare classes one at a time, under one lock for every class (`xb_rlock`, taken again by the thread that
  holds it as a preparation nests into parents and INIT values); a class is published ready (an atomic store of its
  state after everything else) and from then on used without the lock. The part a preparation makes is never freed: a
  class whose line failed keeps it for the next try. INIT values (and the error blocks and destructors that run
  meanwhile) run under that lock: other threads that need an unprepared class wait for the whole evaluation, and code
  run there never waits for another thread that may be preparing a class.
- `NAME()` returns a new object with its initial values; `:New( ... )` is the class's method, else the default one,
  which sends `Init` with the arguments as the class's own method would (a PROTECTED or HIDDEN `Init` is reached) and
  returns the object; `Init` without a method returns the object. A method receives the object as its first argument
  (`Self`); its `PARAMETERS` name the arguments after it.
- A message from a method of class F (or a codeblock it made: `xb_op_send_from`) reaches F's own HIDDEN member when F
  has one; else a method of the object's class (an overriding method wins, and so do a subclass's ACCESS and ASSIGN over
  a variable F declared); else the variable F sees (F's, or its ancestors', unless HIDDEN there); else what the object's
  class answers. `Super:message` takes the member of the ancestors after F in F's line; `Super:ClassName()` is F's first
  parent's name. When `Self` was assigned an object that is not of F's class, `Super:message` is `Message not found`. A
  value that is no object (a scalar class's method) reaches the member of F's line whatever class its type has now:
  deliberately, where `ClassAssociate()` changed it while the method runs (the reference's send then finds no member),
  so that a `Super` send the compiler resolves from F alone gives the same (`resolve-sends`).
- Class variables (`CLASSVAR`): each class of the line holds its own value of each, starting from the INIT value of
  the class that declares it (the value itself: an INIT array is the same array), except a `SHARED` one, which the
  class and its subclasses share; the values are roots of the collector. A cast reads the values of the class it sees.
- Friends: the methods of a class named by `FRIEND CLASS` (not its subclasses'), and a function named by `FRIEND
  FUNCTION` with the codeblocks it makes and the macros it evaluates (found from the innermost frame of the call stack
  that is not a macro's, a codeblock's name without its `(b)` prefix, `XB_BLOCK_PREFIX`), reach the HIDDEN and PROTECTED
  members of the class and assign its READONLY variables.
- Scopes: HIDDEN members answer F's own methods, PROTECTED ones the methods of their class and its subclasses, and a
  READONLY variable (or class variable) is assigned as if it were one scope narrower: an EXPORTED one as PROTECTED, a
  PROTECTED or HIDDEN one as HIDDEN (friends assign them all; the reference lets nothing assign a HIDDEN READONLY one,
  not even its own class's methods, so that only its INIT value is ever set); anything else is `BASE/41`
  `Scope violation (hidden)` or `BASE/42` `Scope violation (protected)` (generic code 13, the operation
  `CLASS:MESSAGE`, no `args`). A message the class does not answer is `BASE/1004` (`BASE/1005` to assign)
  `Message not found`, the operation `CLASS:MESSAGE`, the object in `args`, as in the reference; the runtime's own
  classes keep Clipper's `No exported method`.
- Casts: a message naming a class of the object's line that no member answers (`o:TParent`; `SUPER`, outside the
  `Super:` of a method, the first parent of the class the object is seen as) gives the object seen as that class,
  without allocating: the same item with the class's position in the line in its `width` field. A message to a cast
  reaches what that class answers (its methods, its variables in their areas), `ClassName()` is that class's name, a
  method receives the object itself as `Self`, and a cast kept in a variable stays one. A cast is the object: `==` gives
  `.T.` (the reference makes a new object, so `.F.`).
- A method's frame is marked `XB_PROC_METHOD`: `PCount()` and `PValue()` count the arguments after `Self`, in an
  `INLINE` method as in any other (deliberately: the reference counts `Self` in an `INLINE` method, whose body it runs
  as a codeblock given `Self` first, and not in the others).
- Reflection (`rtl/objects.c`, names in any letter case): `ObjIsKindOf( o, cClass )` (the class or one it inherits
  from), `ObjHasVar( o, cName )` and `ObjHasMethod( o, cName )` (any scope; ACCESS and ASSIGN are methods),
  `ObjVars( o )` and `ObjMethods( o )` (the EXPORTED ones, upper-cased, in table order; anything but an object is
  `BASE/0`), `ObjSend( x, cMessage, ... )` (`x:cMessage( ... )` from outside the class; `_NAME` assigns). The
  runtime's own classes answer with their variables and no methods.
- Operators (`OPERATOR "op"`, `METHOD name OPERATOR "op"`): a method whose message is the operator (`XB_OPERATOR_*` in
  `xbrt.h`: `+ - * / % ^ = == != < <= > >= $ ! ++ -- []`; the compiler maps `**`, `<>`, `#` and `.NOT.` to them), and
  xbase's own: unary minus (`"-" UNARY`, the message `u-`), the bit operators `& | ^^ << >> ~` (the bit table's symbols,
  `xb_bit_function`) and `.AND.` and `.OR.`. The runtime's operator functions ask the class of the left (or only)
  operand when it is an object, before anything else (`xb_class_operator`; `==` too, even for the same object; for a
  cast, the class it shows): the method receives the right operand; `[]` receives the index, and the value when it
  assigns (`o[i] += x` reads, then assigns). `.AND.` and `.OR.` first test their left side (`xb_class_has_operator`,
  the xIR's `has.operator`, a macro's `MO_OPERATOR_JUMP`): an object whose class overloads the operator receives the
  right side's value, evaluated then, once; any other left side decides as before (a logical alone, the right side
  evaluated only when it does not settle the result; anything else the condition's error 1066). In a condition the
  operator's result is tested as a condition. A left side that is a condition by its form (a comparison, `!` or
  `.NOT.`, a logical literal, another `.AND.` or `.OR.`) is tested as a condition without that test
  (`Generator::logicalForm`, the macro's `logical_form`): its own operator runs (a class's `.NOT.` too, in a condition
  as in a value: `xb_cond_not`) and its result must be logical; the conditions of loops keep their booleans. A
  comparison an overload makes into an object (a query being built) is kept in a variable, or sent its operator by
  name, to be combined with `.AND.`. Compiled fast paths never see objects, so they are unchanged. As in the
  reference, the right operand's class is not asked.
- Built-in types as classes: a `SCALAR CLASS type` (`XB_CLASS_SCALAR`, named as `ClassName()` names the type: `ARRAY`,
  `CHARACTER`, `HASH`, `NUMERIC`, `DATE`, `BLOCK`, ...; it has no class function and no instance variables, nor
  inherited ones: the compiler rejects those of a class of the module, and a class of another module's makes its first
  message `BASE/3005`, `xb_class_scalar_vars_error`, as `ClassAssociate()` of such a class) is the class of the type's
  values when no class is associated with the type: a message to such a value reaches its methods, the value as `Self`,
  and one the class does not answer keeps Clipper's error (`No exported method`). A program's `SCALAR CLASS` of a type
  answers first and the runtime's answers what it does not (the compiler marks the runtime's classes `XB_CLASS_RUNTIME`,
  so the order the system runs the modules' registrations in does not matter). The runtime's (`rtl/collections.prg`)
  give the type's functions as messages (`a:Len()`, `a:Add( x )`, `a:Sort()`, `"x":Upper()`, `s:Split( "," )`,
  `h:Keys()`, `n:Round( 2 )`, `d:Year()`, `b:Eval( ... )`); `ARRAY`, `CHARACTER` and `HASH` inherit `Enumerable`, whose
  methods work on any collection through `FOR EACH` over `Self`: `Select( bMap )`, `Where( bTest )`, `Take( n )`, `Skip(
  n )`, `TakeWhile( bTest )`, `SkipWhile( bTest )`, `SelectMany( bMap )`, `Concat( xOther )`, `Zip( xOther, [bCombine]
  )`, `Distinct( [bKey] )` (equal by `==`; numbers, strings and dates found through a hash) and `Reverse()` are
  generators (the sequence is made as it is gone through, so they chain); `OrderBy( [bKey] )` and `OrderByDescending(
  [bKey] )` give an ordered sequence (`OrderedEnumerable`, which implements `IOrderedEnumerable`), ordered as `ASort`
  orders values (the primitive `__ValueLess`) each time it is gone through, as its source is then, equal keys in their
  order: `ThenBy( bKey )` and `ThenByDescending( bKey )` order the elements its keys found equal by one more key (a new
  sequence), and `[]` reads the element at a position; `GroupBy( bKey )` gives a hash of each key to the array of its
  elements, `ToHash( bKey, [bValue] )` a hash; and `ToArray()`, `Count( [bTest] )`, `Any( [bTest] )`, `All( bTest )`,
  `First( [bTest] )`, `Last( [bTest] )`, `Contains( x )`, `Aggregate( bFunc, [xSeed] )`, `Sum( [bValue] )`, `Min(
  [bValue] )`, `Max( [bValue] )`, `Average( [bValue] )` (the last three NIL without elements) and `ForEach( bBlock )` go
  through it at once. A class that inherits `Enumerable` only needs `GetEnumerator()`; a generator's enumerator answers
  `Enumerable`'s messages too (`xb_class_send_as`). `ClassName()` and `ValType()` of the values stay Clipper's.
- Scalar classes: `ASSOCIATE CLASS name WITH TYPE type` (`std.ch`, `ClassAssociate( name(), "type" )`; a type as
  `ClassName()` names it: `ARRAY`, `BLOCK`, `CHARACTER`, `DATE`, `HASH`, `LOGICAL`, `NIL`, `NUMERIC`) makes values of
  the type answer the class's methods and class variables, the value as `Self`; `ClassName()` is the class's, and its
  errors name it. A class with instance variables is `BASE/3005`. Messages it does not answer are the class's
  `Message not found`; values of other types behave as before.
- Destructors (`DESTRUCTOR name`, `xb_class_def.destructor`): an object of a class whose line has one is registered with
  the collector (`xb_gc_finalizable`) when it is made. A collection that finds it unreachable keeps it, and what it
  refers to, alive in a queue and raises `XB_PENDING_FINALIZE` on one context, the first registered (the program's; when
  it goes, the next one, which gets the request if objects wait), so destructors never run on two threads at once; that
  context's next pending test that may collect (`xb_pending_gc`, also right inside `__GcCollect()`) runs the destructor
  of each class of its line that has one, its own class first, with the object as `Self` (`xb_classes_finalize`);
  destructors may allocate and collect. The requests already pending (a `BREAK` on its way, with its value) wait while
  they run; an error is the error block's (also when the destructor runs inside a `Type()`, whose evaluation its errors
  do not end), and a `BREAK` or an unrecovered error ends the destructor alone, while a `QUIT` stays and leaves the
  other queued objects for the end. When the start function returns, a full collection destroys what it alone kept, and
  what is still queued, before the `EXIT` procedures. A destructor runs once: one that stores its object keeps it alive,
  without the reference's error. Objects die when a collection finds them unreachable (a frame temporary may keep one
  until its function returns), not, as with the reference's reference counts, the moment the last reference goes.
- Inline caches: each send of compiled code has its own `xb_send_cache`, of `XB_SEND_CACHE_WAYS` (8) ways (four held too
  few classes for a send that sees six of them, `virtual_dispatch`: every message missed). A message that reaches a
  member of an object (not through a cast) stores the object's class, the member and its slot there (a variable's index,
  a class variable's address) as the newest way (the oldest of a full cache goes); the next message from that send to an
  object of one of those classes runs the member at once, without the lookup, the scope checks (which give the same
  answer for the same class at the same send) or a call to `dispatch` (`perform`). A runtime class's member
  (`xb_runtime_member`: a variable or a method, found as `xb_op_send` finds it) is kept too, its index and how it is
  reached in the slot and no member (`XB_SEND_OTHER` for compiled code), and run by `xb_runtime_perform`: a `Channel`'s
  or a `Task`'s send repeated finds its method with no search by name. What the runtime answers itself for a class
  without such a member is kept as a member of its own (`XB_MK_NEW`, `XB_MK_CLASS_NAME` in `xbclass.h`, made when the
  class is tabled): `New` (the class's `Init` with the arguments, then the object; the object alone without an `Init`),
  `Init` of none and `ClassName`; not `New` whose `Init` is not a method the class's scope reaches (an error each time),
  nor through `Super`. Messages to casts and to values of scalar classes always look up. Each way also tells compiled
  code what the member is (`kind`: `XB_SEND_GET`, `XB_SEND_SET`, `XB_SEND_METHOD` with the method's function in `fn`,
  `XB_SEND_DIRECT` or `XB_SEND_DIRECT_WORDS` with its direct entry there when that takes as many parameters as the
  message's arguments, Self first, or `XB_SEND_OTHER`), so that it performs a way in place
  ([compiler.md](compiler.md#xir-to-llvm)); a send compiled code performs in place passes as many arguments each time,
  and one whose cache's `flags` hold `XB_SEND_COMPACT` gets a method's function, never its direct entry. Threads share
  the caches: each one is a sequence lock (`version`, odd while a thread changes it). A reader that finds the version
  odd, or changed once it has read a way, does the full lookup instead (nothing torn is used, nothing is tried again),
  and a writer that finds another one at work leaves the cache as it is; 20,000,000 messages to objects of two classes
  through one send take the same time as without it. Where the compiler knows the object's class, it resolves the send
  by these rules instead: a call of the method, or the variable at its slot ([xir.md](xir.md#classes)).

## References

An item that designates another item is an `XB_T_CELL` (a variable in a cell: `v.p` is the `xb_cell`) or an `XB_T_REF`
(an element: `v.p` is an `xb_ref` holding the collection, the 1-based index, for a string the current character, and
for a hash element taken by `@h[k]` or from a loop the entry's key, see [Hashes](#hashes)).
The two tags are consecutive, so compiled code tests for both with one unsigned comparison. A cell may hold an
`XB_T_REF` (a `FOR EACH` variable); an element never holds either. `xb_ref`, and `xb_hash` with its entries, are part of
the ABI (`xbrt.h`): compiled code reads an element of an array, or the entry at a position of a hash without holes, in
place.

- A `FOR EACH` variable holds an `XB_T_REF` during the loop (see [Operators](#operators)).
- An argument passed by reference: `@x` passes the variable's cell (`xb_ref_of`), or for a `LOCAL` or parameter that
  holds a loop's reference a copy of it (the current element; a `STATIC` is always passed itself); `@a[i]` passes a
  new `XB_T_REF` from `xb_ref_elem`, which raises the errors of reading `a[i]` (1068, 1132). So the callee, and a
  codeblock that captures its parameter, share the caller's variable. A reference records whether a loop made it,
  so a parameter given an element can be told from a loop variable.
- Compiled code reads and writes a local cell's value in place, and reads a `FOR EACH` variable's element of a local
  collection, or its loop reference's own item, in place; `xb_ref_load` and `xb_ref_store` take the rest, given an
  item's type (`XB_T_CELL`, `XB_T_REF`) and pointer: through a cell, then an element, the whole item (`xb_ref_get`,
  `xb_ref_set`, under the counter or lock of a shared object; `xb_ref_store` makes the write barrier). An element is
  found again at every use, so it follows the array when it grows, and an index outside the array, after the array
  shrank, raises `Bound error: array access` (1132). `xb_ref_load` copies a local reference's element of a local
  array, or its own item, at once.

## Memory variables

`PRIVATE` and `PUBLIC` variables (`core/memvar.c`, the `xb_mv_*` entry points in `xbrt.h`) are found by name when the
code runs, so a function sees and changes the memory variables of the functions that called it, whatever module they
are in.

- Names. A name has a number. The names a module uses (`xb_memvar`) get theirs when it registers and keep them for the
  program's life (permanent). A name met only while the program runs (a macro, `RESTORE`, `MemVarBlock()`, `PRIVATE
  &cName`) has one while something holds it: a binding of an environment, a PRIVATE alive, the PUBLIC of that name, a
  function of the runtime using it a moment (`xb_memvar_hold`). When the last holder lets it go, the name and its number
  are freed, and the number goes to another name later: a long-running program that makes names without end (a server
  evaluating `Type()` of user input, `PRIVATE &cName` with names made from data) keeps only the names in use, and runs
  as fast after a million of them as after ten (`__MemNamesInUse( [lLetGo] )` counts those numbered and not freed, after
  the calling thread lets go of its cache of names with `.T.`). Compiled code, and the macro compiler's code, keep only
  permanent names' numbers; a macro keeps the text of any other name, and so do a `MemVarBlock()` block and an
  environment's snapshot. An operation by a name's text (a macro's `MO_MV_*` with `XB_MACRO_NAMED`, `PRIVATE &c`, a
  `MemVarBlock()` block, `RESTORE`) takes the name's number from its thread's cache of names (`xb_memvar_use`:
  `XB_NAME_CACHE` entries chosen by a hash of the name, each holding the name it last served), so a name used again and
  again costs no lock and no atomic operation, and at most `XB_NAME_CACHE` names per thread stay held for it; a read of
  a name no variable has looks it up without numbering it (error 1003), and a macro's undeclared name tries the work
  area's field of that name first. The table of names is the process's: a name is found under a reader-writer lock,
  numbered or freed under its write lock; a held number's spelling is read without a lock, from chunks of entries that
  never move.
- Bindings. Each environment keeps, by number, the PRIVATE bindings visible now (`xb_env.bindings`): a small hash of
  the numbers bound (open addressing, linear probing, at most half full, backward-shift deletion), which an
  environment that inherits it shares until one of them changes it (copy on write), so an asynchronous call copies
  nothing, and what an environment costs follows what it binds, never how many names the program has. The PRIVATEs
  alive, in creation order, each with the frame of the activation that created it and the binding it hid, are a stack
  of the environment (`xb_env.privates`), which halves as they end while it is past 1024 entries and three quarters
  empty (`tests/platform/shrink`).
- PUBLICs. They are the program's, in one table every thread reads by number without a lock: chunks of items that
  never move, made when a PUBLIC first needs one, an item NIL or the variable's cell, set under the names' write lock
  (the cell made, and shared once the program's storage is, before the lock is taken; the cell stored before the
  type, which a reader loads first) and released by its type made NIL. A name with no binding in the environment is
  the PUBLIC's, if there is one.

All three are roots of the collector. A new flow's environment (a thread's, a task's) starts with the bindings its
creator saw when it was made (`xb_memvar_capture` into the snapshot of the environment, the names that are not
permanent by their text; `xb_memvar_restore` from it), and no PRIVATE record: the same variables, which neither
releases when its activations end.

- A binding is a cell (an `XB_T_CELL` item): `@name` passes it, and a codeblock reaches the variable through the name
  when it runs, not through a capture.
- `PRIVATE name` creates a variable (NIL) that hides any other of its name until the activation that created it (a
  function call or a codeblock call) ends, by `RETURN` or by unwinding; then the hidden one shows again. An activation
  that already has a PRIVATE of that name keeps it (and its value). The compiled code of an activation that may
  create PRIVATEs takes a mark when it starts (`xb_mv_mark`) and releases to it when it ends (`xb_mv_release`).
- `PUBLIC name` creates a variable (`.F.`; `.T.` for `PUBLIC Clipper`, Clipper's rule) that lives until the program
  ends, unless one of that name is visible already (a PRIVATE or a PUBLIC): then it does nothing, and its initializer
  assigns the visible one.
- Assigning a name with no variable visible creates a PRIVATE of the running activation, as in Clipper.
- A name the compiler leaves undeclared is read with `xb_mv_field`: a field of the current work area of that name
  first, else the memory variable ([Work areas](#work-areas)); `MEMVAR` and `M->` read the memory variable alone.
- `PARAMETERS` creates a PRIVATE per name holding the argument at its position (NIL past the arguments received); an
  argument passed by reference makes the PRIVATE the caller's variable itself (its cell, or a cell holding the
  element's reference).
- Reading (or passing `@name`) a name with no variable visible is error `BASE/1003` `Variable does not exist` (the
  name as the operation, no `args`), which the error block may retry: returning `.T.` looks the name up again (a
  block that created the variable fixes it), anything else ends the program (`Error recovery failure`), as in the
  reference.
- `RELEASE name, ...` (`__MemRelease()`): the PRIVATE visible by that name holds NIL where it is (what was passed
  `@name` sees NIL, and so does a memory variable passed by reference to `PARAMETERS`); a `PARAMETERS` one that received
  a LOCAL, a STATIC or an element by reference lets it go for a NIL of its own; one a thread's creator had visible
  holds NIL too. A PUBLIC no longer exists, for every thread. A name with no variable is skipped.
- `RELEASE ALL [LIKE | EXCEPT skeleton]` (`__MemReleaseAll()`): the same for the PRIVATEs that the function or
  codeblock running it created, those whose names match the skeleton (`LIKE`) or do not (`EXCEPT`); PUBLICs and the
  callers' PRIVATEs stay. In a skeleton `*` stands for any characters and `?` for one, letters match in any case. The
  reference takes a skeleton that starts with `*` for every name (`*B`); xbase matches the whole skeleton, so it never
  releases more than the skeleton names.
- `CLEAR MEMORY` (`__MemClear()`): no PRIVATE of the environment, and no PUBLIC of the program, exists any more; the
  PRIVATEs' records stay until their activations end, restoring nothing.
- `SAVE TO file [ALL [LIKE | EXCEPT skeleton]]` (also `SAVE ALL LIKE | EXCEPT skeleton TO file`; `__MemSave()`,
  `rtl/memfile.c`) writes the variables visible, PRIVATE or PUBLIC, whose names match, to a `.mem` file (the extension
  is added when the name has none): Clipper's format, which `memfile.c` describes, holds strings (65534 bytes of each at
  most), numbers (with their width and decimals), dates and logicals, and the first 10 characters of each name; other
  values are left out. The records follow the names' numbers (the reference orders them otherwise; each record is the
  same): the environment's bindings and the PUBLICs it does not hide, held while they are written. No file name (`SAVE`
  alone) is error 2008 (`EG_ARG`). A file that cannot be created or written is error 2006 (`EG_CREATE`, or `EG_WRITE`),
  which the block may retry; the default gives up, leaving a file already created or partly written as it is.
- `RESTORE FROM file [ADDITIVE]` (`__MemRestore()`) gives each variable of the file its value: the one visible by
  that name, else a new PRIVATE of the function (or codeblock) running `RESTORE`, which ends it when it returns (its
  declaration's `PRIVATES` attribute gives that function a mark, see [compiler.md](compiler.md#calls); `Do()` takes
  one around the function it calls).
  Numbers without decimals come back as integers. Without `ADDITIVE`, `CLEAR MEMORY` comes first, once the file is
  open: a file that cannot be opened (error 2005, `EG_OPEN`, which may be retried; the default restores nothing)
  leaves the variables as they were, where the reference releases them first. No file name is error 2007
  (`EG_ARG`). Reading stops at the end mark (0x1A), at the end of the file, or at a record it cannot read.

## Macro operator

`&text` compiles an expression from text when the code runs (`runtime/src/macro/`: `lex.c`, `compile.c`, `run.c`;
the `xb_macro_*` entry points in `xbrt.h`, see [compiler.md](compiler.md#grammar) for the forms).

- The grammar is the compiler's expression grammar, with its precedence, evaluation order and literal rules (the width
  of a numeric literal is `xb_num_literal_width`, shared), and operators on number literals fold as the compiler folds
  them, with the rules of `xbrt.h` (`xb_fold_*`): numbers through the operators, the bit operators and functions,
  `Len()`, `Asc()` and `At()` (the runtime's: not a function of the program of that name), strings through `+`, `-`,
  `Chr()` and `Upper()` (`xb_upper_folds`), logicals through `.NOT.`, `.AND.`, `.OR.`, comparisons (`=`, `==`, `!=` with
  `NIL` too) and `Empty()`, an
  `IIF()` on a folded condition and a list whose leading values are constants: the calls the language evaluates when
  compiling ([compiler.md](compiler.md#constant-folding); only folded numbers are emitted, folded strings and logicals
  matter to what is folded from them: `&( 'Len( Chr( 65 ) + "b" ) / 2' )` is the integer 1; `&( "999999999
  / 3" )` is the integer 333333333, `&( "1000000000 * 1" )` keeps a constant's width; a quotient that is not exact is a
  constant with SET DECIMALS decimals when it runs, `MU_DEC_AT_RUN`). A macro sees memory variables, the program's
  public functions and the runtime's functions, by name; never a function's `LOCAL`s or `STATIC`s, nor a `STATIC
  FUNCTION`. `M->name` and
  `MEMVAR->name` are memory variables; a name alone is a field of the current work area before a memory variable
  (`MO_MV_FIELD`), and the aliases of [rdd.md](rdd.md#field-references) name fields (`alias->name`, `FIELD->name`,
  `FIELD->alias->name`, `( expr )->name`, `&( expr )->name`) or evaluate in a work area (`alias->( expr, ... )`, `( expr
  )->( ... )`, with `MO_AREA`, which makes the area current again when the evaluation ends, also by an error); `M->(
  expr )` names the alias `M`, as in compiled code. A codeblock made from text sees its own parameters and those of the
  codeblocks around it in the text, at any depth, shared through cells as a compiled codeblock captures them
  (`MO_CAPTURE_*`; the parameter's slot moves into a cell when the inner codeblock is made); Clipper's and the
  reference's macros see only their own, and the name is then a memory variable. Inside the text, `&name` is a macro of
  its own (the variable's text); a name joined to other identifiers there is one the substitution below could not make,
  an error. A text nested deeper than 1024 levels (parentheses, a chain of operators, `a[1][2]...`), the compiler's
  limit, is error 1449, so that compiling it keeps the stack small.
- `compile.c` parses the text into a tree, then generates code for a stack machine: a function is an array, its
  code (a string of bytes) and its constants. `run.c` runs it with the runtime's operators (`xb_op_*`, as compiled code
  calls them); nothing is folded, so `&( "7 % 3" )` is `1.00`, as `n % m` is (the reference folds some constants). The
  machine's items live in a frame linked into the context's chain (the collector sees them): up to 32 items in the
  evaluator's own stack frame (an allocation per evaluation would cost about a quarter of a macro block's `Eval()`), a
  larger one allocated. The frame of an
  expression's evaluation is not an activation (`XB_PROC_TRANSPARENT`): a PRIVATE the macro creates (`&c := 1` with no
  variable of that name) belongs to the function running it, which takes a mark for it (see
  [compiler.md](compiler.md#xir-to-llvm)), and `RELEASE ALL` there reaches it. A codeblock from text is an activation
  of its own, which ends its PRIVATEs.
- What a text compiles to depends on the text alone (the symbol table is complete before the program runs, names keep
  their numbers), so each thread context keeps the last texts compiled and their functions (`xb_ctx.macro_cache`, 64
  entries in sets of two, the set chosen by the text's hash, which the string keeps once computed, `xb_str_hash`; a
  text found in its set makes the other entry the one the next text takes; roots of the collector): a macro in a loop
  is compiled once. The evaluator computes an integer sum, difference or product that does not overflow, and a
  comparison of two integers, in place, by the rules of constant folding at run time (`xb_fold_numbers`,
  `xb_fold_compare`); any other case calls the operator.
- Before it is compiled, the text goes through Clipper's text substitution, as a string literal with `&name` does
  (`xb_macro_text`): each `&name` or `&name.` whose name is a memory variable holding a string becomes that string, and
  the scan goes on at the start of what was inserted, so a name may run on past its end (`c := "&d"` then `&c`
  evaluates `d`'s text); anything else stays as written. After 4096 insertions in a row with nothing written out, or
  once the text would grow past about 16 MB, the rest is copied as written: a text that keeps inserting itself
  (`c := "&c"`) ends there, where the reference never ends.
- Errors: a value that is not a string is `BASE/1065` (`&`, the value in `args`), a text that is not an expression
  `BASE/1449` `Syntax error` (`&`, the text in `args`; also a comma list, as in Clipper, where the reference's own mode
  spreads it into arguments), both substitutable; a variable that does not exist is `BASE/1003` (retried, as compiled
  code's), a function the program does not have `BASE/1001` (substitutable).
- `&c( args )` calls the function `c` names (blanks around it dropped; anything but a name is `1449`; references as
  `Do()` passes them). `PRIVATE &c` and `PUBLIC &c` create the variable it names; `&c := v` assigns what it designates
  (a variable, an element, an instance variable, or another macro; anything else is `1449`).
- `Type( cExpr )` substitutes and compiles the text without running the error block: `"UE"` when it is not an
  expression, `"B"` for a codeblock (not evaluated), `"U"` when it calls a function the program does not have, `"UI"`
  when it calls one that is not one of Clipper's predefined ones (`RESERVED` in `rtl.ch`, carried into the symbol
  table; nothing is evaluated), `"U"` when it names a variable or field that is not visible (inside
  `alias->( expr )`, a field of the alias's work area; inside `( expr )->( ... )` nothing is checked); otherwise it
  evaluates the expression with the context's `error_trap` set: an error ends the evaluation (`XB_PENDING_TRAP`, which
  unwinds like a `BREAK` but no `BEGIN SEQUENCE` catches; `error_trapped` keeps its generic code) and gives `"U"` when
  it is `EG_NOVAR` or `EG_NOALIAS` (a variable, field or alias found missing only then), `"UE"` otherwise; else the
  letter `ValType()` would give. A PRIVATE the
  evaluation creates is the calling function's (`PRIVATES` in `rtl.ch`). Anything but a string is `BASE/1121`.
- `o:&text` (`xb_macro_send`) sends the message the text names (blanks around it dropped, any letter case; `_` and
  the name for an assignment), from the method it is written in as a compiled send is, so scopes apply alike; a
  value that is not a string is `1065`, a text that is not a name `1449`. The reference has no `Super:&text`.
- `PCount()` inside a macro expression counts the arguments of the function or codeblock running it; inside a
  codeblock the macro made, the codeblock's own (`PValue()` gives them as passed).
- `@&text` (`xb_macro_ref`) gives a reference to what the text designates, as an argument passed by reference: a
  memory variable (one that does not exist is `BASE/1003`, not created; a macro sees no `LOCAL`) or an element
  (`"a[1]"`, `"h['k']"`); any other text is `1449`, a message too (`"o:x"`, which the reference passes as a reference
  to the instance variable, though no compiled code can). The reference has no `@&( expr )`.

## Calling convention

Every xBase-callable function, compiled or written in C, has the dynamic convention:

```c
void xbf_NAME(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
```

- `NAME` is the upper-cased xBase name. Runtime functions are `xbf_NAME` (`XB_RUNTIME_FUNC_PREFIX`) and the program's
  are `xbp_NAME` (`XB_PROGRAM_FUNC_PREFIX`), so a program may define a function named like a runtime one that is not
  `RESERVED` without a clash at link time. The other functions of a runtime module written in xBase (its methods,
  generators' resume functions, `STATIC` functions) are `xbl_MODULE$NAME` (`XB_LIBRARY_FUNC_PREFIX`, `:` and `.` of the
  name as `$`): a program reaches them once the compiler ran a body of the runtime's in place ([xir.md](xir.md),
  `resolve-scalar-sends`), and `xb_scalar_replaced` (a byte for each type, set once a class of the program answers the
  type's messages before the runtime's own class: a `SCALAR CLASS` of the program, `ClassAssociate()`) tells compiled
  code when it may call a method of that class directly. Each such module also defines `xbl_MODULE$source$HASH` (HASH
  the FNV-1a 64 of its source text, 16 hexadecimal digits), which a program given the carried sources references
  (`xb.source.stamps`): a runtime built from other texts than the compiler carries fails to link instead of running
  bodies that disagree with the program's inlined copies. The public functions of an xbase library are
  `xbx_LIBRARY$NAME` (`XB_XBLIB_FUNC_PREFIX`), and its stamp `xbx_LIBRARY$source$HASH` ([projects.md](projects.md)).
- Every runtime function is declared in `include/rtl.ch` (`EXTERN FUNCTION`, see [compiler.md](compiler.md#calls));
  the `runtime.declarations` test checks that the declarations and the `xbf_` definitions match.
- This is the convention of every function's symbol: the runtime, `Do()`, codeblocks and other modules call through
  it. Within a module, compiled code calls a function's direct entry, passing the parameters' items in registers
  ([compiler.md](compiler.md#xir-to-llvm)); a send calls a method's, of any module, through its send cache
  ([Objects and messages](#objects-and-messages)).
- The caller evaluates the arguments into consecutive slots of its own frame and sets `ret` to `NIL`; a callee that
  returns nothing leaves it untouched. Compiled code always writes `ret` when it returns (`NIL` for no value), so a call
  to a direct entry within the module does not clear it first (when the callee unwinds, the result is not read); a
  direct entry whose results never refer to a heap object (and are not all `NIL`) returns its result's two words in
  registers instead. The
  callee copies its parameters into its frame; missing arguments are `NIL`. An argument passed by reference is an
  `XB_T_CELL` or `XB_T_REF` item (see [References](#references)); a C function receives one only where its declaration
  says `@`.
- A list with `...` (a call, a send or an array literal) has a length known at run time: compiled code reserves a frame
  of that size on the stack (`llvm.stacksave`/`stackrestore` around it), linked into the chain while the call runs, and
  fills it with the other items and, after them, the function's own arguments past its parameters
  (`xb_args_rest`); a reference among those goes on as it is where the callee takes references (a compiled function, a
  runtime function's `@` parameters: `xb_refs_take`) and as its value elsewhere (a method, an array element; its target
  gone: an error). `Do` passes its arguments on by the same rule.
- Every 32-bit parameter of a runtime entry point is `uint32_t` and every 32-bit result `int32_t`; the compiler
  relies on this to apply the target's argument-extension rules.

## Errors and pending requests

`xb_ctx` starts with the fields compiled code uses (`xbrt.h`): `uint32_t pending`, `int32_t set_decimals` (SET DECIMALS,
which inline `/` and `%` read), `xb_frame *frames`, `const char *stack_limit` ([Frames](#frames)) and `xb_item *lent`
(the arguments a compiled caller lends for the call it makes, `NULL` after it: a compiled function with a direct entry
whose `argv` they are reads the items it has parameters for, Self included, sets them to `NIL` and the field to `NULL`,
so that it alone holds what only they held; one without a direct entry ends the loan in its prologue; a send that
calls a method's direct entry does the same itself before the call). Runtime code
passes the `argv` it received on whole only where it reads none of it after the call (a send's dispatch, `Dispose`, an
enumerable message of a generator); code that would read it sets the field to `NULL` first; and `xb_tlab *tlab`, the
thread's hole, where compiled code allocates ([Garbage collector](#garbage-collector)). `pending` holds requests. The
runtime sets bits there (`XB_PENDING_BREAK`, `XB_PENDING_QUIT`, `XB_PENDING_SAFEPOINT`, `XB_PENDING_FINALIZE` when there
are objects to destroy, `XB_PENDING_TRAP` while `Type()` evaluates, see [Macro operator](#macro-operator), and
`XB_PENDING_FAIL` when a task fails: an error the default handler gave up on in a task, its value in the `BREAK` value's
place, which no `BEGIN SEQUENCE` takes and the task's end does, see [Threads and
synchronization](#threads-and-synchronization)). Compiled code tests the field after each fallible call (not where no
call ran since a test that found nothing to unwind for: [compiler.md](compiler.md#object-emission)); when it is set the
code calls `xb_pending_gc` (or `xb_pending`, which never collects nor parks the thread, where a substituted result waits
unseen: see [compiler.md](compiler.md#xir-to-llvm)), which serves a collection another thread makes (the thread parks: a
safepoint) and destructors ([Classes](#classes)) and returns non-zero when the function must unwind: to a `BEGIN
SEQUENCE` handler of the function, or to its caller ([xir.md](xir.md#control-flow-for-errors-and-exceptions)); a loop
tests the field the same way on its back edge, once every 1024 passes in an innermost loop without calls
([compiler.md](compiler.md#object-emission)), and the macro evaluator after each operation that can fail (with
`xb_pending_gc`: its values are all in its frame). Other threads set bits there, so every change is an atomic
read-modify-write (`xb_pending_set`, `xb_pending_clear` in `core/context.h`), and code that sets requests aside or ends
them touches only the flow's own (`XB_PENDING_UNWIND`). The check stays valid across `AWAIT`. `QUIT` raises
`XB_PENDING_QUIT` with exit code 0.

A runtime error (`core/error.c`, `xb_error` and `xb_arg_error` in `core/error.h`; `xb_error_of` for one with another
subsystem, a file name or a system code) makes an `Error` object (see [Objects and messages](#objects-and-messages)):
subsystem `BASE` (one string never collected, shared by every error; the work areas' errors are `DBCMD` and the driver's
name, [rdd.md](rdd.md#work-areas)), the subcode, the generic code (`error.ch`, shared with xBase code like `set.ch`) and
its description (in the current language, [Languages](#languages)), the operation, severity `ES_ERROR`, `canSubstitute`,
and in `args` the operands of an operator or the arguments of a function (NIL when there are none). Then it evaluates
the context's error block with it (`ErrorBlock( [bNew] )` returns the block and replaces it with a codeblock argument;
the block and the `BREAK` value are roots of the collector). With no error block (a program's `ErrorSys()` set none) the
error is unrecoverable: `Unrecoverable error 9002: No ERRORBLOCK() for error`. After the block:

- a request to unwind is pending (the block called `Break()`, or quit): the operation fails with it;
- the error may be retried (`XB_ERR_RETRY`, `xb_error_retry`: `canRetry`, with `tries` counting the attempts from 1;
  a missing memory variable, a file error) and the block returned `.T.`: the operation is tried again;
- the error has a default (`XB_ERR_DEFAULT`: `canDefault`; a file error, `xb_error_file`, which also sets `filename`
  and `osCode`, the `FError()` code of [Files](#files)) and the block returned anything else: the operation does its
  default, which gives up quietly;
- the error allows substitution (`XB_ERR_SUBSTITUTE`: operators, `SWITCH`, a non-numeric index, messages, `Eval` and
  the runtime functions, except `ASize`, `AEval`, `AFill`, `Set` and the array dimension): the block's return value
  becomes the operation's result, and the program goes on;
- otherwise (a bound error, an assignment to an element, a condition that is not logical, the collection of `FOR EACH`,
  ...) the program ends: `Unrecoverable error 9001: Error recovery failure` on standard error, exit code 1, a `QUIT`
  pending.

Which errors allow substitution, and their `args`, follow Clipper/Harbour. An error raised while the error block runs is
handled the same way, nested at most 8 deep (`XB_ERROR_MAX_DEPTH`); one more ends the program (`Unrecoverable error
9003: Too many recursive error handler calls`). In a task, none of these three ends the program: the task fails with the
error no handler could take (the one the block returned for, the one raised with no block, the innermost of the
recursion), its lines of the calls kept, as the default handler fails a task with an error nobody handled
(`xb_error_fail_task`, `core/error.c`); its waiter gets the error. In a `FOR` test, a substituted result stands for the
comparison `counter > limit` (`<` stepping back): a logical, else the error of a condition.

A context starts with no error block. The program's startup calls `ErrorSys()` by name after the `STATIC`
initializations and before the `INIT` procedures ([Program startup](#program-startup)): the runtime's, in xBase
(`rtl/errorsys.prg`), unless the program defines its own. It installs `{| oError | DefError( oError ) }`: a division by
zero gives 0 (as Clipper's default handler does); an opening refused for the file's sharing (`EG_OPEN` with system code
32) and an append that cannot lock its record (`EG_APPENDLOCK`), when they allow the default, set `NetErr()` and do
nothing, as in Clipper ([rdd.md](rdd.md#sharing-locks-and-concurrency)); any other error prints
`Error <subsystem>/<subcode>  <description>: <operation>` (the file name instead of the operation when there is one,
then ` (DOS Error <osCode>)` when there is a system error code; `Warning` for a severity below `ES_ERROR`), then a line
`Called from NAME(line)  ` for each level of the call stack (see [Frames](#frames); the handler's own frames are hidden,
so level 0 is where the error happened), to standard error (`OutErr()`), and ends the program with `ErrorLevel( 1 )` and
`QUIT`. In a task it fails the task instead (`__TaskFail`), the error keeping those lines; an error that comes from a
task (its lines kept) is reported with them first, then the handler's own calls, the first as
`Waited for in NAME(line)  ` (where it was awaited, or its `Result` taken). The choice between quitting, retrying and
the default (Clipper's `Alert()`) waits for the terminal layer. An unrecoverable error lists the levels as
`Called from NAME(line) in FILE` (without ` in FILE` for a runtime function). Both list the innermost `XB_CALLS_SHOWN`
(100) levels and then, when there are more, the line `(further calls not shown)` (`include/error.ch`): the reference
lists every level, which after a runaway recursion means millions of lines. The handler's lines come from
`__CallLines( oError )` (`xb_error_call_lines`, `core/error.c`, which a task failed by an unrecoverable error uses too),
one walk of the frames (asking `ProcName( n )` level by level would walk them again for each).

`Break( [xValue] )` (and the `BREAK [value]` command, `__Break`, and `Throw( [xValue] )`) stores the value in the
context and raises `XB_PENDING_BREAK`. In a `BEGIN SEQUENCE` handler, `xb_seq_catch` takes a `BREAK` that is the only
request pending, and `xb_seq_value` moves the value out (`RECOVER USING`). With no sequence active the `BREAK` reaches
the start function, and the program ends quietly with exit code 0. `BEGIN SEQUENCE WITH` (and `TRY`) calls
`xb_seq_with`, which makes its block the error block (`ErrorBlock()` gives it meanwhile) and returns the one it
replaces, and `xb_seq_restore` puts that back. On the way out of an unwinding through `ALWAYS`, `xb_seq_suspend` sets
the pending requests aside and `xb_seq_resume` raises them again with the `BREAK` value, unless the `ALWAYS` code raised
one of its own, which replaces them (a `QUIT` set aside stays, as in the reference). An error in the `WITH` block itself
goes to that block again, as in the reference, until the limit of nested handler calls (error 9003).

## Operators

`xb_op_*(ctx, result, a, b)`; the result may alias either operand.

| Operation | Behavior |
|---|---|
| `+` `-` on integers | 64-bit; on overflow a double with 0 decimals |
| `++` `--` (`xb_op_inc`, `xb_op_dec`; also the step of a `FOR` without `STEP`) | `+ 1`, `- 1` on a number or a date; anything else is their own argument error (1086, 1087) with the operand as its only argument, as in the reference; `+= 1` and a `FOR` with a `STEP` are `+` (1081, both operands) |
| `*` on integers | 64-bit, exact; on overflow a double with `SET DECIMALS` decimals |
| `+` `-` `*` on numbers | double; decimals: the larger of both for `+`/`-`, the sum for `*` |
| `/` | always a double with `SET DECIMALS` (2) decimals; zero divisor is an error |
| `%` | always a double with `SET DECIMALS` decimals (remainder of the integers, or `fmod`); zero divisor is an error |
| `^` | double with `SET DECIMALS` decimals |
| `+` on strings | concatenation; an append (`x += y`) from 512 bytes a view, grown at its buffer's tip ([Values](#values)) |
| `-` on strings | concatenation moving the left operand's trailing blanks to the end |
| `=` on strings | with `SET EXACT OFF`, compares only the right operand's length (`"abc" = "ab"` is true) |
| `==` | exact comparison; on strings byte-for-byte |
| `<` `<=` `>` `>=` `!=` | numbers, strings (same rule as `=`), logicals (`.F.` < `.T.`); NaN is ordered with nothing (IEEE 754, `xb_order_is` in `xbrt.h`): every comparison with it is false but `!=`, so a `FOR` loop ends at one, where the reference loops forever |
| `$` | substring test; an empty left operand is false; with a hash on the right, whether the left is one of its keys, or, for a hash of one entry on the left, whether the right has that key with the same value (of one type, a string byte for byte, an array, codeblock, hash or object the same one) |
| `.NOT.` | logicals only |
| `&` `\|` `^^` `<<` `>>` `~` | xbase's bit operators (`xb_op_bitand`, ..., `xb_op_bitnot`): numbers taken as integers (`xb_dbl_to_i64`: truncated, saturated, NaN 0), an integer result; `>>` is arithmetic; anything else is an argument error with the operator and the code of its operation (`xb_bit_function`) |

Comparing `NIL` with `=`/`==`/`!=` is allowed; other type mismatches raise `BASE` argument errors (whose error block
may substitute the result, see [Errors and pending requests](#errors-and-pending-requests)). `==` on two arrays,
two codeblocks, two objects or two hashes tests identity, and an array with an object is `.F.` (Clipper's objects are
arrays); other comparisons of them are argument errors.

Element access `a[i]` (`xb_op_index`) and assignment (`xb_op_index_set`) raise `Argument error: array access` (1068)
or `array assign` (1069) when the base is not an array or the index not a number, and `Bound error` (1132/1133)
outside `1 .. Len( a )`. An object is no array: unless its class defines `[]`, an index on it is that error (the
reference reads the object's variables by position).

`xb_num_default_dec(ctx, r, value)` makes a double with SET DECIMALS decimals; the compiler uses it for folded
expressions whose decimals are decided at run time. Items keep 16 bits of SET DECIMALS, as in Clipper.

`FOR EACH` (`core/control.c`) enumerates arrays (their items), hashes (their values, by position) and strings (their
characters as one-character strings); `xb_enum_len` raises `Argument error: array access` (1068) for any other
collection. Indexes are 1-based. The loop variable holds an `XB_T_REF` item (see [References](#references)); for a
string the reference holds a copy of the character, so assigning it leaves the string unchanged. `xb_each_key` gives the
key of `FOR EACH k => v`: the entry's key for a hash, the index for an array or a string; a hash that shrank below the
position raises the bound error. The loop works on a base item: the variable's cell when it is shared at entry
(`xb_each_shared`; a parameter holding an element passed by reference moves into a cell then), else the variable's own
item. `xb_each_enter` saves the base's value and puts the loop's reference there, `xb_each_move` points it at the next
element (taking the variable copies the reference, so the base keeps it; compiled code does it in place for an array or
a hash without holes, [compiler.md](compiler.md#xir-to-llvm)), and `xb_each_leave` restores the saved value; for a
`STATIC` the compiled code tests for a cell at each call, since a reference taken during the loop designates the
`STATIC` itself. `SWITCH` uses `xb_switch_check`, which raises `Argument error: SWITCH` (3104) for a value that is not
an integer or a string, and `xb_switch_match`.

`FOR` uses `xb_for_test`: a non-negative step loops while the counter is not greater than the limit, a negative step
while it is not less; operands that cannot be compared raise the `>` or `<` argument error, a non-numeric step the
`<` error.

Integer multiplication is exact over the whole 64-bit range; Harbour computes it in floating point and loses digits
above 2^53.

## Display

`xb_item_display` formats values the way `?`/`QOut()` print them: `NIL`, `.T.`/`.F.`, strings as they are, arrays,
codeblocks, objects and hashes as nothing, function references as `@NAME()`, and
numbers through `xb_num_to_str` (`runtime/src/core/numfmt.c`; `xb_num_text` writes the same text into a caller's
buffer, as `Transform()` builds its result, with nothing allocated):

- the field has the item's width (or `xb_num_int_width`/`xb_num_dbl_width`) for the sign and integer digits, then a
  dot and the item's decimals; with SET FIXED ON, SET DECIMALS decimals for every number but one of width 1, which
  keeps its own, as in Clipper (`xb_num_shown_field`, also for padding and `Transform()` without a template). In
  `Transform()` with no template, or one of functions only, an integer's field is instead two columns wider and two
  per decimal, without decimals (`xb_num_fixed_int_extra`), and with one of functions only a double of any width
  takes SET DECIMALS decimals; `Str()` and templates with digits are not affected;
- doubles keep 16 significant digits (later digits print as `0`) and are rounded half-up to the decimals as Clipper
  rounds them: when the shown digits hold at most 16 significant ones, the digit after the last one shown decides, a
  4 counting as a 5 when the digits after it up to the 16th significant one are 9s and the 17th is 2 or more. So
  `Str( 8904.185, 10, 2 )`, whose double lies a little below the half, is `8904.19`, and `Str( 9832.585, 10, 2 )`, a
  little further below, `9832.58` (`Round()` rounds both up: its rule is its own). The digits are the double's exact
  ones (`clipper_digits`, `core/numfmt.c`): Clipper takes the fraction's from repeated products by ten in doubles,
  whose error reaches the 13th decimal and beyond, so with 13 or more decimals xbase may show a digit closer to the
  value than the reference does (`Str( 8528950501312055 / 2 ^ 53, 18, 16 )` is `0.9469037222444915` here,
  `0.9469037222444916` there); a double that holds an integer below 2^52 in magnitude takes the integers' way, which
  gives the same text without computing a fraction's digits (`format_double`; digits two at a time, `uint_digits`);
- a minus sign appears only when a non-zero digit is shown (`-0.001` with 2 decimals prints `0.00`);
- a value that does not fit, and any infinite or NaN value, fills the field with `*`.

With more significant digits shown, the 16 are the first ones of the exact digits, rounded half up on the next one as
Clipper rounds them (`significant_digits`: an exact half after 16 digits goes up, `1286742750677284.5` shows
`1286742750677285`). Harbour derives them by repeated floating-point division, which can change the last significant
digits of values above 2^53 (`51090942171709440000` prints as `51090942171709430000` there, `2 ^ 60 + 512` as
`1152921504606848000` where its exact value is `1152921504606847488`).

## Functions

The functions of the language live in `runtime/src/rtl/`, by area; [coverage.md](coverage.md) lists which exist.
They are written in C (`*.c`) or in xBase (`*.prg`, see [Functions written in xBase](#functions-written-in-xbase)).
Conventions:

- A wrong argument type raises the function's own `BASE` error code (as in Clipper: `SubStr` 1110, `Left` 1124,
  `Year` 1112, `CMonth` 1116, `StrZero` 6003, ...). An optional argument passed as `NIL` counts as passed where
  Clipper says so: `SubStr( c, n, NIL )` and `Str( n, NIL )` are argument errors. A few functions return a neutral
  value instead (`Right` gives `""`, `Stuff` gives `""` unless its arguments are a string, two numbers and a
  string, `ATail`, `AClone`, `Array` with a non-numeric argument give `NIL`, the `Is*` tests give `.F.`).
- Number arguments that do not fit 64 bits saturate (`xb_num_to_i64`). Harbour converts them to its C integer type
  and gets wrapped values, so results for such arguments (`SubStr( c, 1e20 )`, `Round( n, -1e20 )`) may differ.
- A string result equal to its argument shares it instead of copying it.
- Number results carry the display width Clipper gives them: `Year` 5 columns, `Month`/`Day`/`DoW` 3, `Val` the
  width of the text it read (blanks, sign and every character before the dot) and the decimals after it; its value is
  the correctly rounded double of the digits read (`strtod`, as numeric literals in compiled code and in macros), also
  past 2^53 and with many decimals.
- `Str( n [, nWidth [, nDec]] )`: `nWidth` is the whole field; without it the number's own field is used (at most 90
  columns), and a `nWidth` below 1 means 10 columns. A field with no room for its decimals (`nDec >= nWidth`, or no
  integer column left) is all `*`. `StrZero` is `Str` with leading blanks as zeros.
- `Int( n )` is the integer part with `n`'s display width: a field's integer columns (`N(6,2)`: 3), a number's own
  width otherwise; an integer comes back as it is. `Abs( n )` of an integer not below zero is `n` as it is (its
  width); otherwise the magnitude with the default width (a double keeps its decimals).
- `Round` rounds half away from zero; a negative number of decimals rounds to tens, hundreds, ... As in Clipper, the
  half is added to ten times the scaled value and the result, divided back, is multiplied by `1 + DBL_EPSILON`
  (about one unit in the last place) before truncating, so decimal halves stored just below `.5` (`2.675`) round up
  while `0.4999999999` rounds down. That product is rounded once (`fma`), so a floating-point unit that computes with
  more bits (x87) gets the same result. A number whose scaled value reaches 2^52 (`XB_DBL_INTEGRAL`: the doubles there
  are at least a rounding step apart) is already the nearest double to its rounding and comes back as it is (the
  reference, as that formula, moves `4503599627370497` to `...499`). A scaling that overflows gives a non-finite
  result (shown as stars). `Int`
  truncates without that adjustment.
- `Sqrt` of a negative number and an `Exp` that overflows give 0; `Log` of a negative number gives minus infinity, like
  `Log( 0 )`.
- `^`, `Exp` and `Log` are xbase's own (`runtime/include/xbmath.h`), not the C library's, whose last bit differs from
  one system to another (glibc's and MinGW's round some arguments wrongly; the reference uses them): table-driven
  reductions (ln by 1 / (1 + j / 128), e^y by 2^(j / 64)), then double-double arithmetic (about 106 bits) rounded once,
  subnormal results too, so the result is the correctly rounded double, the same everywhere, except for an argument
  whose result lies within about 2^-90 of a tie between two doubles (`Exp( 2^-53 )` is one). `Exp` and `Log` first take
  a fast form (the table's value as a double-double, the reduced argument's terms in double, summed once: relative error
  below 2^-58; near the table's row of 1, `Exp` of |x| below 0.005 and `Log` of |x - 1| below 0.0039, where only the
  argument plus its terms is rounded, below 2^-51 |x| and 2^-51 |x - 1|, so that a value near 1 that lies close to a tie
  is still settled at once, `xb_math_exp_near_row1_err`, tried when 2^-58 cannot settle it), then for `Log` a quick form
  (the series in double beyond its first terms, below 2^-66; `^` takes the quick forms, below 2^-62 for e^y); each gives
  the result when its error bound cannot change the rounding (Ziv's strategy), else the next one, and the full form
  decides last (for about 1 argument in 128). `^`'s exact and halfway results (an integer exponent, or `n / 2^k` of an
  exact root, such as `7 ^ 19` and `1046529 ^ 1.5`) are computed exactly, ties to even, and `x ^ 0.5` is IEEE's square
  root. C's special cases of `pow` hold (`0 ^ 0` is 1, a negative base needs an integer exponent). The compiler folds
  `^` of constants with the same code, and its inline `Exp()` and `Log()` call `xb_exp` and `xb_log`; none of them
  writes `errno`. They cost more than the C library's (x86-64: `^` about 150 ns, `Exp` 22 ns, `Log` 30 ns, against 10, 4
  and 4 ns). The runtime and `xbc` are compiled without contracting `a * b + c` into fused multiply-adds and, on 32-bit
  x86, with SSE2 rather than the x87 unit (`xb_exact_doubles` in CMake), on which double-double arithmetic depends.
- The CA-Tools math functions (`core/trig.c`, `rtl/numbers.c`) are correctly rounded the same way, the same doubles
  everywhere: `Sin`, `Cos`, `Tan` and `Cot` reduce any argument exactly (`Sin( 10^22 )` is -0.8522008497671888), to a
  multiple of pi / 128 and a rest, by a table of sin and cos at those multiples (below 2^14 in one step with pi / 128 in
  parts of 33 bits; the full form through pi / 2 in parts below 2^20, from 1536 bits of 2 / pi beyond, by Payne and
  Hanek); `ATan`, `Atn2( nY, nX )` (C's atan2), `ASin` and `ACos` go through a table of atan( j / 64 ); `Sinh`, `Cosh`
  and `Tanh` through e^x (a series near 0); `Log10` through ln; `DToR` and `RToD` multiply by the constant as a
  double-double. Each takes a fast form first, then a full one in double-double (relative error near 2^-100) when the
  fast one's bound leaves the rounding open (x86-64: `Sin` and `Cos` about 27 ns, `Tan` 40, `ATan` 83, `Log10` 29, the
  hyperbolic functions up to 170, against 7 to 14 for glibc). On a target whose compiler guarantees a fused multiply-add
  in hardware (AArch64, x86-64 built for `x86-64-v3`), the exact products of `xbmath.h` (`xb_dd_prod`, `XB_MATH_FMA`)
  take it instead of Dekker's product: the same exact pair, so the same results, sooner (`Tan` 33 ns, `ATan` 68). Their
  results have SET DECIMALS decimals; outside the domain they are a NaN or an infinity (stars: `ASin( 2 )`, `Log10( 0
  )`, `Cot( 0 )`, `Sinh( 1000 )`), in the widths these take everywhere in xbase (a NaN 13 columns with 2 decimals, an
  infinity 23; the reference's CA-Tools shows its NaNs in 23 too). `Pi()` is the double nearest pi. `Floor` and
  `Ceiling` give integers (a double with no decimals past 64 bits), `Sign` -1, 0 or 1. `Fact( n )` truncates n and gives
  n! correctly rounded up to 170 (the reference loses digits from 21! and gives -1 past 21), -1 below 0 or past 170,
  with SET DECIMALS decimals. A missing or non-numeric argument is an argument error with xbase's codes 9036 (`Sin`) to
  9053 (`Fact`) in the order of `rtl/numbers.c` (the reference returns 0). These are not reserved names: a program's own
  function of one of them comes first.
- `Replicate` raises `String overflow` (1234) when the result size overflows.
- `PadL`/`PadR`/`PadC( x, nLength, [cFill] )` (`Pad` is `PadR`) pad a string, a number (as `Str` gives it, without
  its leading blanks) or a date (as `DToC` gives it) with the fill's first character (a blank by default, also for an
  empty fill, where the reference pads with zero bytes), or keep its first `nLength` characters; any other value, or
  a length that is not a positive number, gives `""`. `PadC` puts the extra half, rounded down, on the left.
- `Mod( n, nBase )` is dBase's modulus (the remainder takes the sign of `nBase`), with SET DECIMALS decimals; a zero
  base is error 1341 (`%`), after which the number comes back as it is. `Descend` gives each byte of a string as 256
  minus it (0 stays 0), a number negated (SET DECIMALS decimals), a date as `5231808` minus its day number, a logical
  negated, and NIL for anything else. `Bin2I`/`Bin2W`/`Bin2L` read a signed 16-bit, unsigned 16-bit and signed 32-bit
  little-endian number from a string's first bytes (zeros past its end); `I2Bin`/`L2Bin` write the low 16 or 32 bits
  of a truncated number.
- `SoundEx` keeps the first letter, then the digits of the next letters, a digit once for neighbors with it (A E I O U
  H W Y have none and part neighbors), skipping anything but a letter, padded with zeros to 4 characters.
- `Transform( x, cPicture )` (`rtl/transform.c`, the rules in its header): Clipper's functions (`@B @C @X @( @) @R
  @! @D @E @Z @S`, and `@L`/`@0` leading zeros) and templates for strings, numbers, dates and logicals, with their
  corner cases (`@R` on a date inserts the picture's separators again, `@E` on a string trades characters 1-2 and 4-5,
  a field too narrow is stars). Dates follow SET DATE and SET CENTURY (`xb_date_format_digits` formats the
  `"99999999"` template). A template without digits takes the number's own field, under SET FIXED with SET DECIMALS
  decimals (an integer's two columns wider and two per decimal: [Display](#display)). Without a picture: a string as
  it is, a number as `?` shows it (under SET FIXED an integer in that wider field), a date as `DToC`, a logical as
  `T`/`F`; any other value, or a picture that is
  not a string, is error 1122. Compared with the reference over random values, pictures and settings, the results are
  the same.
- The memo-line functions (`rtl/memoline.c`) cut a text into lines of a width (79 by default; tab size 4, word wrap on):
  `MemoLine`, `MLCount`, `MLPos`, `MLCToPos`, `MPosToLC` share one scanner, Clipper's rules and their corner cases (a
  line ends at a hard return, CR LF, or before the character that would pass the width: with word wrap after the line's
  last blank; a soft return, the byte 141 and LF, is skipped with word wrap and ends the line without it; a tab takes
  the columns to the next multiple of the tab size). A width or column past what `size_t` holds (32-bit hosts) is as
  wide as no line (`xb_size_sat`), never the value narrowed; `MemoLine`, whose line is padded to the width, gives the
  string overflow for a width past the largest string. The reference reads its line end from a setting and takes more
  arguments; xbase takes Clipper's arguments and CR LF. Compared with the reference over random texts (letters, blanks,
  tabs, hard and soft returns, lone CR and LF) and arguments, the results are the same. `HardCR` makes soft returns
  hard; `MemoTran( c, [cHard], [cSoft] )` replaces hard and soft returns by the first character of its arguments (`;`
  and a blank by default, also for an empty one, where the reference writes a zero byte).
- `MemVarBlock( cName )` returns a block that reads the memory variable and, called with a value that is not NIL,
  assigns it; NIL when no variable of that name is visible.
- `StrTran( c, cSearch, cReplace, nStart, nCount )`: `nStart` or `nCount` of 0 gives `""`; a negative `nStart`
  replaces nothing, a negative `nCount` replaces every occurrence from `nStart`. The text between matches is copied
  whole.
- Searching bytes in bytes (`At`, `RAt`, `$`, `StrTran`, `Split`: `xb_bytes_find`, `xbrt.h`) is linear whatever the
  texts. A needle of up to 16 bytes (its length bounds the work at each place) is looked for forward with `memchr` of
  its first byte, each place tested in place (first and last bytes, then the rest); where `memchr` comes back within
  `XB_FIND_SPARSE` bytes (a common first byte, as in text) the next `XB_FIND_STRETCH` places are looked at eight at a
  time (`xb_find_places8`: the places where its first byte and, `len - 1` further, its last byte stand, from two
  little-endian words, `xb_le64`), then `memchr` again; backward, eight places at a time throughout. A longer needle
  is looked for with the two-way search (Crochemore and Perrin's critical factorization: no needle and haystack make
  it quadratic), the last occurrence as the first one of the reversed strings.
- `CHARACTER:Split` is one pass in C (`__Split`), `StartsWith` and `EndsWith` compare in place (`__Affix`).
- `Stuff( c, nStart, nDelete, cInsert )`: `nStart` 0 is 1; a negative `nStart` or `nDelete` counts as past the end.
- Trimming from the left, `Val` and `Empty` skip blanks, tabs, carriage returns and line feeds (`xb_is_blank`); trimming
  from the right removes spaces only. Case changes and the character classes (`IsAlpha`, `IsUpper`, `IsLower`) read
  letters as SET CODEPAGE says ([Codepages](#codepages)); `IsDigit` takes ASCII digits.
- `Empty` is true for `NIL`, `0`, a string of blanks (as above), `.F.`, an empty array, an empty hash and the empty
  date.
- `Set( n [, x] )` returns the setting before the call. `_SET_EXACT` takes a logical or a string starting with
  `ON`/`OFF`; `_SET_DECIMALS` and `_SET_EPOCH` take a number of 0 or more (a negative one is error 2020; so is a count
  of decimals past 65535, the most a number keeps, which the reference keeps and formats modulo 65536); `_SET_FIXED`,
  `_SET_DELETED`, `_SET_SOFTSEEK` and `_SET_UNIQUE` take a logical or `ON`/`OFF` as `_SET_EXACT` does
  ([rdd.md](rdd.md#orders)); `_SET_DATEFORMAT` takes a string (kept whole) and sets `SET CENTURY` on when its first run
  of `Y`s has 4 or more, off otherwise; `__SetCentury( [lOn] )` rewrites that run of `Y`s as `YYYY` or `YY` (or puts it
  in front when the picture has no `Y`) and the picture in upper case. `_SET_LANGUAGE` takes a language's name
  ([Languages](#languages)), `_SET_CODEPAGE` a codepage's ([Codepages](#codepages)). Other values change nothing.
- `__GcCollect( [lYoung] )`, `__GcMemory()`: see [Garbage collector](#garbage-collector). `__MemNamesInUse( [lLetGo] )`:
  see [Memory variables](#memory-variables). `__dbPageCache( [nBudget] )`: see [rdd.md](rdd.md#performance).
- `AScan( a, xSearch, [nStart], [nCount] )` returns the position of the first element of the range equal to `xSearch`
  (values of one comparable kind, compared like the operators, strings with SET EXACT; two `NIL`s; arrays and blocks
  never), or for which a block `xSearch( element, position )` returns `.T.`, else 0 (also for anything but an array, and
  without `xSearch`). `AEval( a, bBlock, [nStart], [nCount] )` calls `bBlock( element, position )` for each element of
  the range that still exists when its turn comes and returns the array (anything else: `BASE/2017`). In both, and in
  `ACopy`, `AFill` and `ASort`, `nStart` missing, not a number or 0 is 1, and negative or past the end gives no range
  (`xb_range_start` in `rtl/collections.c`); in `AScan`, `AEval` and `ACopy`, `nCount` 0 is nothing and a negative or
  missing one the rest (`xb_range_count`). The block's arguments are roots, and the array is read again after each call;
  blocks may run these functions again to any depth.
- `AIns( a, nPos )` moves the elements from `nPos` one place up, losing the last, and stores `NIL` at `nPos`;
  `ADel( a, nPos )` moves those after `nPos` one place down and stores `NIL` in the last. Both keep the length, take
  0 as 1, do nothing outside the array and return it (`NIL` for anything else).
- `ACopy( aSource, aTarget, [nStart], [nCount], [nTargetPos] )` copies the range into `aTarget` from `nTargetPos`
  (missing or 0: 1; past the end or negative: the last position), as many elements as fit, and returns `aTarget`
  (`NIL` unless both are arrays). Every store into an array goes through the write barrier.
- `AFill( a, xValue, [nStart], [nCount] )` stores `xValue` in `nCount` elements from `nStart` and returns the array;
  `nStart` follows `ASort`'s rule, `nCount` missing or reaching past the end means the rest, 0 fills nothing, a
  negative one the rest from the first element and nothing from another (Clipper's rule), and a missing `xValue`
  nothing. Anything but an array is an argument error (`BASE/6004`). The range is written at once
  (`xb_array_fill`, through `__ArrayFill`): the array takes the form that holds the value first, an array of logicals
  gets its bytes in one pass, the write barrier covers the range when the value refers to an object, and a shared
  array's elements are each stored under its lock.
- `ASort( a, [nStart], [nCount], [bOrder] )` sorts in place and returns the array (`NIL` for anything else). `nStart` 0,
  missing or not a number is 1, one past the end or negative sorts nothing (`xb_range_start`, which `AFill` shares);
  `nCount` missing, 0, negative or reaching past the end means the rest. The sort is stable (a merge sort of positions:
  equal elements keep their order; one algorithm, `XB_DEFINE_MERGE_SORT` in `rtl/collections.h`, compiled once per
  comparison: without `bOrder`, a range of integers only or of doubles only is sorted comparing them in place). Without
  `bOrder`, a range of 256 or more integers only or doubles only is sorted by radix instead (`radix_sort`, stable too:
  the values as unsigned keys, 16 bits a pass, a pass skipped when every key has the same digit there; `-0.0` as `0.0`,
  which compares equal; a NaN, which orders before nothing, leaves the range to the merge sort). `bOrder(
  x, y )` says whether `x` goes first (a number: whether it is not 0; any other value: yes); without it, values of one
  kind compare like the operators (strings follow SET EXACT, integers exactly, dates by day, `.F.` before `.T.`) and
  other pairs by kind: array, object or hash, block, string, logical, date, number, `NIL`. The block's arguments are
  kept as roots while it runs, and moving the elements goes through the write barrier.
- The bit functions, xbase's own: `BitAnd`, `BitOr`, `BitXor`, `BitNor` (two numbers), `BitNot` (one), `BitShl` and
  `BitShr` (a number and a shift count) work on the 64-bit two's complement form of integers (`xb_bit_apply` in
  `xbrt.h`): numbers are truncated to integers (saturated; NaN is 0), `BitShr` is a logical shift and a count outside
  0..63 gives 0. Anything else than a number is an argument error. xbase's own functions have `BASE` codes from 9001
  (`BitAnd` 9001, `BitOr` 9002, `BitXor` 9003, `BitNor` 9004, `BitNot` 9005, `BitShl` 9006, `BitShr` 9007). The bit
  operators raise the code of their operation: `&` 9001, `|` 9002, `^^` 9003, `~` 9005, `<<` 9006, and `>>`, an
  arithmetic shift (a count past 63 gives 0, or -1 for a negative number), 9008. The function names, operator symbols,
  operand counts and codes are the table `xb_bit_function` in `xbrt.h`, which the compiler reads too. The `Char*()`
  functions and `CPConvert` have 9009 to 9015 ([Codepages](#codepages)), the thread and task functions 9016 to 9031
  ([concurrency.md](concurrency.md#threads)), a size no memory holds 9032 (`Memory low`), and the `CHARACTER` methods
  given an argument that is no string `Split` 9033, `StartsWith` and `EndsWith` 9034, a channel enumerator's
  `MoveNextAsync` (and an asynchronous generator's) while another of its own waits 9035.
- `Seconds()` gives the seconds since midnight, local time, to the millisecond, with SET DECIMALS decimals;
  `Time()` the local time as `"hh:mm:ss"`. `ElapsedMs()`, an xbase function, gives the milliseconds since the
  program started from a monotonic clock (`xb_program_elapsed_ms`), for timing: the difference of two calls.

Settings live in the environment of the flow ([Threads and synchronization](#threads-and-synchronization); start: `SET
EXACT OFF`, `SET DECIMALS TO 2`, date format `mm/dd/yy`, `SET EPOCH TO 1900`, `SET CENTURY OFF`), but for `SET
DECIMALS`, which compiled code reads from the thread context, where it is kept. Their numbers come from
`include/set.ch`, shared with xBase code.

## Functions written in xBase

C keeps the primitives: values, the collector, the storage of arrays and hashes, operations on the bytes of strings,
number formatting, date arithmetic, sorting without a block, and the operating system (files, the console). Functions
built from other functions, and rules such as where output goes, are written in xBase: memory-safe by construction
(the compiler roots everything), the same source on every target, and faster with every improvement of the compiler.
Each one moved from C is measured against its C version (callgrind).

- The `*.prg` files of `runtime/src/rtl/` (listed in `runtime/CMakeLists.txt`) are compiled by the `xbc` of the same
  build with `--runtime` ([compiler.md](compiler.md#calls)) for `XB_TARGET` (the C compiler's
  `CMAKE_C_COMPILER_TARGET`, else the host; a build that changes the target by flags, such as `gcc -m32`, names it:
  `-DXB_TARGET=i686-pc-linux-gnu`), and their objects go into `libxbrt` with the C ones.
- Their public functions are runtime functions: `xbf_NAME`, declared `NATIVE` in `include/rtl.ch` like the C ones
  (they receive values where the declaration has no `@`), in the runtime's symbol table, and checked by the
  `runtime.declarations` test (`FUNCTION`/`PROCEDURE` lines of the `.prg` files).
- On the call stack they show with line 0, as the C functions do; their `STATIC` functions and codeblocks do not show
  (`XB_PROC_HIDDEN`), though `PCount()` and `PValue()` still read their frames.
- The compiler carries some of these sources (`XB_INLINE_PRG` in `compiler/CMakeLists.txt`: `rtl/arrays.prg`), so that
  a program's calls of their functions may run in place ([xir.md](xir.md#passes)), with the same frames on the call
  stack: line 0, no source file, the hidden ones hidden. Their functions are written so that a call with a codeblock
  literal, inlined, folds to its array path (`ValType()` of a block or an array literal is known).
- Primitives for them: `__ConsoleWrite( lNewLine, [xValues...] )` writes a new line when `lNewLine` is `.T.`, then
  the display forms of the values ([Display](#display)) separated by spaces, to the console (standard output until the
  terminal layer exists); `__StdWrite( nHandle, [xValues...] )` writes the values so to the standard output (1) or
  error (2), flushing the output before writing to the error. A call gathers its bytes first (`xb_buffer`,
  `core/text.h`: each display form copied as it is made), then writes them in one blocking operation under the output
  lock (`xb_output_lock`, `core/sync.h`, which the report of an unrecoverable error takes too), so no other thread's
  output splits them.
  `__ArgError( nSubCode, [lSubstitute] )` raises the calling function's argument error (`EG_ARG`, its name as the
  operation, the arguments it received), as its C counterparts do: not substitutable, or with `lSubstitute`
  substitutable, the handler's value its result.
- Written in xBase: `QOut`, `QQOut`, `OutStd` and `OutErr` (`rtl/console.prg`), `AEval` and `AScan`
  (`rtl/arrays.prg`; they also go through collection objects, those `__IsCollection( xValue )` (`core/control.c`)
  finds `FOR EACH` goes through: an `Enumerable`, a generator's enumerator, from position `nStart`, `nCount` elements;
  another object is `AEval`'s argument error and `AScan`'s 0; `AScan` finds a value in an array with the primitive
  `__ScanValue`, and in a collection with `__ValueMatches`, both `xb_value_matches`), `AFill`, `ACopy`, `HEval`, `HScan`
  (a value through `__ScanValue` too) and `HMerge` (without a filter, `HCopy`) (`rtl/arrays.prg`), the `Enumerable`
  class and the classes of the built-in types (`rtl/collections.prg`, see [Classes](#classes)), `ErrorSys` and the
  default error handler (`rtl/errorsys.prg`), Clipper's time sample functions `Secs`, `TString`, `Days`, `ElapTime`,
  `AMPM`, and `LenNum` (`rtl/time.prg`), `Directory`, `ADir`, `File`, `MemoRead`, `MemoWrit` and `FReadStr`
  (`rtl/files.prg`, see [Files](#files)), `CDoW` and `CMonth` (`rtl/dates.prg`) and the languages (`rtl/languages.prg`,
  see [Languages](#languages)). Measured against their C versions (callgrind, x86-64): 200 000 `? i, "abc", .T.` take
  267M instructions (220M in C; about 6% more time to `/dev/null`); `AEval` over a million elements with
  `{| x | n += x }`, 121M (116M). Timed against the C versions they replaced (100,000 elements, 300 rounds): `AScan` of
  a value and with a block 225 ms (170 ms in C), `AFill` with `ACopy` 108 ms (76 ms), `HEval` with `{| k, v | n += v }`
  362 ms (290 ms); `HScan` of a value and with a block 347 ms (300 ms), `HMerge` with a filter 807 ms (927 ms); `CDoW`
  with `CMonth`, a million pairs, 103 ms (57 ms). `QQOut` keeps its own loop instead of sharing `OutStd`'s: passing the
  values on through one more call costs `QOut` 13% more cycles.

## Dates

A date is a Julian day number (0: the empty date) (`types/date.c`). Dates are built for years 0 to 9999; arithmetic can
go further.

- Day numbers from year 0 up to 2147483647 decode to a calendar date; text shows the last four digits of the year.
  Negative day numbers show like the empty date, and other day numbers outside that range have no fields (zeros, `0`
  from `Year`/`Month`/`Day`/`DoW`, `""` from `CMonth`).
- `date + n`, `n + date` and `date - n` give dates (`n` truncated to whole days); `date - date` gives the number of
  days; `date + date` adds the day numbers, as Clipper does. Dates compare by day number.
- Text follows the SET DATE FORMAT picture (`xb_date_format`): runs of `D`, `M`, `Y` become digits (with tens `t` and
  units `u`, a run of 4 gives `tttu`, 3 `ttu`, 2 `tu`, other lengths `u` and the remaining letters; a year run of 4, 3
  or 2 gives its last digits), other characters are copied in upper case, at most 10 characters
  (`XB_DATE_TEXT_SIZE`); the empty date shows blanks for the digits.
- `CToD` reads the fields in the order the picture gives them; each run of digits fills a field and the first
  non-digit after it moves to the next. Two-digit years fall in the hundred years starting at SET EPOCH. Invalid
  dates give the empty date.
- `CDoW`/`CMonth` (`rtl/dates.prg`) give the names of the language `SET LANGUAGE` selected ([Languages](#languages);
  `""` for the empty date, and from `CMonth` outside the calendar); `DoW` counts from Sunday = 1. `DoW` works from the
  day number (0 outside the calendar); `CDoW`, as in Clipper, applies the Gregorian weekday formula to the decoded year,
  month and day, so it differs from `DoW` in year 0 and gives Wednesday for day numbers outside the calendar. Something
  that is no date is their argument error (1117, 1116), whose handler may give the result.

## Languages

`SET LANGUAGE TO <id>` (`Set( _SET_LANGUAGE, cId )`; `_SET_LANGUAGE` is xbase's own setting, numbered from 200 as
`set.ch` says) selects the language of the runtime's texts: the names of the days and months, and the descriptions of
the errors it raises. The languages are written in xBase (`rtl/languages.prg`): `__LanguageTexts( cId )` makes the texts
of `EN` (English, the default), `PT` (Portuguese) or `ES` (Spanish) in the parts `rtl/language.ch` numbers (the same
file for xBase and C), NIL for any other name. The context keeps the current language's texts (`xb_env.language`, a root
of the collector; English, made when first needed), `__LanguageCurrent()` gives them to xBase code, and
`core/language.c` gives an error's description to `core/error.c`: the subcode's own text (the part of subcodes: by
generic code, a hash of subcodes), else the generic code's. `Set( _SET_LANGUAGE )` returns the current language's name;
a name in any letter case selects, and one the runtime has no language for leaves it as it is. A description an error
carries itself (a class error's, ...) stays English. `__LanguageCurrent()` hands out the very texts the runtime reads,
which are never to be changed. The English texts, which every environment that starts without texts takes (a thread's,
an asynchronous function's), are made once for the process and shared (`core/language.c`, a root of the collector): an
error raised in a new environment does not make them again. The texts are UTF-8 bytes (`Len( "Terça-feira" )` is 12), as
the source is.

## Codepages

Strings are bytes: `Len`, `SubStr`, `Left`, `Right`, `At`, `Asc`, `Chr` and the rest count bytes, as in Clipper. `SET
CODEPAGE TO <cp>` (`Set( _SET_CODEPAGE, cName )`, xbase's own setting 201) says how the functions that read characters
take them (`core/codepage.c`, `xb_env.codepage`, per environment): `UTF8` (the default), or the single-byte `CP437`,
`CP850` (DOS), `CP1252` and `ISO8859-1` (Windows; other names: `UTF-8`, `IBM437`, `IBM850`, `WINDOWS-1252`,
`ISO-8859-1`, `LATIN1`, in any letter case). `Set( _SET_CODEPAGE )` returns the current one's name; an unknown name
leaves it as it is.

- `Upper` and `Lower` change ASCII letters eight bytes at a time and, only when a string has bytes above 0x7F, the
  other letters as the codepage reads them; a string never changes length: in UTF-8 a letter whose other case takes
  another number of bytes keeps its case (`ß`, `ı`). `IsAlpha`, `IsUpper` and `IsLower` read the first character.
- The data (`core/codepage_data.c`, generated: it says how) holds the Unicode 18.0 letters by class (uppercase,
  lowercase, other) and the simple case mappings that keep the UTF-8 length, as runs of code points, and for each
  single-byte codepage the code point, the upper and lower case and the class of each of its bytes. Writing a character
  in a single-byte codepage searches that codepage's table backwards (its 256 pairs of character and byte in the order
  of the character, made once for all the codepages: the lowest byte of a character), not its 256 bytes.
- In UTF-8 a byte that starts no valid sequence (overlong forms, surrogates and code points past U+10FFFF included) is
  a character of its own, U+FFFD, and keeps its case.
- xbase's own functions count characters: `CharLen( c )`, `CharSubStr( c, nStart, [nCount] )` (a negative start counts
  from the end, as `SubStr`'s), `CharLeft( c, n )`, `CharRight( c, n )`, `CharAt( cSearch, c )`, `CharAsc( c )` (the
  code point of the first character) and `CharChr( nCode )` (`""` for a code point the codepage has not); `CharAt` takes
  a match only where a character starts. Their argument errors have the codes 9009 (`CharLen`), 9010 (`CharSubStr`),
  9011 (`CharLeft`), 9012 (`CharAt`), 9013 (`CharAsc`) and 9014 (`CharChr`); `CharRight`, like `Right`, gives `""`.
- `CPConvert( cText, cFrom, cTo )` reads the text in one codepage and writes it in another: a character the target has
  not becomes `?`; between a codepage and itself the text comes back as it is. A byte `CP1252` leaves undefined (0x81,
  0x8D, 0x8F, 0x90, 0x9D) stands for the code point of its own value, so it converts both ways. An unknown codepage, or
  an argument that is no string, is its argument error (9015).
- Comparisons and sorting compare bytes; the console writes the bytes as they are.

## Files

The runtime reaches files through the platform layer (`platform/platform.h`): files are opened with an `FOpen()` mode
(`xb_platform_file_open`), created (`xb_platform_file_create`, open for reading and writing, with `FCreate()`'s
attributes), read, written (also at a position, in one call: `xb_platform_file_read_at`, `_write_at`, which tables and
indexes use), moved in (`xb_platform_file_seek`) and closed on the system's handle as an integer, deleted and renamed.
Tables, memos and indexes also take a sharing (`xb_platform_file_share`: `flock` on POSIX, the share mode at opening on
Windows), byte-range locks (`xb_platform_file_lock`: `fcntl`, `LockFileEx`) and the file's identity
(`xb_platform_file_id`), through the registry of shared files ([rdd.md](rdd.md#inside-the-process)); a file can be
opened without being emptied, created when missing (`xb_platform_file_open_always`). The layer also tells a file's entry
(`xb_platform_file_info`), lists a directory (`xb_platform_dir_open`, `_next`, `_close`: name, size, attributes, time of
last change in local time), whether names differ by letter case, the current directory, the free space of a drive and an
environment variable (for `GetEnv()` only). Paths are UTF-8 (UTF-16 on Windows). A failure gives the code `FError()`
reports: a DOS error number (2 file not found, 3 path not found, 5 access denied, 25 seek error, ...), which Windows
reports itself and the POSIX build translates `errno` values to (31, general failure, for those without one). A name
that is missing is 2 and a directory on its way that is missing is 3, on POSIX systems as on Windows, where `errno`
alone does not tell them apart.
`xb_platform_path_separator` tells the characters that separate directories (`/`; also `\` and `:` on Windows), and
`xb_platform_eol` the end of a line of text (LF; CR LF on Windows: the lines of the text files tables are copied to).
Binary fields of the file formats go through the little-endian helpers of `core/bytes.h`. `SAVE` and `RESTORE` ([Memory
variables](#memory-variables)) use this layer too.

- `include/fileio.ch` (`FO_*` modes, `FS_*` origins, `FC_*` attributes, `F_ERROR`) is included by the runtime's C as
  well, as `set.ch` is; `include/directry.ch` names the positions of a `Directory()` entry.
- `rtl/files.c`: `FOpen`, `FCreate`, `FRead`, `FWrite`, `FSeek`, `FClose`, `FError`, `FErase`, `FRename`, `CurDir`,
  `DirChange`, `DiskSpace`, `GetEnv`, `__DirScan` and `__Run` (the `RUN` and `!` commands: the command line through the
  system's shell, `sh -c` or `cmd.exe`, after the program's output is flushed, waited for; `ErrorLevel()` stays). A
  handle is the system's handle as a number; `FOpen`, `FCreate` and `FClose` go through the registry of shared files, so
  closing a table's file keeps the areas' locks, and neither empties a table nor keeps its areas from it
  ([rdd.md](rdd.md#inside-the-process)). Each function stores the code of its outcome for `FError()`
  (`xb_env.file_error`, 0 after a success); an argument of the wrong type gives the function's failure value and leaves
  `FError()` alone, but a name that is not a string is `FOpen()`'s `BASE/2021`, and an access of 3 is 5. `FRead(
  nHandle, @cBuffer, nBytes )` replaces the first bytes of the buffer, which keeps its length; without the buffer passed
  by reference, or with `nBytes` below 1 or past its length, it reads nothing (a bad handle is still `FError()` 6).
  `FWrite` writes the whole buffer for a count past its length or below 0, and writes nothing, successfully, for a count
  of 0. A seek before the start is 25 from any origin (an unknown origin is the start) and leaves the position, which
  `FSeek` returns. `CurDir()` gives the current drive's directory without its drive and leading separators (its argument
  is not used); `DiskSpace( [nDrive] )` counts the drive on Windows only. `__DirScan( cSpec )` gives `{ cName, nSize,
  dDate, cTime, cAttributes }` for each entry, of any attributes (letters in the order `RHSDA`), whose name matches the
  mask of `cSpec` (`*` and `?`; a mask ending in `.*` also takes a name without a dot, so `*.*` takes every name, as in
  DOS; letter case counts where the file system tells it, and is folded for ASCII letters only); a mask without
  wildcards names one entry, and none (a directory's name with its separator) every entry of the directory. A link that
  leads nowhere is listed as the link itself. On POSIX systems a name starting with a dot (but `.` and `..`) is hidden
  (`H`); on Windows the attribute is the file system's.
- `rtl/files.prg`: `Directory( [cSpec], [cAttributes] )` lists the files without the attributes D, H and S, and those
  whose such attributes `cAttributes` all names (`V`, the volume label, gives none); `ADir` fills its arrays from it
  (directories, hidden and system files too when the attributes' array is given; 0 without `cSpec`); `File()` is whether
  a file, hidden or system too but not a directory, matches; `MemoRead` reads a whole file without the end mark
  (`Chr( 26 )`) that ends it, `MemoWrit` writes the text and that mark; `FReadStr` reads and stops at the first NUL (the
  file position past all the bytes read).
- Safer than the reference on POSIX systems, as DOS and Windows behave: a directory cannot be opened (`FOpen`) or
  deleted (`FErase`): 5; `FRename` never replaces another existing file: 5; reading a handle opened `FO_WRITE`, or
  writing one opened `FO_READ`, is 5 (access denied), not 6; a read-only file shows `R`. POSIX systems
  have no sharing modes (the sharing bits of `FOpen()` are ignored there), and they mark names starting with a dot
  hidden, which `Directory()` lists only with `H`, whatever the mask. `MemoRead` and `MemoWrit` leave the failure's code
  in `FError()`. On Windows, `FCreate` of an existing hidden or system file keeps those attributes. File offsets are 64
  bits everywhere.

## Work areas

The work areas, their tables and the database functions are in `runtime/src/rdd/` ([rdd.md](rdd.md)). Each environment
has its own (`xb_env.areas`, `areaCurrent`, `set_deleted`, `rddDefault`, `netErr`). Compiled code reaches fields through
three entry points of `xbrt.h`, each call with a cache of its own (`xb_field_cache`, a zeroed static of the reference:
the `generation` of the table it found the field in, from a process-wide atomic 64-bit counter, and the field's
position, in one 64-bit word read and written atomically, so threads find either their table's entry whole or another
one; the word keeps 32 bits of the generation, 16 of the area's number and 16 of the position, and a table past any of
them is looked up each time; an alias written in the code is found through the area's number while its generation
lasts):

- `xb_field_get( ctx, r, area, alias, name, cache )` reads field `name` (upper case) of the work area that the item
  `area` names (an alias or a number, for `( expr )->name`), else the C string `alias`, else the current one (both
  NULL); `xb_field_set( ctx, area, alias, name, cache, v )` writes it. A field that is not there is `BASE/1003`, an
  alias that names no area `BASE/1002`, both retried while the error block returns `.T.`.
- `xb_mv_field( ctx, r, memvar, cache )` reads an undeclared name: the current area's field of the variable's name,
  else the memory variable (as `xb_mv_get`).

`__AreaEval( xArea, bBlock )` (`alias->( expr )`) and the macro machine's `MO_AREA` make an area current through
`xb_area_enter`, which raises the alias error.

## Threads and synchronization

The program's threads (`ThreadStart`, `Mutex`, `AtomicInt`: [concurrency.md](concurrency.md#threads)) and the
services below, which tasks will be built on too ([concurrency.md](concurrency.md)).

- Contexts (`core/context.h`): a thread context (`xb_ctx`) holds what the code running on its thread uses (the `[ABI]`
  head compiled code reads: pending requests, `SET DECIMALS`, the chain of frames, the stack limit; the C roots, a
  `BREAK` on its way, the error blocks running, `Type()`'s trap) and caches of its own (the macro cache), and points at
  the environment of the flow it runs (`xb_env`): `ErrorBlock()`, the memory variables, the `SET` values, `FError()`,
  the language and the codepage, the work areas and their settings. Each thread context has one environment, made with
  it. A new flow's starts as a copy of its creator's: `xb_env_capture` makes a snapshot of it, an array that the thread
  or task object keeps (the scalar `SET` values, `SET DECIMALS` too, as the bytes of a string; the date format;
  `ErrorBlock()`; the language's texts; the PRIVATE bindings, pairs of a name, its number when it is permanent or else
  its text, and its cell), and `xb_env_restore` gives a new environment its values; work areas, none. A task runs in an
  environment of its own, made from its snapshot, that `xb_env_enter` puts in front of the thread's (the one set aside
  is `xb_env.outer`, marked by the collector too) and `xb_env_leave` closes and takes away. An asynchronous function's
  environment is its own from its call to its end: `xb_env_inherit` makes it from its caller's directly (no snapshot:
  the structure copied, the PRIVATE bindings shared until either changes them, no work area), and between steps
  `xb_env_park` sets it aside (`SET DECIMALS` kept) until `xb_env_unpark` puts it back in front of the thread's. The SET
  values that only `SET` changes (`EXACT`, `FIXED`, the date format, `EPOCH`, `CENTURY`, the codepage, `DELETED`,
  `EXCLUSIVE`, `SOFTSEEK`, `UNIQUE`, the default driver) are an `xb_settings` the environments share, counted (atomic):
  an environment that changes one takes a copy of its own first (`xb_settings_own`), so its caller and the functions it
  started keep theirs, and a suspended function costs its environment (152 bytes) but no settings; `ErrorBlock()` and
  the language's texts, which `BEGIN SEQUENCE WITH` and a first use change, are each environment's own. An environment
  joins a list when its function first suspends and leaves it when the function ends (64 shards by its address, each
  with a word lock, `xb_word_lock`: one compare-and-swap and one exchange), so a suspension and a resumption take no
  lock: a suspension adds the environment to its thread's list of recent ones (`xb_ctx.recent`, once, no lock; a thread
  that ends gives its list to the orphans), which the next two young collections scan (a parked environment does not
  change, and all it reaches is old by then), and a full collection scans every listed one: a young collection costs
  what was parked lately, however many functions wait. An environment that ends while a recent list holds it is freed by
  the collector (`ended`). An asynchronous generator's pipeline of three takes 10% fewer instructions and 14% fewer
  cycles than with a lock at every suspension and resumption. `xb_env_parked_settle`, after each collection's marking,
  moves the environments whose tasks it found unreachable to a list that `xb_env_reap` closes (their work areas, then
  the rest): the collection raises `XB_PENDING_FINALIZE` on the context that runs destructors, whose next pending test
  closes them (and so does the next step any thread runs), and `xb_env_reap_all` closes every one left suspended at the
  program's end. Each thread context keeps the last environment that ended for the next one (`spareEnv`): an
  asynchronous call that does not suspend allocates no environment, nor one that ends on the thread whose recent list
  has it last (it leaves the list then); one that ends while another place lists it is freed by the next collection. One
  that creates PRIVATEs copies the bindings it shares (once) and grows its own stack of PRIVATEs. The program's exit
  code is the process's.
- Program threads (`rtl/threads.c`): `xb_program_thread_start` starts a thread with the stack it is given (reserved,
  committed as used): 16 MB for `ThreadStart`'s (`XB_PROGRAM_THREAD_STACK`), 4 MB for the scheduler's workers and its
  timers' thread (`XB_WORKER_STACK`: most of it for the program's calls, above the stack error's reserve,
  `XB_STACK_RESERVE`), detached (`xb_thread_detach`), counted until it ends, that attaches (the second attached thread
  publishes the program's storage), makes its context (with a `QUIT` pending while the program ends) and runs its body;
  `ThreadStart`'s and the scheduler's workers are such threads. `ThreadStart` makes the `Thread` object (its block and
  arguments in an array, the snapshot of the creator's environment), shares it and starts the thread, which stores its
  `xb_thread_self` and tells the creator, which waits for that in a blocking operation (the thread object stays on its
  stack, pinned). The thread restores the environment, evaluates the block (`xb_eval_work`), stores the result, sets its
  done word and wakes the joiners, frees its context and detaches from the collector. A `Mutex` is three words in its
  object's own variables (the state, 0 free, 1 held, 2 held with waiters; the holder's `xb_flow_self`; the depth), a
  `Thread`'s done flag one: changed with atomic operations, waited on in the parking lot in a blocking operation, so
  collections go on, until its value changes or a `QUIT` comes: the waiter publishes the word in its context
  (`waitingOn`) before it tests for a `QUIT`, the park tests the context's pending `QUIT` again under its bucket's lock
  (`xb_park_wait_blocking_stop`), and `xb_gc_raise_all` raises the `QUIT` and then wakes the word it reads there, so no
  wait outlives a `QUIT` and none looks at the clock meanwhile; the object a waiter waits on is pinned by its stack.
  While the program is ending (`ending`, stored before the `QUIT` reaches the contexts), a waiter whose `QUIT` is set
  aside (`ALWAYS` code running, counted in `xb_ctx.quitAside` by `xb_seq_suspend` and `xb_seq_resume`), or the context
  ending the program (`xb_threads_end`), waits no more; any other waits for its `QUIT`, so that it leaves with the
  request pending (a `Join()` that saw `ending` first once returned with none, and its caller went on: seen on ppc64).
  An `AtomicInt`'s integer is changed by a compare-and-swap of its word once it is shared (`xb_seq_int_step`; under its
  stripe's lock where items' words are 4 bytes), in place while it is local. `QUIT` in a thread raises it on every other
  context (`xb_gc_raise_all`); the program's end (`xb_threads_end`, before the `EXIT` procedures and after them) does
  too when threads still run, and waits until none does; a thread made meanwhile quits at once. A `Mutex`'s `Hold()`
  returns a `MutexHolder` (the mutex in its own variable) whose `Dispose()` releases it once; `SYNCHRONIZED` gets the
  mutex's own `MutexReleaser`, made with it (`__Synchronize`: nothing allocated per entry). A `Mutex`'s and a
  `Semaphore`'s asynchronous waiters (`LockAsync`, `HoldAsync`, `WaitAsync`) are a ring (`xb_ring`) of `{task, flow,
  value}` in the object's variables under a short lock of their own, with their count in a word kept equal to the ring's
  under that lock: the task is a waiter's (`xb_task_waiter`: its token cancels it, its timer completes it with the value
  it was made with); a release that frees what they wait for, after a sequentially consistent fence (for a `Mutex`, the
  sequentially consistent change of its state word, `xb_rwords`, orders it: no fence), reads the count and, when there
  are waiters, takes the lock and grants them in turn (the lock taken for the waiter's flow, or a place; the waiter
  claimed, then completed once the lock is released; one that ended meanwhile passed over, one that ends as it is
  granted given back), and a new waiter, counted before a fence and its try once it is in the ring, tries again: one of
  the two sees the other. A new waiter goes in line whenever one waits (strict order), unless its flow holds the lock
  already. A `Mutex` also keeps how many waiters each flow has in line (a hash): once a flow holds the lock, granted or
  taken with `Lock()` or `TryLock()`, its waiters further in line take it too at once (they would wait behind others for
  what their flow has). A `Condition` keeps its waiters, the signals given to them (16 bits each: a waiter takes a
  signal and leaves in one compare-and-swap, so a signal never outlives the waiters it was for) and the generation of
  the last signal (32 bits) in one 64-bit word: a waiter takes only a signal given after it came (one that starts
  waiting right after a `NotifyAll()` would take another's otherwise, which would wait on unwoken), and parks on a
  second word that each signal, and each departure from a full word, changes (one that a `Notify()` woke for another
  passes the wake on); a `Semaphore` its count and maximum (`Release( n )` wakes one waiter when the count leaves 0:
  each that takes a place with more left wakes the next). Their waits take a limit in milliseconds on the monotonic
  clock. The words, the waits with a limit, the checks of arguments and the error codes that threads and tasks share are
  in `rtl/flows.h`.
- The scheduler (`core/sched.c`, [concurrency.md](concurrency.md#scheduler)) and tasks (`rtl/tasks.c`,
  [concurrency.md](concurrency.md#tasks)). Workers are program threads started when tasks first need them, `target` of
  them active at once (one per processor, or `//WORKERS:n`), up to 1024 with those that took the place of blocked ones.
  Each has a ring of 256 tasks it fills at the tail and takes from at the head (slots read and written atomically; a
  thief moves half of it with one compare-and-swap of the head) and a slot for the task to run next; a thread outside
  the pool that submits tasks has a ring of its own (a feeder: `fed`, made or reused at its first submission, up to 1024
  at once, then the pool's queue), filled without a lock and stolen from as the workers' are; when the thread ends
  (`xb_sched_thread_end`) the feeder, with what is left in it, serves the next thread, and the program's end cancels
  what is left in the feeders; the pool's queue (under the pool's lock) takes the half a full ring sheds, and a worker
  takes from it first once every 61 tasks, with its share of what waits. An idle worker parks on a word of its own,
  listed idle under the pool's lock after one more look at the count of queued tasks (a submission counts its task
  before it looks for an idle worker: one of the two sees the other). Before it parks, a worker that found nothing
  searches up to 50 µs while another worker runs (one searcher at a time, `searching`; it looks at the count of queued
  tasks, and searches the queues every 16 looks while it is not 0), and a submission wakes no worker while one searches
  (a searcher that finds work wakes another while work waits; a worker woken from its park does not: measured, those
  wakes cost more than they found); a thief takes another's next task only once it has seen that worker run the same
  task for 5 µs (its count of tasks, `runs`, unchanged; or it is away), and the monitor sees a task, or a chain, run
  long by the same counts unchanged between its looks: running a task reads no clock. `xb_blocking_run` and
  `xb_sleeping_run` tell the scheduler when a worker running a task enters and leaves an operation that may block
  (`xb_sched_blocking`): one that waits (`xb_sleeping_run`, `XB_BLOCK_WAITS`) is not active meanwhile, and when work
  waits another worker is woken or started; one that usually returns at once (`xb_blocking_run`, `XB_BLOCK_MAY`:
  `RUN_CALL`) stays active, and the monitor gives its place away when it finds it in the same operation 50 µs or more
  after it first saw it there ([concurrency.md](concurrency.md#scheduler)); one beyond the target that stays idle 2
  seconds ends. `xb_waiting_run` is the same for the collector but not for the scheduler: a wait for a lock of the
  runtime's held a moment (a stripe, `xb_object_lock_short` for a channel's) keeps the worker's place. The queues hold
  object addresses, which the collector marks (`xb_sched_mark`). A task is a `Task` object, shared: a state word
  (created, scheduled, running, then completed, failed or canceled), the result or the failure's value, the block and
  its arguments and the snapshot of its creator's environment until it runs, and the continuations, nodes `{target,
  next}` pushed under the task's stripe lock (compared with the head read before, again when another came first;
  finished combinators at the head are left out of the new node), which completing takes under the same lock with the
  final state; then each fires, from a stack of frames `{list, antecedent, below}` (local arrays) rather than by
  recursion, so a chain of combinators runs in constant stack: a continuation is scheduled (to run next on the
  completing worker; on the completing thread's feeder when another thread completes it), a combinator counts
  (`TaskWhenAll`) or completes once (`TaskWhenAny`) and pushes its own continuations. A delay, a promise, a timeout, a
  waiter and a yield are smaller objects of the same class's name and methods (`task_small_class`: 8 variables, without
  the block, the environment, the frame, the tasks waited for and an asynchronous function's resume point, which they
  never have; 14 for the others). A combinator waits for the tasks of a snapshot of its array (`xb_array_snapshot`),
  checked as one read saw it. Running a task is claiming it (scheduled to running, a compare-and-swap) by a worker or by
  a thread that waits for it with no limit, which runs it inline: a queue may hold a task that runs or ran, which a
  worker then skips. The task (a block's: an asynchronous function's steps are below) runs in an environment made from
  its snapshot, without the `Type()` trap of a waiter that runs it inline, with `ctx->task_depth` counting
  (`__TaskFail()`, which the default error handler calls: in a task it fails it with the error, through
  `XB_PENDING_FAIL`, instead of ending the program, and an Error object keeps the lines of where it was raised, a hidden
  variable that `__ErrorCalls()` reads for the report of whoever it reaches at last: the default handler shows them,
  then its own calls from `Waited for in`); a `BREAK` that leaves its block fails it with the `BREAK`'s value; a `QUIT`
  ends the program and leaves it canceled. The task's tables are closed as its environment goes (`xb_env_close_areas`,
  which sets the requests and the `BREAK` value aside meanwhile and keeps a `QUIT` that comes); a `ThreadStart` thread's
  when it ends, the program's at its end. A failure that carries an Error object is raised again for whoever takes the
  result, through its error block (`xb_error_raise`); another value as `Break( value )`. At the program's end the
  workers stop with the other threads, and the tasks that never started are canceled (`xb_sched_end`). A worker's `run`
  word (idle, running, blocked, long; changed with compare-and-swap so that each absence counts and is repaid once)
  tells whether it counts as active: not while it blocks or, seen by the monitor, runs one task longer than 20 ms while
  work waits. The monitor hands over at most one place per look, only when no task ended since its last look, and keeps
  no more workers long than the target; while work waits it fills again the places that a thread which did not start
  left empty (`fill_place`). A new worker starts at once while fewer than twice the target are alive, else when
  `may_spawn_locked` allows it (paced: `SPAWN_STEP_NS` more for each `target` workers past twice the target, at most
  `SPAWN_MAX_NS`; the monitor tries again at each look while work waits), up to the most workers (`workersMax`: 1024,
  fewer when their stacks would take more than a quarter of the address space the process may map,
  `xb_platform_address_space`). When no task ended for 2 seconds while tasks wait, and then no worker is active (each
  waits or runs long, or none could start), it tells the standard error once (`starved`, until tasks end again).
  `TaskWorkers()` gives `{ alive, target, most }` (`xb_sched_workers`). Timers are binary heaps of `{deadline, target}`,
  16 shards each under a word lock (a thread adds to its own: `timerShardOf`), each publishing its earliest deadline
  (`top`), which one program thread serves (the earliest of all first) (started with the pool; with nothing queued it
  sleeps until its next timer, or with no limit when there is none, and a submission that finds every worker busy wakes
  it, whatever timers wait): it fires each due target without the lock (`xb_task_timer`: a delay completes, a
  cancellation source's signal is set, a timeout fails with `Task timed out`, an Error object made with
  `xb_error_object`), and is the monitor. The heap drops the timers whose targets need them no more
  (`xb_task_timer_dead`: settled, or replaced by a later `CancelAfter`, whose deadline the signal keeps) once it
  doubled, or once the timers whose targets ended before their deadlines (`xb_timer_dropped`, told by the target's end:
  the deadline is in its `V_TIMER`) are half of it, looked at every 64 such ends; at the program's end the timers left
  fire as ending (a delay and a timeout canceled, a source canceled). Cancellation: a source and its token share a
  signal, a task that completes when the source is canceled, whose continuations are what waits for it (a delay with
  that token, a linked source's signal, `Register`'s tasks). A list of continuations drops, at its head, the targets
  that need it no more (ended, or the signal of a disposed source), and is pruned whole once as many were pushed as it
  kept at its last pruning. A signal that had 64 continuations pushed gets 16 partitions, signals of its own pushed onto
  its list (it fires them when it is canceled, as a linked source's), made at once and never changed after; from then on
  each thread pushes into one of them (a thread-local index), so tasks on many workers that wait with one token at once
  (a server's shutdown token) do not all take its lock: 16 consumers receiving 50000 values each with one token take 63
  ms and 0.85 s of processor time, as with a token each (all pushing onto its own list: 300 ms and 3.5 s). Channels
  (`rtl/channels.c`, their rings and the claims of their waiters in `rtl/waiters.c`, shared with `Mutex` and
  `Semaphore`): see [concurrency.md](concurrency.md#channels); `FOR EACH` reaches the enumerator of a runtime class's
  object (`GetEnumerator` in its method table): a channel's is a `ChannelEnumerator`, whose `MoveNext` takes the next
  value (waiting for it) into `Current`, and whose `MoveNextAsync` waits as a receiver whose entry in the ring is the
  pair `{task, enumerator}`: the value delivered goes into the enumerator's `Current` under the channel's lock and the
  task completes with `.T.`; `Close` completes such tasks with `.F.` instead of failing them. The enumerator keeps the
  token `GetEnumerator` was given and the task of its `MoveNextAsync` that waits (read and set under the channel's lock:
  a second one while it waits raises 9035). Their error codes are xbase's own: 9016 to 9031, and 9035.
- Platform layer (`platform/platform.h`, implemented in `posix/` and `win32/`, which needs Windows 8 or later and says
  so to the system's headers, `_WIN32_WINNT`, whatever version a toolchain assumes): threads (`xb_thread_start` with a
  reserved stack size, rounded up to the system's page and minimum; `xb_thread_join`; `xb_thread_detach`, the thread and
  its starter each letting go of its record, the last one freeing it; `xb_platform_threads_wait`, which the program's
  `main` calls last: on Windows it waits until every detached thread has ended at the system's level (their handles
  kept, those that ended closed when the list fills), since a thread that ends while the process exits can give the
  process its own exit code instead of the program's (seen under Wine about once in two hundred ends); POSIX ends every
  thread with the process and keeps the code, so it waits for nothing; `xb_thread_yield`, a waiter's back-off), mutexes
  (`xb_mutex`, with `trylock`), condition variables (`xb_cond`, with waits timed on the monotonic clock: POSIX binds the
  condition to it, macOS waits for a relative time, Windows in whole milliseconds rounded up), reader-writer locks
  (`xb_rwlock`; on glibc of the kind where a waiting writer goes before readers that come after it, elsewhere with the
  system's policy), the number of processors the program may run on (its affinity on Linux, else every processor
  online), the bounds of the calling thread's stack (`xb_platform_stack_bounds`), a monotonic clock in nanoseconds
  (`xb_platform_monotonic_ns`), which `ElapsedMs` counts from the program's first context (`xb_program_elapsed_ms`),
  pages for the collector's heap (`xb_platform_pages_map` aligned by mapping the size and the alignment and giving back
  the parts around, `VirtualAlloc` at an aligned address found by a reservation on Windows; `_unmap`; `_decommit`:
  `MADV_DONTNEED` on Linux, `MADV_FREE_REUSABLE` on macOS, `posix_madvise` elsewhere, `MEM_DECOMMIT` on Windows;
  `_recommit`, which only Windows and macOS need; `xb_platform_page_size`), and the process's resident memory
  (`xb_platform_resident_bytes`: `/proc/self/statm` on Linux, the task's basic information on macOS, the working set on
  Windows, 0 elsewhere), which `__GcMemory()` reports with the live objects and the heap's memory, and the address space
  the system lets the process map (`xb_platform_address_space`: the soft `RLIMIT_AS` on POSIX systems, the process's
  virtual space on Windows, `SIZE_MAX` when none is told), which bounds the scheduler's workers. With glibc,
  `xb_platform_init` keeps the C allocator to 4 arenas: each would reserve 64 MB of address space for a thread that
  allocates, and the runtime's objects live in the collector's heap. The functions that may block (files, directories)
  are the operating system's side (`xb_os_*`, `platform/os.h`) wrapped once for every platform through `xb_blocking_run`
  (`platform/blocking.c`, the arguments in a structure), which the collector provides ([Garbage
  collector](#garbage-collector)); so is the flush of a C stream (`xb_platform_flush`). Each object is made by its
  `init` function, which fails only for lack of memory or system objects, and freed by its `destroy` function. None of
  these locks is recursive: the thread holding one never asks for it again, not even for a second read lock, which would
  wait behind a waiting writer.
- Atomic operations (`platform/atomic.h`) on ordinary 8-bit, 32-bit, 64-bit, `size_t` and pointer fields, so that
  memory read plainly where no other thread sees it (an object header, a field compiled code reads) is also changed
  atomically where one may: loads, stores, add, or, and, exchange and compare-and-swap with an explicit order (not
  every one for every size). A 64-bit field used atomically is aligned to 8 bytes (`XB_ALIGN8` in `xbrt.h`), which
  32-bit targets do not do by themselves. GCC and
  Clang use their `__atomic` builtins (with libatomic on a target that lacks native atomics of some size, found when
  the build is configured); MSVC builds the runtime with `/volatile:ms` (volatile loads acquire, stores release, on
  every architecture; a build with `/volatile:iso` stops) and its `Interlocked` intrinsics.
- On top of them, the same everywhere (`core/sync.c`): the owners of locks, 64-bit numbers from one counter that starts
  at 1 and never comes back to a number it gave (the owner word 8-aligned, read and written atomically):
  `xb_thread_self` (the thread's, for the runtime's own locks, which no program code runs under) and `xb_flow_self` (the
  program's locks: the innermost flow entered on the thread, a task's or an asynchronous function's step, records on the
  C stack chained by `xb_flow_enter`/`xb_flow_leave` and numbered when one first takes a lock, a function's number kept
  in its task across its steps so a lock held across `AWAIT` stays its function's; else the thread's); `xb_flow_outer`
  tells whether a lock's holder is a flow the running one runs inside on the thread (a `Mutex` wait for it raises
  `BASE/9031` instead of never ending), `xb_once` (a function run once whatever threads ask at the same time, the others
  waiting until it returns), a recursive lock over three words (`xb_rwords`: the state, 0 free, 1 held, 2 held and maybe
  waited for, changed by sequentially consistent read-modify-writes; the holder's number; the depth: the holder takes it
  again, and holds it until it has released it as often; a release wakes a waiter only when the state was 2), which
  `xb_rlock` keeps together (made statically, for rare nesting work such as preparing classes) and a `Mutex` object in
  its variables, and a parking lot: `xb_park_wait` sleeps while a 32-bit value at an address is the one expected (with
  an optional limit in nanoseconds, which its own deadline decides), `xb_park_wake` wakes the waiter of the address that
  came first, or every one. Waiters queue in arrival order in one of 256 buckets chosen by the address (each made when
  first used) and sleep on a word of their own through the system's wait on an address (`xb_platform_addr_wait`: Linux's
  futex, Windows' `WaitOnAddress`; `XB_PLATFORM_ADDR_WAIT`), or else on a condition variable of their own (no system
  object made per wait where the first is), so a wake reaches only the waiters it marks; the waker marks and wakes a
  waiter under the bucket's lock, which the waiter takes again before it leaves. A waiter counts itself in its bucket
  before it tests the value under the bucket's lock; the waker, after changing the value and a full fence, reads the
  count: none waits, it takes no lock (completing a task nobody waits for costs a load); else it takes the lock, so no
  wake is lost. The lock of a `Mutex` or a `Channel` looks at the state a few times before its waiter sleeps, and so
  does a wait for a task (short work on the other side ends sooner than a sleep). `xb_park_wait_blocking` waits so in a
  blocking operation. The output lock (`xb_output_lock`) serializes the program's writes to the standard streams.
- Shared objects (`core/shared.h`, [concurrency.md](concurrency.md#shared-objects); the runtime's operations on arrays,
  hashes, objects, cells and references use them, and compiled code tests the bit): `XB_GC_SHARED` in an object's header
  (`XB_GC_STATE_SHARED`, kept by every collection) marks one that more than one flow can reach. `xb_share` marks what an
  item refers to and all it reaches (an explicit stack, stopping at what is shared already, literal strings left alone,
  a growing string no longer growing in place); it allocates nothing (its list of objects to visit starts on the stack).
  A stripe, one of 1024 chosen by an object's address, each on a cache line of its own, is one word: the lock's bits
  below a sequence count that each release moves, so that taking and releasing it is a read-modify-write each. A shared
  object's items are read while the lock is not held (`xb_seq_read_begin`, the items' words loaded with acquire order,
  `xb_seq_read_valid`: the word unchanged; `xb_seq_get` for one item), and written under the lock (`xb_seq_lock`, a load
  before each compare-and-swap of its spin, the words stored with release order, `xb_seq_unlock`; `xb_seq_put`), with no
  fences, so the thread sanitizer follows them. A waiter spins, then yields (a reader), or sleeps on the word through
  `xb_waiting_run` (a writer); a writer never reaches a safepoint while it holds a stripe, so a reader that waits never
  holds up a collection. A writer that has waited a millisecond marks the lock starving, and its release then hands the
  lock to a thread that slept waiting instead of freeing it: a thread that takes the lock again and again (long
  operations such as `AIns` on a large array) cannot keep a waiting one out.
- Arrays (`types/array.c`): `xb_array_length`, `xb_array_get` and `xb_array_put` read and write an element whole (under
  the array's sequence counter or lock when it is shared, in place when it is local; a read a writer overlapped never
  reaches its destination; a value stored into a shared array is shared first), and the index operators, `Len`, `Empty`,
  `AScan`'s search and the count of `FOR EACH` use them; `xb_array_last` (`ATail`) reads the length and the last element
  in one read, and `xb_array_snapshot` (`AClone`, `ASort`) copies all the elements as one read saw them (under the lock
  after four reads that writers overtook, so a long copy ends while threads write without a pause);
  `xb_array_append` (`AAdd`) adds an element at once whatever other threads add, `xb_array_insert` and `xb_array_delete`
  (`AIns`, `ADel`) shift under the lock, and `xb_array_resize_any` (`ASize`) changes the length. A shared array's `len`,
  `cap` and `items` change under its lock with release stores, and storage it grows into is allocated before the lock is
  taken (rooted meanwhile; a capacity only grows, so it is still enough under the lock); `xb_array_resize` is for arrays
  only the runtime reaches yet. `ASort` of a shared array is one operation among other threads' writes: it sorts a copy
  (a block may run any code) and puts the sorted range back under the lock only when the array still holds what the
  copy was made of (the same range, the same elements), else it starts again from a new copy, so no value another
  thread stores meanwhile is lost; without a block it sorts the array itself under the lock after two such conflicts
  (no code runs, nothing is allocated), so it always ends. A local array that its block shares meanwhile is sorted
  again so; its block reads elements whole.
- Hashes (`types/hash.c`): every operation on a shared hash runs under its lock, reads too (a read may reorganize the
  hash: the position tree, a compaction), and keys and values come out copied: `xb_hash_length`, `xb_hash_get`,
  `xb_hash_lookup` (with the key as stored), `xb_hash_position`, `xb_hash_entry` (by position), `xb_hash_value_at`,
  `xb_hash_replace` (an existing key's value), `xb_hash_remove`, `xb_hash_remove_at`, `xb_hash_empty`, `xb_hash_fill`,
  `xb_hash_case_match`; `xb_hash_put` and `xb_hash_reserve_any` take a larger store, allocated before the lock (rooted
  meanwhile), under it when still needed; `xb_hash_snapshot` and `xb_hash_copy_any` copy the entries as one locked read
  saw them. The index operators, `$`, `Len`, `Empty`, the `H*` functions, `AScan`'s search, `AClone` and the count and
  keys of `FOR EACH` use them (a local hash is still read in place where nothing else could reach it). `HSort` of a
  shared hash sorts a copy and puts the order back under the lock (`xb_hash_take_order`): each key found as the hash is
  then, once, with its value then, else the hash stays as it is; a local hash whose block shares it meanwhile is sorted
  again so.
- Objects, cells and references: `xb_object_get` and `xb_object_put` (an object's variable: the messages that read and
  assign them, compiled classes' and the runtime's), `xb_cell_get` and `xb_cell_put` (a codeblock's capture, a memory
  variable), inline for a local one (one header load: sharing and the write barrier's test), under the counter or the
  lock for a shared one; `xb_ref_get` and `xb_ref_set` read and write what a reference designates whole, through them
  and through the array and hash functions (an element gone is the bound error, then the reference's own item), for
  memory variables, macros' captures and variables, `PValue`, `FRead`, `FOR EACH` and the arguments `...` passes on. A
  `FOR EACH` variable in a cell (one a codeblock captured) is read and written through the cell, and its loop's
  reference keeps its index in one word and its own item (a string's character, an enumerator's `Current`) under its
  counter or lock (`xb_ref_index`, `xb_ref_set_index`, `xb_ref_own_get`, `xb_ref_own_put`).
- Compiled code tests the header bit before reading or writing in place (an array's element, its length for `Len()`,
  a hash's key in `FOR EACH`, a cell's value, a loop's reference) and calls the runtime for a shared object
  (`xb_op_index`, `xb_ref_load`, ...; [compiler.md](compiler.md#xir-to-llvm)).

## Program startup

Before `main`, the global constructor of every module calls `xb_module_register` (`core/module.c`) with the
initialization of its `STATIC`s (which become a GC root), its `INIT` and `EXIT` procedures, its public functions by
name, and the names of its memory variables ([Memory variables](#memory-variables)). These go into the symbol table
(`core/symbols.c`): upper-cased names numbered by a table of names (`core/names.c`, the one memory variables use too;
lookups by a name in any letter case upper-case it first, `xb_names_find_upper`) and what each designates, which the
runtime's own functions join when the program starts (`xb_program_run`), from a table generated from `include/rtl.ch` at
build time (`runtime/symbols.cmake`, with which parameters take references, read from the declarations by
`runtime/rtl_declarations.cmake`); a program function hides a runtime function of its name, and of the functions of one
name that modules register, the one of the highest level stays (`xb_module_register`'s level, `XB_LEVEL_PROGRAM`: a
module of the program over one of a library, [projects.md](projects.md); the first registered among equals); classes
likewise (`xb_classes_register`). The table refers to every runtime function, so every program links them all in. `Do(
cName | bBlock, [args] )` calls a function found there (letter case aside; spaces are part of the name, which ends at a
NUL), a codeblock or a function reference ([Function references](#function-references)), with its arguments: a reference
goes on as passed to a compiled function and to a runtime function's parameter declared `@`, other runtime parameters
receive the value. Do shows as `DO` on the call stack, and ends the PRIVATEs that a runtime function creates for its
caller (`RESTORE`), as a compiled caller does; an unknown name is `BASE/1001` `Undefined function` (not substitutable),
anything else as its first argument `BASE/3012`.

`main` (in `runtime/src/core/main.c`) runs the platform setup (`xb_platform_init`: on Windows, standard output and error
in binary mode so programs write the same bytes everywhere) and takes the command-line arguments in UTF-8
(`xb_platform_args`: as `main` received them on POSIX systems, converted from the wide command line on Windows). Then,
on the program's thread (started with `xb_thread_start` and a stack reserved at `XB_PROGRAM_STACK_SIZE`; the process's
first thread when it cannot be started), attached to the collector, it creates the context (the first, which starts the
`ElapsedMs` clock), makes the arguments after the program's name strings in a frame of its own linked into the context's
chain (so the collector sees them; an argument that starts with `//` is a runtime option, which the program does not
receive: `//WORKERS:n` sets the workers that run tasks at once, letters in any case, and any other is ignored, as in the
reference), calls `xb_app_main( ctx, argc, argv )` (emitted by the compiler) with them, flushes output, detaches and
returns the exit code. `xb_app_main` checks the ABI and calls `xb_program_run`, which runs, in this
order:

1. the modules' `STATIC` initializations, from the lowest level up (a library before the ones that use it, the
   program's modules last), in registration order within a level;
2. `ErrorSys()`, found by name (the program's own, else the runtime's), which installs the error block
   ([Errors and pending requests](#errors-and-pending-requests));
3. the `INIT` procedures, module by module in that same order and, within a module, from the last one written to the
   first (as the reference does), each with the command-line arguments;
4. the start function, with the arguments, as Clipper passes them: strings, `NIL` for a parameter without one, and
   arguments past the parameters passed too; when it returns a `Task` (as an asynchronous start procedure will), the
   program waits for it, and a failure or a cancellation is raised as `Result` raises it (`xb_task_await_start`: the
   default error handler reports an error and ends the program with exit code 1); then the threads and the workers
   that still run are asked to `QUIT` and waited for, and the tasks that never started are canceled
   (`xb_threads_end`);
5. the `EXIT` procedures, module by module from the highest level down (the program's modules first, then the
   libraries they use), in registration order within a level, each module's in the order written, without arguments,
   the threads and tasks they leave running ended the same way after them.

A request to unwind (a `QUIT`, a `BREAK` with no sequence, an error that ended the program) stops steps 1 to 4; the
request is then dropped (with a `BREAK` value) and step 5 runs, a new request stopping it. Then every table open is
written and closed ([Work areas](#work-areas)), whatever ended the program. The exit code is the process's (any thread's
code sets the same one) and kept throughout: `ErrorLevel( [nNew] )` returns it and sets it from an argument, a number
wrapped to 32 bits, anything else 0 (the default error handler sets 1). Registration order follows the global
constructors: the link order on ELF systems, the reverse on MinGW; the levels keep the libraries' modules before the
program's whatever it is. Inside an `EXIT` procedure the call stack starts with it (the reference shows the function
that ended the program, such as `__QUIT`, under it).
