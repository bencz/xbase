# Runtime (`xbrt`)

Code: `runtime/include/xbrt.h` (the contract with the compiler), `runtime/src/`.

`xbrt.h` is the ABI shared with `xbc`. The compiler includes it for constants only (type tags, ABI version, flags,
shared formulas) and mirrors the structures as LLVM types laid out by the target `DataLayout`; it never relies on the
host's layout. Any change to a structure or an entry point used by generated code updates `CodeGen` and bumps
`XB_ABI_VERSION`.

## Values

```c
typedef struct xb_item {
  uint32_t type;        /* xb_type */
  uint16_t width, dec;  /* numeric display width of the integer part, decimals */
  union { int32_t l; int64_t i; double d; xb_str *s; void *p; } v;
} xb_item;
```

- Types: `NIL`, `LOGICAL`, `INT` (64-bit), `DOUBLE`, `STRING`, `ARRAY`, `BLOCK`, `OBJECT`, `HASH`, `DATE`, and two kinds
  the language never shows, which designate another item (see [References](#references)): `CELL`, a variable in a cell,
  and `REF`, an element. An all-zero item is `NIL`.
- Copying an item is a plain memory copy; there is no reference counting. Items are written as two 8-byte words, the
  header and the value (`xb_put` with `xb_header_word` and the value word in `xbrt.h`), by the runtime's setters
  (`xb_set_int`, ...) and by compiled code alike, never field by field: compiled code reads an item by words, and a
  load is served by the store that wrote it only when that store covers it. The value bytes a member does not use are
  zero.
- The heap types (`STRING` to `HASH`) are one range of tags (`XB_T_HEAP_FIRST`, `XB_T_HEAP_LAST`, `xb_type_is_heap`):
  an item of another type refers to no object, so the collector need not see it.
- Strings (`xb_str`: flags, hash, length, bytes) are immutable and NUL-terminated, so they can be shared between items
  and threads and passed to C. Literals emitted by the compiler carry `XB_STR_STATIC` and are not heap objects, nor are
  the empty string and the 256 strings of one byte (`xb_str_bytes` in `xbrt.h`): `xb_str_new` and `xb_str_concat` return
  those for results of up to one byte (`Chr()`, `SubStr( c, n, 1 )`, ...), so making one allocates nothing, and compiled
  code takes them from the table too. One exception to immutability is invisible to programs: `xb_op_append( ctx, var, b
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
- Arrays (`xb_array` in `xbrt.h`: `len`, `cap`, `items`; `types/array.c`, functions in `rtl/arrays.c`) are 1-based.
  Their items start in the array's own allocation and move to a separate heap object when the array grows past it.
  Compiled code reads `len` and `items` to access elements in place.
- Hashes (`xb_hash` in `types/types.h`) map keys to values in insertion order; see [Hashes](#hashes).
- Codeblocks (`xb_block`: function, captured cells) and cells (`xb_cell`: one item at offset 0) are described in
  [Codeblocks](#codeblocks), objects in [Objects and messages](#objects-and-messages).

## Frames

Parameters, `LOCAL`s and temporaries of a compiled function live in its item frame (`xb_frame`: link, what the call
stack shows, slot count, current line, the arguments as received, items), an `alloca` on the machine stack. The function
links it into its thread context's chain (`xb_ctx.frames`) at entry, with the parameters copied into the first slots
(from `argv`, `NIL` past `argc`, or from registers in a direct entry, [compiler.md](compiler.md#xir-to-llvm)) and the
other slots `NIL`, and unlinks it at exit; every exit goes through one block, and errors return normally (the pending
flag), so the chain is always right. The collector scans the frames of every context's chain. A frame dies with its
function: nothing keeps a pointer to a slot (a variable that outlives it moves into a heap cell first). The program runs
on a thread whose stack is reserved at 1 GB on 64-bit targets and 256 MB on 32-bit ones (`XB_PROGRAM_STACK_SIZE` in
`core/main.c`; pages are committed as used), so deep recursion has room.

Running out of it is an error, not the end of the process: the context knows the lowest address of its thread's
stack the program may use (`xb_ctx_set_stack`, from `xb_platform_stack_bounds`: the system's bounds of the thread's
stack, above its guard pages; none on a system that does not tell, where the error is never raised), and its
`stack_limit` lies 1 MB above it. Every place where xBase code goes deeper tests it: a compiled function (a
codeblock's too) compares its frame's address with the limit after linking the frame, and below it, before its body
runs, calls `xb_stack_overflow` and returns through its exit, unwinding; the macro evaluator does the same for each
evaluation (a codeblock made from text), and a list of arguments forwarded with `...` is tested before it is written.
That raises `EG_LIMIT` (`Stack overflow`, the function's name as the operation; it cannot be substituted), whose
handler runs in the reserve, the limit lowered to 64 KB above the lowest address meanwhile: a `BEGIN SEQUENCE` or
`TRY` recovers from it and the program goes on, the default handler reports it and ends the program; a handler that
runs out of the reserve too ends the process with `xbrt: stack overflow` (the reference's program crashes). Pages of
the stack used once stay committed. The test costs a load, a comparison and a branch per call: on the `fib`
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
it: `SUBSTR`, `ERROR:FOO` for a message to an object, `FOO` without an object, `(b)EVAL` for `Eval` of a non-block).
Compiled code links a frame without slots too, over the function's own, while the body of a function or codeblock it
runs in place (inlined, [xir.md](xir.md#passes)) runs: the call stack shows the callee as its call would.
Operators and statements show nothing of their own. `ProcName( [nLevel] )`, `ProcLine( [nLevel] )` and
`ProcFile( [nLevel] )` read it: level 0 is the function calling them, a non-numeric level is 0, a negative one gives
`PROCNAME`, line 0 and `""`, a level past the end `""`, 0 and `""`. `ProcFile` gives the name of the function's source
file (`xb_proc.file`; a codeblock's is its function's), without the directories: the reference keeps the path given to
the compiler, which would put build directories into programs; a runtime function, in C or in xBase, has none (`""`).

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
arrays, array item storage, hashes, hash entry stores, codeblocks, cells, reference targets and objects (instances of
classes) are its objects; an 8-byte header (size, kind, state, flags) precedes each one, so object pointers are what the
rest of the runtime and compiled code use.

- Heap: objects up to 256 bytes (header included) live in blocks of 32 KB, aligned to their size and divided into
  lines of 128 bytes. A block's first lines hold its metadata: per line, the epoch of the last collection that reached
  an object covering it and whether an old object covers it, and a bit per 8 bytes where an object starts
  (`xb_hblock`). Larger objects, up to the largest size class (four slots to a block, close to 8 KB), live in blocks of
  one size class each: the classes are computed at start-up, each the largest slot that fits its number of slots in a
  block, and each at least an eighth larger than the one before. A class's block takes its objects' slots from a free
  list, and a dead object's slot serves the next object of its class, where holes of many lines would fragment the
  heap; a class's block left empty becomes a free block of lines. Blocks come from chunks of 32 blocks obtained with
  `malloc` and aligned by hand. Larger objects are allocated individually (`XB_GC_HDR_LARGE` in their header).
- Allocation: every thread fills a hole, a run of free lines, by bumping a cursor, without locking (and takes the slots
  of a size class from a block of its own); when the object
  does not fit, it takes the block's next hole, then a recyclable block (free lines among live ones), then a free
  block. An object above one line that does not fit the hole goes to an overflow run of the thread instead (holes
  large enough for it, from the same recyclable blocks, then free ones; a recyclable block without one leaves the list
  until the next sweep), so no hole is given up for it and the free lines of recyclable blocks serve objects of every
  size.
- Roots: the frames and temporary C roots (`xb_gc_root_push`, a vector of the context that grows as C calls nest;
  also compiled code's, while it makes a codeblock's cells) of every registered context, and global item arrays
  registered with `xb_gc_add_root` (the modules' `STATIC`s).
- Generations: marks are sticky. An object marked by a collection stays marked and is *old*; new objects are young.
  A young collection marks from the roots and the remembered set, stops at old objects instead of traversing them,
  and frees only the young objects it did not reach. A full collection clears every mark and traces the whole heap;
  it runs when the old generation reaches twice its size after the previous full one (at least 8 MB).
- Aging: a young object reached by a young collection ages, and is promoted by the next one that reaches it, so
  objects that die soon after a collection (a table whose entries are replaced) die young. An object that is old
  after a young collection and still refers to a young one stays in the remembered set (an array keeps the card
  dirty) until that one is promoted or dies. Aging adapts: it turns off when the bytes promoted at a young collection
  exceed four times the aged bytes that died (aging then only doubles their tracing; reached objects are promoted at
  once), and back on when a full collection finds that more than half of the old bytes had died.
- Young budget: a young collection runs when the allocation since the last collection reaches the budget, at least
  4 MB. The budget doubles after a collection where more than half of the allocation survived (collecting such
  objects early only promotes them) and halves when less than an eighth did; it never exceeds the size of the old
  generation.
- Write barrier: storing a pointer into an old object must record that object in the remembered set, because a young
  collection does not scan old objects. The state byte sits `XB_GC_STATE_BACK` bytes before the object; its bits
  `XB_GC_STATE_OLD` and `XB_GC_STATE_RECORDED` tell whether a store must record it (`xb_gc_write` in `xbrt.h`). C code
  calls `xb_gc_write(obj)` after storing into a heap object (a grown array's item storage, a reference's character) and
  `xb_gc_write_item(array, index)` after storing an array element (it returns at once for a value of a type that refers
  to no object, `xb_type_is_heap`; compiled code that stores an element in place calls it only for such values);
  compiled code tests the byte inline after storing into a captured variable's cell and calls `xb_gc_remember`, and
  calls `xb_ref_written` after a store through a reference (a `FOR EACH` variable, a parameter passed by reference). The
  barrier comes after the store, with no allocation in between, and an item a call produces never goes straight into a
  heap object: it goes through a frame slot first (a root). A recorded object is traced by the next young collection and
  leaves the set. Threads record without a lock: the thread that sets an object's bit first (atomically) lists it in a
  list of its own, which a collection merges into the remembered set.
- Card marking: array storage for 1024 items or more carries one card byte per 64 items after its last item. A store
  into such an old array marks the element's card and records the array for its cards only; the young collection
  scans the marked cards (eight card bytes per step) instead of the whole array. Storage that is replaced (the array
  grew) records the whole array.
- Collection: marking uses an explicit stack. Marking an object gives the lines it covers the collection's epoch, and
  making it old (a full collection, a promotion) makes them old lines. A line is then free when it is not old and no
  object reached by this collection covers it: sweeping reads a block's line marks, never its objects, and makes the
  lists of free and recyclable blocks. A young collection sweeps only the blocks that received objects or hold aged
  ones since their last sweep; old lines stay until a full collection, which clears every line and walks the objects
  through the start bits to clear their marks. When the epoch (8 bits) wraps, every line mark is cleared. The blocks
  of size classes are swept slot by slot (their dead objects' slots return to the free list). Large objects are freed
  at once. Memory is not returned to the operating system.
- Evacuation: dead objects spread over many blocks leave holes that objects of several lines may not fit, and the
  heap would grow instead. A full collection defragments when it can: it picks the emptiest blocks of lines (at most
  half their lines live), as many as the free blocks and a reserve of new ones (a sixteenth of the heap's blocks) can
  take, and moves every object it reaches in them into free blocks. The old place keeps the address of the copy
  (`XB_GC_HDR_MOVED`; the address is stored past a string's flags, which stay readable, since a string is tested for
  `XB_STR_STATIC` before its header: a literal has none), every reference the collection sees is set to the copy (roots,
  fields, an array's own items, a hash's tables inside its entry store, a codeblock's cells, the collector's own
  lists), and the emptied blocks become free. When the heap is about to grow (no free or recyclable block left, the
  last chunk used up), at least a young budget was allocated since the last full collection, and a quarter of the
  heap is free lines of blocks such a collection may empty, it runs first instead, on a thread that may move objects.
  Objects of size classes, large objects and objects too small to hold the copy's address (on 32-bit targets, a
  codeblock without cells or an object without variables) never move.
- Pinning: compiled code and C code keep object addresses in registers and on the machine stack, where the collector
  cannot update them. Before moving anything, the collector saves its registers on its stack
  (`xb_platform_with_registers`) and scans every thread's stack up to its end (the system's bounds of the stack,
  `xb_platform_stack_bounds`, else the address the thread attached with): its own from there, a parked thread's from
  where it parked or entered its blocking operation (its registers saved on the stack above that point); the slots of
  the compiled code's frames are left out (they are precise roots). An object of the blocks being emptied that a word
  points into (to its header, anywhere inside it or just past it) is pinned (`XB_GC_HDR_PINNED`) and stays. The object
  that a C root's address lies in is pinned too. A collection moves nothing when a thread's stack is unknown, when the
  collecting thread is not attached, while a thread is in a blocking operation it entered alone (below), or while C
  code holds the collector. A pointer below an object's start (such as
  `items - 1` for 1-based indexing) does not pin it: C code keeps the object's address, or one inside it, across a call
  that may allocate.
- Frame temporaries are not cleared after each statement, so a value a temporary held stays alive until its slot is
  reused or the function returns.
- Sizes that cannot be allocated (an array longer than `XB_ARRAY_MAX_LEN`, an object above half the address space,
  memory exhausted), a lock that cannot be created and an ABI mismatch
  end the program through `xb_gc_fatal`, the runtime's single fatal exit, with a message on standard error and exit
  code 1.
- Rules for C code: a collection can run inside any allocation, so every object still needed must be reachable from a
  frame or a C root before the next allocation, and a store into a heap object that may have aged since it was allocated
  (any allocation in between) is followed by `xb_gc_write` or `xb_gc_write_item`. An object address in a C variable
  stays valid across a collection (the stack scan pins the object), but an address kept anywhere the scan does not see
  (in `malloc`'d memory, as a map's key) is valid only between `xb_gc_hold()` and `xb_gc_release()`, during which no
  collection moves anything (`xb_clone_deep` keeps such a map). What may block the thread (a file's I/O, a wait, a write
  to or flush of the standard output or error) runs through `xb_blocking_run`: it allocates nothing and changes no
  reference, and the objects it reads or writes are rooted (`FRead()` roots the string a read fills, the console the
  text it writes), since another thread may collect meanwhile. A runtime lock held across a safepoint or a blocking
  operation (the registry of shared files', whose holder may park on its way back from a file operation; the lock of
  class preparations, whose holder runs xBase code) is waited for in a blocking operation; others (the heap's, the
  memory variable names') are never held across one.
- Threads: a thread that runs xBase code attaches (`xb_gc_thread_attach`, by its first function) and detaches before
  it ends; the allocation fast path takes no lock, the global structures one (`xb_mutex`). A collection stops the
  world: the collecting thread sets `stopping` and `XB_PENDING_SAFEPOINT` on the contexts of the other threads still
  running (each context knows the thread that registered it; a request served for nothing would send that thread's
  loops to their generic copy for an iteration, [compiler.md](compiler.md#object-emission)), then waits (on a
  condition) until no other attached thread is running. A running thread parks at its next safepoint (a poll of
  compiled code that serves the request, `xb_pending_gc`; the slow path of an allocation; a change of the contexts),
  its registers saved on its stack, until the collection ends. The request is set with release and read with acquire,
  and the collector raises it again each millisecond it waits, so a thread that served it before `stopping` reached
  it parks at its next poll. A thread in a blocking operation counts as stopped: the operation runs inside
  `xb_platform_with_registers`, so the registers of every caller stay saved on the stack while it runs (a thread that is
  the only one attached skips that, nothing else collecting then, and marks itself `unsaved`: a thread that attaches
  meanwhile and collects moves nothing until the operation ends); the thread
  publishes where its stack is in use and its state (sequentially consistent, as the collector's `stopping` is, so
  neither misses the other), and on its way back it parks if a collection runs. A second thread that wants to collect
  parks until the first one's collection ends, then sees whether its own is still needed. What holds a collection up
  is code that reaches no safepoint: a runtime loop of C alone (a sort without a block) for as long as it runs.

`__GcCollect( [lYoung] )` runs a full collection (a young one with `.T.`), then the destructors of the objects it
found unreachable ([Classes](#classes)), and returns the number of live objects the last full collection found (tests
use it).

Finalization: an object registered with `xb_gc_finalizable` (an object whose class has a destructor) that a
collection does not reach moves, after marking, to a queue of finalized objects, and is marked with what it refers to
(the queue is a root until `xb_gc_next_finalized` hands it out); the collection then raises `XB_PENDING_FINALIZE` in
the context that runs the destructors (the first one registered, then another when it goes).

## Hashes

`types/hash.c` holds the hash itself, `rtl/hashes.c` the `H*` functions (`HEval`, `HScan` and `HMerge` are in
`rtl/arrays.prg`), and `rtl/collections.c` what they share with the array functions: argument ranges, block calls
from C, the order of values, the stable merge sort and deep copies.

- Keys are strings, numbers and dates; values are anything. A number that equals an integer is that integer as a key,
  so `1`, `1.0` and `-0.0` against `0` are each one key, also in a literal (`{ 1 => "a", 1.0 => "b" }` has one entry,
  `"b"`); other doubles are equal when their bits are. Strings compare byte for byte, or with ASCII letter case aside
  when the hash ignores case. A date never equals a number.
- Layout: one store (`XB_GC_HSTORE`) holds the entries (key and value items) in insertion order, then their 32-bit key
  hashes, then an index of twice as many slots holding entry numbers plus one (0 is free), then a Fenwick tree with a
  counter per entry. Lookups probe the index linearly from the key hash; the index is at most half full. Deleting an
  entry frees its index slot by moving back the slots after it that probing would no longer reach (no tombstones) and
  leaves a hole (a `NIL` key) in the store. A full store whose holes are a quarter of it is compacted (the live
  entries move down in order); otherwise it doubles. A new hash has no store; the first holds 8 entries, the largest
  `XB_HASH_MAX_LEN` (2^30), and a larger one is the fatal error of a size that cannot be allocated.
- Positions (`xb_hash_at`, `xb_hash_pos`: `HKeyAt`, `HPos`, `FOR EACH`, ...) count live entries in order. Without
  holes position k is entry k. With holes the Fenwick tree, built over the entries when a position is first needed
  and updated by every insertion and deletion until the next compaction, finds the entry of a position, or the
  position of an entry, in O(log n); so deleting from the front, or while walking the hash, stays cheap. The functions
  that go over every entry (`HKeys`, `HValues`, `HFill`, `HSort`, `HScan` with a value, `HCopy`) compact first
  (`xb_hash_compact`), which keeps positions as they were.
- Key hashes: a string's is `xb_hash_bytes` (`xbrt.h`), which reads its bytes one at a time, so every target, little
  or big endian, 32 or 64 bits, computes the same value. The compiler stores it in the `hash` field of the strings it
  emits, and the runtime caches it there for heap strings (0 means not computed). A hash that ignores case hashes the
  upper-cased letters. Numbers and dates are scrambled by `xb_hash_mix64`.
- `HCaseMatch( h, .F. )` rebuilds the index ignoring case. Keys that then become equal merge into the first of them,
  which keeps its spelling and position and takes the value of the last, so a key always names one entry.
- Elements: `h[k]` reads the value; a missing key is `Bound error: array access` (1132), a value that cannot be a key
  `Argument error: array access` (1068, whose handler may substitute the value). `h[k] := v` replaces the value in place
  or adds the key last; a value that cannot be a key is `Argument error: array assign` (1069). A reference to `h[k]`
  (`@h[k]`) designates the entry of key `k` (`xb_ref.key`, the entry's own key as it is spelled): it finds it again at
  every use, wherever deletions move it, and using it once the key is gone is the bound error (1132). The `FOR EACH`
  reference designates a position; a codeblock or a reference that takes the loop's current element gets the entry's key
  instead. A literal `{ k => v, ... }` is built by `xb_hash_new` from frame slots; a value that cannot be a key raises
  `Bound error: array assign` (1133) with the hash built so far, the key and the value as arguments.
- The `H*` functions raise `Argument error` (1123) for a first argument that is not a hash or a key that cannot be one,
  and `Bound error` (1187, named after the function, `HDelAt` included, where the reference implementation gives 1133)
  for a position outside `1 .. Len( h )`. `Hash( k1, v1, ... )` with a key and no value is `Bound error: array
  dimension` (1131), and a value that cannot be a key the error of a literal with one (1133). `HCopy( hSource, hDest,
  [nStart], [nCount] )` takes the source first and returns `hDest`; `HMerge( hDest, hSource, [bFilter] )` takes the
  destination first. `HAllocate` returns the hash.
- Walking a hash that changes: every walk reads the entry at a position when its turn comes, so the entries after a
  deleted one move down a position. `FOR EACH` and `HMerge` check each position against the current length, so they
  reach the entries a block adds; `HEval` and `HScan` walk the range fixed when they start, and stop at a position
  that no longer exists. `HSort` sorts positions and puts the entries back only when its block left the number of
  entries alone (otherwise the hash stays as the block left it). No change made by a block leaves the hash
  inconsistent.
- Copies: `HClone` and `AClone` copy the arrays and hashes inside too (`xb_clone_deep`), each once, so shared and
  cyclic parts stay shared and cyclic in the copy; objects and codeblocks are shared. The copy works through a list
  of copies to visit, not recursion, so the depth of the structure has no limit. `HCopy` and `HMerge` copy entries
  only.
- Collector: a hash (`XB_GC_HASH`) is traced by marking its store and the keys and values of its entries; the store
  holds no pointers of its own to trace. C code calls `xb_gc_write(hash)` after storing into an entry.

## Codeblocks

A variable that a codeblock captures, or that is passed by reference, moves into a cell when that first happens: its
value moves into a new cell and an `XB_T_CELL` item is left in its slot or `STATIC` storage (a parameter passed by
reference already holds its caller's). `xb_cell_of` returns the cell a codeblock captures; for a `LOCAL` or
parameter that holds the reference of a `FOR EACH` loop it returns a new cell holding the current element, and the
variable stays the loop's. The function and every block
that captured it share the cell, which outlives the function. A codeblock is a compiled function
`(ctx, ret, argc, argv, self)` plus the captured cells (`self->cells[]`); `xb_block_new` creates it and the compiled
code stores the cells. `Eval( bBlock, ... )` and `xb_block_call` run it; the caller sets the result to `NIL` first.

## Objects and messages

An object (`xb_object` in `types/types.h`, `types/object.c`) is an instance of a class: its class and one item per
instance variable. The runtime defines classes of its own in C (`xb_class`: name and variables), and programs define
theirs in xBase ([Classes](#classes)). `xb_op_send(ctx, ret, message, argc, argv)` sends a message to `argv[0]` with
the arguments `argv[1 .. argc-1]`; the compiler passes the message upper-cased, and an assignment `o:x := v` sends
`_X` with `v`. Compiled code sends through `xb_op_send_from`, with the send's inline cache and the class of the method
it is in ([Classes](#classes)).

- An object's variables: `X` returns the variable (arguments are ignored), `_X` assigns it (without an argument, `NIL`).
  A variable's rule (`xb_var_rule`) says what it accepts: any value; only an array, a logical or a string (a value of
  another type is ignored); or a number, kept as an integer truncated toward zero and wrapped to 32 bits signed or 16
  bits unsigned (0 for a double beyond 64 bits), where a value of another type raises `BASE/0` `Argument error` with
  the variable's name as the operation. `_X` returns the value kept, except that an ignored array or logical gives
  `NIL`, a string variable returns its argument even when it ignores it, and a 16-bit variable returns the number
  wrapped to 32 bits only. That result is the value of `o:x := v` (see [compiler.md](compiler.md#syntax-tree-to-xir)).
  A new object holds its rules' empty values (`NIL`, `.F.`, `""`, 0).
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
`genCode`, `osCode`, `subCode` (32-bit numbers), and `severity` and `tries` (16-bit numbers).

### Enumerators

`FOR EACH x IN o`, for an object of a compiled class, goes through an enumerator (`core/control.c`): once before the
loop, `xb_each_source` takes `o:GetEnumerator()`, or `o` itself when it answers `MoveNext` (calling its `Reset()`
first when it has one); the enumerator answers `MoveNext()` (`.T.` while there is an element) and `Current` (a method,
an `ACCESS` or a variable). Each iteration's length test (`xb_enum_len`) sends `MoveNext()` (a length as large as can
be while it gives `.T.`, else 0), and `xb_each_move` puts `Current` in the loop reference's own item: the variable
reads it, and assigning the variable changes nothing in the collection. `k` in `FOR EACH k => x` is the position.
With `DESCEND` the enumerator's elements are gathered into an array first. An object without an enumerator is the
error of any other value that is no collection (`BASE/1068`). The paths of arrays, strings and hashes come first and
do not change.

### Generators

`types/generator.c`. A generator's enumerator is an object of the runtime class `GENERATOR`: `Current` (a variable no
message assigns, `XB_VAR_READ`) is the last element, `MoveNext()` runs one step (`xb_gen_move_next`), and its own
variables, whose names no message can spell, hold the state, the resume function (its address as an integer) and the
frame between steps (an array). `xb_gen_new` makes it with the function's arguments in frame slots 1 .. n (slot 0 is the
enumerator, the resume function's argument); at each step `xb_gen_restore` copies the saved slots into the resume
function's frame and returns the state, marking the enumerator ended until the step yields (a step that fails, or a
`MoveNext` inside the step, ends it); `xb_gen_yield` saves the frame (one write barrier per card, `xb_gc_write_items`),
the state and the element; `xb_gen_done` ends it and drops the frame, at the end of the body and after a step that fails
(`Current` is then NIL). `FOR EACH` calls the step and reads `Current` directly; its other messages are those of the
runtime's `Enumerable` (`g:Where( ... )`), the class its own class function makes (`xbf_ENUMERABLE`,
`xb_class_send_as`): a program's class named `Enumerable` is another one. A message neither answers is the usual
`GENERATOR:message` error. The `generator` benchmark (3,000,000 steps of one generator feeding another) takes about 80
ms.

## Classes

Classes compiled from xBase (`types/class.c`, `xb_class_def` in `xbrt.h`) are described by their module and
registered with it by name (a `STATIC CLASS` is not: it is known to its module only); the runtime prepares a class the
first time it is used (its class function, or a subclass):

- A parent is the module's own class of that name when there is one, else the class registered under the name (an
  unknown one is error 1001, as its class function would give). The class's line is the class, then each parent's
  line, depth first and left to right, each class once. Each class of the line has an area of the object's variables
  of its own: two ancestors' variables of the same name are two variables, each seen by its class's methods (the
  reference keeps one, the first ancestor's). A class met again while its line is built, with no INIT values
  evaluated meanwhile, inherits from itself (`BASE/0` `Class inherits from itself`, the class's name as the operation;
  the compiler reports it within a module).
- The messages it answers are tabled: for each class of the line in order, its methods, then its variables (`NAME`
  reads, `_NAME` assigns); the first class that has a message takes it. The INIT values are evaluated then, once per
  class (`CLS:(INIT)`); each new object gets them, an array or a hash copied for each object (`xb_clone_deep`). When
  evaluating them fails, the class is not usable yet and they are evaluated again the next time it is used; INIT
  values that need an object of their own class, or of a subclass being prepared, are `BASE/0` `Class used by its own
  initial values`.
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
- A message from a method of class F (or a codeblock it made: `xb_op_send_from`) reaches F's own HIDDEN member when
  F has one; else a method of the object's class (an overriding method wins, and so do a subclass's ACCESS and ASSIGN
  over a variable F declared); else the variable F sees (F's, or its ancestors', unless HIDDEN there); else what the
  object's class answers. `Super:message` takes the member of the ancestors after F in F's line; `Super:ClassName()`
  is F's first parent's name. When `Self` was assigned an object that is not of F's class, `Super:message` is
  `Message not found`.
- Class variables (`CLASSVAR`): each class of the line holds its own value of each, starting from the INIT value of
  the class that declares it (the value itself: an INIT array is the same array), except a `SHARED` one, which the
  class and its subclasses share; the values are roots of the collector. A cast reads the values of the class it sees.
- Friends: the methods of a class named by `FRIEND CLASS` (not its subclasses'), and a function named by `FRIEND
  FUNCTION` with the codeblocks it makes and the macros it evaluates (found from the innermost frame of the call stack
  that is not a macro's, a codeblock's name without its `(b)` prefix, `XB_BLOCK_PREFIX`), reach the HIDDEN and PROTECTED
  members of the class and assign its READONLY variables.
- Scopes: HIDDEN members answer F's own methods, PROTECTED ones the methods of their class and its subclasses, a
  READONLY variable is assigned by those only (and by friends); anything else is `BASE/41` `Scope violation (hidden)` or
  `BASE/42` `Scope violation (protected)` (generic code 13, the operation `CLASS:MESSAGE`, no `args`). A message the
  class does not answer is `BASE/1004` (`BASE/1005` to assign) `Message not found`, the operation `CLASS:MESSAGE`, the
  object in `args`, as in the reference; the runtime's own classes keep Clipper's `No exported method`.
- Casts: a message naming a class of the object's line that no member answers (`o:TParent`; `SUPER`, outside the
  `Super:` of a method, the first parent of the class the object is seen as) gives the object seen as that class,
  without allocating: the same item with the class's position in the line in its `width` field. A message to a cast
  reaches what that class answers (its methods, its variables in their areas), `ClassName()` is that class's name, a
  method receives the object itself as `Self`, and a cast kept in a variable stays one. A cast is the object: `==` gives
  `.T.` (the reference makes a new object, so `.F.`).
- A method's frame is marked `XB_PROC_METHOD`: `PCount()` and `PValue()` count the arguments after `Self`.
- Reflection (`rtl/objects.c`, names in any letter case): `ObjIsKindOf( o, cClass )` (the class or one it inherits
  from), `ObjHasVar( o, cName )` and `ObjHasMethod( o, cName )` (any scope; ACCESS and ASSIGN are methods),
  `ObjVars( o )` and `ObjMethods( o )` (the EXPORTED ones, upper-cased, in table order; anything but an object is
  `BASE/0`), `ObjSend( x, cMessage, ... )` (`x:cMessage( ... )` from outside the class; `_NAME` assigns). The
  runtime's own classes answer with their variables and no methods.
- Operators (`OPERATOR "op"`, `METHOD name OPERATOR "op"`): a method whose message is the operator (`XB_OPERATOR_*` in
  `xbrt.h`: `+ - * / % ^ = == != < <= > >= $ ! ++ -- []`; the compiler maps `**`, `<>`, `#` and `.NOT.` to them). The
  runtime's operator functions ask the class of the left (or only) operand when it is an object, before anything else
  (`xb_class_operator`; `==` too, even for the same object; for a cast, the class it shows): the method receives the
  right operand; `[]` receives the index, and the value when it assigns (`o[i] += x` reads, then assigns). Compiled fast
  paths never see objects, so they are unchanged. As in the reference, the right operand's class is not asked, and
  `.AND.`, `.OR.` and unary minus cannot be overloaded (the compiler rejects `.AND.` and `.OR.`, which the reference
  accepts and never calls).
- Built-in types as classes: a `SCALAR CLASS type` (`XB_CLASS_SCALAR`, named as `ClassName()` names the type: `ARRAY`,
  `CHARACTER`, `HASH`, `NUMERIC`, `DATE`, `BLOCK`, ...; it has no class function and no instance variables) is the class
  of the type's values when no class is associated with the type: a message to such a value reaches its methods, the
  value as `Self`, and one the class does not answer keeps Clipper's error (`No exported method`). The runtime's
  (`rtl/collections.prg`) give the type's functions as messages (`a:Len()`, `a:Add( x )`, `a:Sort()`, `"x":Upper()`,
  `s:Split( "," )`, `h:Keys()`, `n:Round( 2 )`, `d:Year()`, `b:Eval( ... )`); `ARRAY`, `CHARACTER` and `HASH` inherit
  `Enumerable`, whose methods work on any collection through `FOR EACH` over `Self`: `Select( bMap )`, `Where( bTest )`,
  `Take( n )`, `Skip( n )`, `TakeWhile( bTest )`, `SkipWhile( bTest )`, `SelectMany( bMap )`, `Concat( xOther )`,
  `Zip( xOther, [bCombine] )`, `Distinct( [bKey] )` (equal by `==`; numbers, strings and dates found through a hash) and
  `Reverse()` are generators (the sequence is made as it is gone through, so they chain); `OrderBy( [bKey] )` and
  `OrderByDescending( [bKey] )` give an array ordered as `ASort` orders values (the primitive `__ValueLess`), equal keys
  in their order; `GroupBy( bKey )` gives a hash of each key to the array of its elements, `ToHash( bKey, [bValue] )` a
  hash; and `ToArray()`, `Count( [bTest] )`, `Any( [bTest] )`, `All( bTest )`, `First( [bTest] )`, `Last( [bTest] )`,
  `Contains( x )`, `Aggregate( bFunc, [xSeed] )`, `Sum( [bValue] )`, `Min( [bValue] )`, `Max( [bValue] )`,
  `Average( [bValue] )` (the last three NIL without elements) and `ForEach( bBlock )` go through it at once. A class
  that inherits `Enumerable` only needs `GetEnumerator()`; a generator's enumerator answers `Enumerable`'s messages too
  (`xb_class_send_as`). `ClassName()` and `ValType()` of the values stay Clipper's.
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
- Inline caches: each send of compiled code has its own `xb_send_cache`, of `XB_SEND_CACHE_WAYS` (4) ways. A message
  that reaches a member of an object (not through a cast) stores the object's class, the member and its slot there (a
  variable's index, a class variable's address) as the newest way (the oldest of a full cache goes); the next message
  from that send to an object of one of those classes runs the member at once, without the lookup, the scope checks
  (which give the same answer for the same class at the same send) or a call to `dispatch` (`perform`). Messages to
  casts, to values of scalar classes and to the runtime's own classes always look up. The `oop` benchmark (method calls,
  one through `Super`, variable reads and assignments) went from 292 ms to 69 ms; a send that goes to objects of three
  classes in turn, from 46 ms to 23 ms for 3,000,000 messages once the cache held more than one class. Threads share
  the caches: each one is a sequence lock (`version`, odd while a thread changes it). A reader that finds the version
  odd, or changed once it has read a way, does the full lookup instead (nothing torn is used, nothing is tried again),
  and a writer that finds another one at work leaves the cache as it is; 20,000,000 messages to objects of two classes
  through one send take the same time as without it. Not yet: devirtualized calls and variables at known offsets in
  the generated code ([xir.md](xir.md#classes)).

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
- `xb_ref_target` returns the item a reference designates in the end (through a cell, then an element); an element
  is found again at every use, so it follows the array when it grows, and an index outside the array, after the array
  shrank, raises `Bound error: array access` (1132). `xb_ref_written` is the write barrier after a store there: the
  cell, the array, the hash, or the reference itself for a string's character (or after an error left no element).

## Memory variables

`PRIVATE` and `PUBLIC` variables (`core/memvar.c`, the `xb_mv_*` entry points in `xbrt.h`) are found by name when the
code runs, so a function sees and changes the memory variables of the functions that called it, whatever module they
are in. Each module gives the runtime the names it uses (`xb_memvar`, numbered when it registers; `RESTORE` numbers
the names of its file that no module uses, while the program runs, so `xb_env.memvars` may grow, and move, whenever
xBase code runs: C code keeps no pointer into it across an error block). The table of names is the process's: a name
is found under a reader-writer lock and added under its write lock, so a macro or `RESTORE` may number a new name on
any thread; a number's spelling is read without a lock, from chunks of entries that never move. Each environment keeps,
by number, the binding visible now (`xb_env.memvars`), and the PRIVATEs alive, in creation order, each with the
frame of the activation that created it and the binding it hid (`xb_env.privates`). Both are roots of the collector.

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
  a LOCAL, a STATIC or an element by reference lets it go for a NIL of its own. A PUBLIC no longer exists. A name with
  no variable is skipped.
- `RELEASE ALL [LIKE | EXCEPT skeleton]` (`__MemReleaseAll()`): the same for the PRIVATEs that the function or
  codeblock running it created, those whose names match the skeleton (`LIKE`) or do not (`EXCEPT`); PUBLICs and the
  callers' PRIVATEs stay. In a skeleton `*` stands for any characters and `?` for one, letters match in any case. The
  reference takes a skeleton that starts with `*` for every name (`*B`); xbase matches the whole skeleton, so it never
  releases more than the skeleton names.
- `CLEAR MEMORY` (`__MemClear()`): no PRIVATE or PUBLIC exists any more; the PRIVATEs' records stay until their
  activations end, restoring nothing.
- `SAVE TO file [ALL [LIKE | EXCEPT skeleton]]` (also `SAVE ALL LIKE | EXCEPT skeleton TO file`; `__MemSave()`,
  `rtl/memfile.c`) writes the variables visible, PRIVATE or PUBLIC, whose names match, to a `.mem` file (the extension
  is added when the name has none): Clipper's format, which `memfile.c` describes, holds strings (65534 bytes of each
  at most), numbers (with their width and decimals), dates and logicals, and the first 10 characters of each name;
  other values are left out. The records follow the names' numbers (the reference orders them otherwise; each record
  is the same). No file name (`SAVE` alone) is error 2008 (`EG_ARG`). A file that cannot be created or written is
  error 2006 (`EG_CREATE`, or `EG_WRITE`), which the block may retry; the default gives up, leaving a file already
  created or partly written as it is.
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
  of a numeric literal is `xb_num_literal_width`, shared). A macro sees memory variables, the program's public functions
  and the runtime's functions, by name; never a function's `LOCAL`s or `STATIC`s, nor a `STATIC FUNCTION`. `M->name` and
  `MEMVAR->name` are memory variables; a name alone is a field of the current work area before a memory variable
  (`MO_MV_FIELD`), and the aliases of [rdd.md](rdd.md#field-references) name fields (`alias->name`, `FIELD->name`,
  `FIELD->alias->name`, `( expr )->name`, `&( expr )->name`) or evaluate in a work area (`alias->( expr, ... )`,
  `( expr )->( ... )`, with `MO_AREA`, which makes the area current again when the evaluation ends, also by an error);
  `M->( expr )` names the alias `M`, as in compiled code. A codeblock made from text sees its own parameters, not those
  of the codeblock around it (as a compiled one, Clipper's rule). Inside the text, `&name` is a macro of its own (the
  variable's text); a name joined to other identifiers there is one the substitution below could not make, an error. A
  text nested deeper than 1024 levels (parentheses, a chain of operators, `a[1][2]...`), the compiler's limit, is error
  1449, so that compiling it keeps the stack small.
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
  entries chosen by a hash of the text, roots of the collector): a macro in a loop is compiled once.
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
- Not yet: `o:&message`, `@&name`. `PCount()` inside a macro expression counts the arguments of the function or
  codeblock running it; inside a codeblock the macro made, the codeblock's own (`PValue()` gives them as passed).

## Calling convention

Every xBase-callable function, compiled or written in C, has the dynamic convention:

```c
void xbf_NAME(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
```

- `NAME` is the upper-cased xBase name. Runtime functions are `xbf_NAME` (`XB_RUNTIME_FUNC_PREFIX`) and the
  program's are `xbp_NAME` (`XB_PROGRAM_FUNC_PREFIX`), so a program may define a function named like a runtime one
  that is not `RESERVED` without a clash at link time.
- Every runtime function is declared in `include/rtl.ch` (`EXTERN FUNCTION`, see [compiler.md](compiler.md#calls));
  the `runtime.declarations` test checks that the declarations and the `xbf_` definitions match.
- This is the convention of every function's symbol: the runtime, `Do()`, codeblocks and other modules call through
  it. Within a module, compiled code calls a function's direct entry, passing the parameters' items in registers
  ([compiler.md](compiler.md#xir-to-llvm)).
- The caller evaluates the arguments into consecutive slots of its own frame and sets `ret` to `NIL`; a callee that
  returns nothing leaves it untouched. Compiled code always writes `ret` when it returns (`NIL` for no value), so a call
  to a direct entry within the module does not clear it first (when the callee unwinds, the result is not read). The
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

`xb_ctx` starts with the fields compiled code uses (`xbrt.h`): `uint32_t pending`, `int32_t set_decimals` (SET
DECIMALS, which inline `/` and `%` read), `xb_frame *frames` and `const char *stack_limit` ([Frames](#frames)).
`pending` holds requests. The runtime sets bits there (`XB_PENDING_BREAK`, `XB_PENDING_QUIT`, `XB_PENDING_SAFEPOINT`,
`XB_PENDING_FINALIZE` when there are objects to destroy, and `XB_PENDING_TRAP` while `Type()` evaluates, see [Macro
operator](#macro-operator)). Compiled code tests the field after each fallible call; when it is set the code calls
`xb_pending_gc` (or `xb_pending`, which never collects nor parks the thread, where a substituted result waits unseen:
see [compiler.md](compiler.md#xir-to-llvm)), which serves a collection another thread makes (the thread parks: a
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
subsystem `BASE` (the work areas' errors are `DBCMD` and the driver's name, [rdd.md](rdd.md#work-areas)), the subcode,
the generic code (`error.ch`, shared with xBase code like `set.ch`) and its description (in the current language,
[Languages](#languages)), the operation, severity `ES_ERROR`, `canSubstitute`, and in `args` the operands of an operator
or the arguments of a function (NIL when there are none). Then it evaluates the context's error block with it
(`ErrorBlock( [bNew] )` returns the block and replaces it with a codeblock argument; the block and the `BREAK` value are
roots of the collector). With no error block (a program's `ErrorSys()` set none) the error is unrecoverable:
`Unrecoverable error 9002: No ERRORBLOCK() for error`. After the block:

- a request to unwind is pending (the block called `Break()`, or quit): the operation fails with it;
- the error may be retried (`XB_ERR_RETRY`, `xb_error_retry`: `canRetry`, with `tries` counting the attempts from 1;
  a missing memory variable, a file error) and the block returned `.T.`: the operation is tried again;
- the error has a default (`XB_ERR_DEFAULT`: `canDefault`; a file error, `xb_error_file`, which also sets `filename`
  and `osCode`, the `FError()` code of [Files](#files)) and the block returned anything else: the operation does its
  default, which gives up quietly;
- the error allows substitution (`XB_ERR_SUBSTITUTE`: operators, `SWITCH`, a non-numeric index, messages, `Eval` and
  the runtime functions, except `ASize`, `AEval`, `AFill`, `Set` and the array dimension): the block's return value
  becomes the operation's result, and the program goes on;
- otherwise (a bound error, an assignment to an element, a condition that is not logical, the collection of
  `FOR EACH`, ...) the program ends: `Unrecoverable error 9001: Error recovery failure` on standard error, exit code
  1, a `QUIT` pending.

Which errors allow substitution, and their `args`, follow Clipper/Harbour. An error raised while the error block runs
is handled the same way, nested at most 8 deep (`XB_ERROR_MAX_DEPTH`); one more ends the program (`Unrecoverable
error 9003: Too many recursive error handler calls`). In a `FOR` test, a substituted result stands for the comparison
`counter > limit` (`<` stepping back): a logical, else the error of a condition.

A context starts with no error block. The program's startup calls `ErrorSys()` by name after the `STATIC`
initializations and before the `INIT` procedures ([Program startup](#program-startup)): the runtime's, in xBase
(`rtl/errorsys.prg`), unless the program defines its own. It installs `{| oError | DefError( oError ) }`: a division by
zero gives 0 (as Clipper's default handler does); an opening refused for the file's sharing (`EG_OPEN` with system
code 32) and an append that cannot lock its record (`EG_APPENDLOCK`), when they allow the default, set `NetErr()` and
do nothing, as in Clipper ([rdd.md](rdd.md#sharing-locks-and-concurrency)); any other error prints
`Error <subsystem>/<subcode>  <description>: <operation>` (the file name instead of the operation when there is one,
then ` (DOS Error <osCode>)` when there is a system error code; `Warning` for a severity below `ES_ERROR`), then a line
`Called from NAME(line)  ` for each level of the call stack (see [Frames](#frames); the handler's own frames are
hidden, so level 0 is where the error happened), to standard error (`OutErr()`), and ends the program with
`ErrorLevel( 1 )` and `QUIT`. The choice between quitting, retrying and the default (Clipper's `Alert()`) waits for the
terminal layer. An unrecoverable error lists the levels as `Called from NAME(line) in FILE` (without ` in FILE` for a
runtime function). Both list the innermost `XB_CALLS_SHOWN` (100) levels and then, when there are more, the line
`(further calls not shown)` (`include/error.ch`): the reference lists every level, which after a runaway recursion
means millions of lines.

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
| `+` on strings | concatenation |
| `-` on strings | concatenation moving the left operand's trailing blanks to the end |
| `=` on strings | with `SET EXACT OFF`, compares only the right operand's length (`"abc" = "ab"` is true) |
| `==` | exact comparison; on strings byte-for-byte |
| `<` `<=` `>` `>=` `!=` | numbers, strings (same rule as `=`), logicals (`.F.` < `.T.`); NaN is ordered with nothing (IEEE 754, `xb_order_is` in `xbrt.h`): every comparison with it is false but `!=`, so a `FOR` loop ends at one, where the reference loops forever |
| `$` | substring test; an empty left operand is false; with a hash on the right, whether the left is one of its keys |
| `.NOT.` | logicals only |
| `&` `\|` `^^` `<<` `>>` `~` | xbase's bit operators (`xb_op_bitand`, ..., `xb_op_bitnot`): numbers taken as integers (`xb_dbl_to_i64`: truncated, saturated, NaN 0), an integer result; `>>` is arithmetic; anything else is an argument error with the operator and the code of its operation (`xb_bit_function`) |

Comparing `NIL` with `=`/`==`/`!=` is allowed; other type mismatches raise `BASE` argument errors (whose error block
may substitute the result, see [Errors and pending requests](#errors-and-pending-requests)). `==` on two arrays,
two codeblocks, two objects or two hashes tests identity; other comparisons of them are argument errors.

Element access `a[i]` (`xb_op_index`) and assignment (`xb_op_index_set`) raise `Argument error: array access` (1068)
or `array assign` (1069) when the base is not an array or the index not a number, and `Bound error` (1132/1133)
outside `1 .. Len( a )`.

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
codeblocks, objects and hashes as nothing, and
numbers through `xb_num_to_str` (`runtime/src/core/numfmt.c`):

- the field has the item's width (or `xb_num_int_width`/`xb_num_dbl_width`) for the sign and integer digits, then a
  dot and the item's decimals;
- doubles keep 16 significant digits (later digits print as `0`) and are rounded half-up to the decimals;
- a minus sign appears only when a non-zero digit is shown (`-0.001` with 2 decimals prints `0.00`);
- a value that does not fit, and any infinite or NaN value, fills the field with `*`.

The 16 digits are the correctly rounded ones. Harbour derives them by repeated floating-point division, which can
change the last significant digits of values above 2^53 (`51090942171709440000` prints as `51090942171709430000`
there).

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
  width of the text it read (blanks, sign and every character before the dot) and the decimals after it.
- `Str( n [, nWidth [, nDec]] )`: `nWidth` is the whole field; without it the number's own field is used (at most 90
  columns), and a `nWidth` below 1 means 10 columns. A field with no room for its decimals (`nDec >= nWidth`, or no
  integer column left) is all `*`. `StrZero` is `Str` with leading blanks as zeros.
- `Int( n )` is the integer part with `n`'s display width: a field's integer columns (`N(6,2)`: 3), a number's own
  width otherwise; an integer comes back as it is.
- `Round` rounds half away from zero; a negative number of decimals rounds to tens, hundreds, ... As in Clipper, the
  half is added to ten times the scaled value and the result, divided back, is multiplied by `1 + DBL_EPSILON`
  (about one unit in the last place) before truncating, so decimal halves stored just below `.5` (`2.675`) round up
  while `0.4999999999` rounds down. That product is rounded once (`fma`), so a floating-point unit that computes with
  more bits (x87) gets the same result. A scaling that overflows gives a non-finite result (shown as stars). `Int`
  truncates without that adjustment.
- `Sqrt` of a negative number and an `Exp` that overflows give 0; `Log` of a negative number gives minus infinity, like
  `Log( 0 )`.
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
  `"99999999"` template). SET FIXED does not exist yet, so numbers keep their own decimals. Without a picture: a
  string as it is, a number as `Str`, a date as `DToC`, a logical as `T`/`F`; any other value, or a picture that is
  not a string, is error 1122. Compared with the reference over random values, pictures and settings, the results are
  the same.
- The memo-line functions (`rtl/memoline.c`) cut a text into lines of a width (79 by default; tab size 4, word wrap on):
  `MemoLine`, `MLCount`, `MLPos`, `MLCToPos`, `MPosToLC` share one scanner, Clipper's rules and their corner cases (a
  line ends at a hard return, CR LF, or before the character that would pass the width: with word wrap after the
  line's last blank; a soft return, the byte 141 and LF, is skipped with word wrap and ends the line without it; a
  tab takes the columns to the next multiple of the tab size). The reference reads its line end from a setting and
  takes more arguments; xbase takes Clipper's arguments and CR LF. Compared with the reference over random texts
  (letters, blanks, tabs, hard and soft returns, lone CR and LF) and arguments, the results are the same. `HardCR`
  makes soft returns hard; `MemoTran( c, [cHard], [cSoft] )` replaces hard and soft returns by the first character of
  its arguments (`;` and a blank by default, also for an empty one, where the reference writes a zero byte).
- `MemVarBlock( cName )` returns a block that reads the memory variable and, called with a value that is not NIL,
  assigns it; NIL when no variable of that name is visible.
- `StrTran( c, cSearch, cReplace, nStart, nCount )`: `nStart` or `nCount` of 0 gives `""`; a negative `nStart`
  replaces nothing, a negative `nCount` replaces every occurrence from `nStart`.
- `Stuff( c, nStart, nDelete, cInsert )`: `nStart` 0 is 1; a negative `nStart` or `nDelete` counts as past the end.
- Trimming from the left, `Val` and `Empty` skip blanks, tabs, carriage returns and line feeds (`xb_is_blank`); trimming
  from the right removes spaces only. Case changes and the character classes (`IsAlpha`, `IsUpper`, `IsLower`) read
  letters as SET CODEPAGE says ([Codepages](#codepages)); `IsDigit` takes ASCII digits.
- `Empty` is true for `NIL`, `0`, a string of blanks (as above), `.F.`, an empty array and the empty date.
- `Set( n [, x] )` returns the setting before the call. `_SET_EXACT` takes a logical or a string starting with
  `ON`/`OFF`; `_SET_DECIMALS` and `_SET_EPOCH` take a number of 0 or more (a negative one is error 2020);
  `_SET_DELETED`, `_SET_SOFTSEEK` and `_SET_UNIQUE` take a logical or `ON`/`OFF` as `_SET_EXACT` does
  ([rdd.md](rdd.md#orders));
  `_SET_DATEFORMAT` takes a string (kept whole) and sets `SET CENTURY` on when its first run of `Y`s has 4 or more, off
  otherwise; `__SetCentury( [lOn] )` rewrites that run of `Y`s as `YYYY` or `YY` (or puts it in front when the picture
  has no `Y`) and the picture in upper case. `_SET_LANGUAGE` takes a language's name ([Languages](#languages)),
  `_SET_CODEPAGE` a codepage's ([Codepages](#codepages)). Other values change nothing.
- `__GcCollect( [lYoung] )`: see [Garbage collector](#garbage-collector).
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
  `nStart` follows `ASort`'s rule, `nCount` missing or reaching past the end means the rest, and 0 or negative fills
  nothing, as does a missing `xValue`. Anything but an array is an argument error (`BASE/6004`). Each store goes
  through the write barrier.
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
  functions and `CPConvert` have 9009 to 9015 ([Codepages](#codepages)).
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
- Primitives for them: `__ConsoleWrite( xValue )` writes the display form of a value ([Display](#display)) to the
  console (standard output until the terminal layer exists), `__StdWrite( nHandle, xValue )` to the standard output (1)
  or error (2), flushing the output before writing to the error; `__ArgError( nSubCode, [lSubstitute] )` raises the
  calling function's argument error (`EG_ARG`, its name as the operation, the arguments it received), as its C
  counterparts do: not substitutable, or with `lSubstitute` substitutable, the handler's value its result.
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
which are never to be changed. The texts are UTF-8 bytes (`Len( "Terça-feira" )` is 12), as the source is.

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
  single-byte codepage the code point, the upper and lower case and the class of each of its bytes.
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
  `DirChange`, `DiskSpace`, `GetEnv` and `__DirScan`. A handle is the system's handle as a number. Each function stores
  the code of its outcome for `FError()` (`xb_env.file_error`, 0 after a success); an argument of the wrong type gives
  the function's failure value and leaves `FError()` alone, but a name that is not a string is `FOpen()`'s `BASE/2021`,
  and an access of 3 is 5. `FRead( nHandle, @cBuffer, nBytes )` replaces the first bytes of the buffer, which keeps its
  length; without the buffer passed by reference, or with `nBytes` below 1 or past its length, it reads nothing.
  `FWrite` writes the whole buffer for a count past its length or below 0, and writes nothing, successfully, for a count
  of 0. A seek before the start is 25 from any origin (an unknown origin is the start) and leaves the position, which
  `FSeek` returns. `CurDir()` gives the current drive's directory without its drive and leading separators (its argument
  is not used); `DiskSpace( [nDrive] )` counts the drive on Windows only. `__DirScan( cSpec )` gives
  `{ cName, nSize, dDate, cTime, cAttributes }` for each entry, of any attributes (letters in the order `RHSDA`), whose
  name matches the mask of `cSpec` (`*` and `?`; a mask ending in `.*` also takes a name without a dot, so `*.*` takes
  every name, as in DOS; letter case counts where the file system tells it, and is folded for ASCII letters only); a
  mask without wildcards names one entry, and none (a directory's name with its separator) every entry of the directory.
  A link that leads nowhere is listed as the link itself. On POSIX systems a name starting with a dot (but `.` and `..`)
  is hidden (`H`); on Windows the attribute is the file system's.
- `rtl/files.prg`: `Directory( [cSpec], [cAttributes] )` lists the files without the attributes D, H and S, and those
  whose such attributes `cAttributes` all names (`V`, the volume label, gives none); `ADir` fills its arrays from it
  (directories, hidden and system files too when the attributes' array is given; 0 without `cSpec`); `File()` is whether
  a file, hidden or system too but not a directory, matches; `MemoRead` reads a whole file without the end mark
  (`Chr( 26 )`) that ends it, `MemoWrit` writes the text and that mark; `FReadStr` reads and stops at the first NUL (the
  file position past all the bytes read).
- Safer than the reference on POSIX systems, as DOS and Windows behave: a directory cannot be opened (`FOpen`) or
  deleted (`FErase`): 5; `FRename` never replaces another existing file: 5; a read-only file shows `R`. POSIX systems
  have no sharing modes (the sharing bits of `FOpen()` are ignored there), and they mark names starting with a dot
  hidden, which `Directory()` lists only with `H`, whatever the mask. `MemoRead` and `MemoWrit` leave the failure's code
  in `FError()`. On Windows, `FCreate` of an existing hidden or system file keeps those attributes. File offsets are 64
  bits everywhere.

## Work areas

The work areas, their tables and the database functions are in `runtime/src/rdd/` ([rdd.md](rdd.md)). Each environment
has its own (`xb_env.areas`, `areaCurrent`, `set_deleted`, `rddDefault`, `netErr`). Compiled code reaches fields
through three entry points of `xbrt.h`, each call with a cache of its own (`xb_field_cache`, a zeroed static of the
reference: the `generation` of the table it found the field in, from a process-wide atomic 64-bit counter, and the
field's position, in one 64-bit word read and written atomically, so threads find either their table's entry whole or
another one; the word keeps 48 bits of the generation and 16 of the position, and a table past either is looked up
each time):

- `xb_field_get( ctx, r, area, alias, name, cache )` reads field `name` (upper case) of the work area that the item
  `area` names (an alias or a number, for `( expr )->name`), else the C string `alias`, else the current one (both
  NULL); `xb_field_set( ctx, area, alias, name, cache, v )` writes it. A field that is not there is `BASE/1003`, an
  alias that names no area `BASE/1002`, both retried while the error block returns `.T.`.
- `xb_mv_field( ctx, r, memvar, cache )` reads an undeclared name: the current area's field of the variable's name,
  else the memory variable (as `xb_mv_get`).

`__AreaEval( xArea, bBlock )` (`alias->( expr )`) and the macro machine's `MO_AREA` make an area current through
`xb_area_enter`, which raises the alias error.

## Threads and synchronization

The program's code runs on one thread today; the services below are what tasks and threads are built on
([concurrency.md](concurrency.md)).

- Contexts (`core/context.h`): a thread context (`xb_ctx`) holds what the code running on its thread uses (the
  `[ABI]` head compiled code reads: pending requests, `SET DECIMALS`, the chain of frames, the stack
  limit; the C roots, a `BREAK` on its way, the error blocks running, `Type()`'s trap) and caches of its own (the macro
  cache), and points at the environment of the flow it runs (`xb_env`): `ErrorBlock()`, the memory variables, the `SET`
  values, `FError()`, the language and the codepage, the work areas and their settings. Today each thread context has
  one environment, made with it; the program's exit code is the process's.

- Platform layer (`platform/platform.h`, implemented in `posix/` and `win32/`): threads (`xb_thread_start` with a
  reserved stack size, rounded up to the system's page and minimum; `xb_thread_join`), mutexes (`xb_mutex`, with
  `trylock`), condition variables (`xb_cond`, with waits timed on the monotonic clock: POSIX binds the condition to it,
  macOS waits for a relative time, Windows in whole milliseconds rounded up), reader-writer locks (`xb_rwlock`; on glibc
  of the kind where a waiting writer goes before readers that come after it, elsewhere with the system's policy), the
  number of processors the program may run on (its affinity on Linux, else every processor online), the bounds of the
  calling thread's stack (`xb_platform_stack_bounds`), and a monotonic clock in nanoseconds
  (`xb_platform_monotonic_ns`), which `ElapsedMs` counts from the program's first context (`xb_program_elapsed_ms`). The
  functions that may block (files, directories) are the operating system's side (`xb_os_*`, `platform/os.h`) wrapped
  once for every platform through `xb_blocking_run` (`platform/blocking.c`, the arguments in a structure), which the
  collector provides ([Garbage collector](#garbage-collector)); so is the flush of a C stream (`xb_platform_flush`).
  Each object is made by its `init` function, which fails only for lack of memory or system objects, and freed by its
  `destroy` function. None of these locks is recursive: the thread holding one never asks for it again, not even for a
  second read lock, which would wait behind a waiting writer.
- Atomic operations (`platform/atomic.h`) on ordinary 8-bit, 32-bit, 64-bit, `size_t` and pointer fields, so that
  memory read plainly where no other thread sees it (an object header, a field compiled code reads) is also changed
  atomically where one may: loads, stores, add, or, and, exchange and compare-and-swap with an explicit order (not
  every one for every size). A 64-bit field used atomically is aligned to 8 bytes (`XB_ALIGN8` in `xbrt.h`), which
  32-bit targets do not do by themselves. GCC and
  Clang use their `__atomic` builtins (with libatomic on a target that lacks native atomics of some size, found when
  the build is configured); MSVC builds the runtime with `/volatile:ms` (volatile loads acquire, stores release, on
  every architecture; a build with `/volatile:iso` stops) and its `Interlocked` intrinsics.
- On top of them, the same everywhere (`core/sync.c`): `xb_thread_self` (a number per thread, never 0, no other
  thread's until 2^32 threads have asked), `xb_once` (a function run once whatever threads ask at the same time, the
  others waiting until it returns), a recursive lock (`xb_rlock`, made statically, for rare nesting work such as
  preparing classes: the thread holding it takes it again, and holds it until it has released it as often), and a
  parking lot: `xb_park_wait` sleeps while a 32-bit value at an address is the
  one expected (with an optional limit in nanoseconds, which its own deadline decides), `xb_park_wake` wakes the
  waiter of the address that came first, or every one. Waiters queue in arrival order in one of 256 buckets chosen by
  the address (each made when first used) and sleep on a condition of their own, so a wake reaches only the waiters
  it marks; the value is tested under the bucket's lock, which the waker takes after changing it, so no wake is lost.

## Program startup

Before `main`, the global constructor of every module calls `xb_module_register` (`core/module.c`) with the
initialization of its `STATIC`s (which become a GC root), its `INIT` and `EXIT` procedures, its public functions by
name, and the names of its memory variables ([Memory variables](#memory-variables)). These go into the symbol table
(`core/symbols.c`): upper-cased names numbered by a table of names (`core/names.c`, the one memory variables use too;
lookups by a name in any letter case upper-case it first, `xb_names_find_upper`)
and what each designates, which the runtime's own functions join when the program starts (`xb_program_run`), from a
table generated from `include/rtl.ch` at build time (`runtime/symbols.cmake`, with which parameters take references,
read from the declarations by `runtime/rtl_declarations.cmake`); a program function hides a runtime function of its
name. The table refers to every runtime function, so every program links them all in. `Do( cName | bBlock, [args] )`
calls a function found there (letter case aside; spaces are part of the name, which ends at a NUL) or a codeblock, with
its arguments: a reference goes on as passed to a compiled function and to a runtime function's parameter declared `@`,
other runtime parameters receive the value. Do shows as `DO` on the call stack, and ends the PRIVATEs that a runtime
function creates for its caller (`RESTORE`), as a compiled caller does; an unknown name is `BASE/1001`
`Undefined function` (not substitutable), anything else as its first argument `BASE/3012`.

`main` (in `runtime/src/core/main.c`) runs the platform setup (`xb_platform_init`: on Windows, standard output and error
in binary mode so programs write the same bytes everywhere) and takes the command-line arguments in UTF-8
(`xb_platform_args`: as `main` received them on POSIX systems, converted from the wide command line on Windows). Then,
on the program's thread (started with `xb_thread_start` and a stack reserved at `XB_PROGRAM_STACK_SIZE`; the process's
first thread when it cannot be started), attached to the collector, it creates the context (the first, which starts the
`ElapsedMs` clock), makes the arguments after the program's name strings in a frame of its own linked into the context's
chain (so the collector sees them), calls `xb_app_main( ctx, argc, argv )` (emitted by the compiler) with them, flushes
output, detaches and returns the exit code. `xb_app_main` checks the ABI and calls `xb_program_run`, which runs, in this
order:

1. the modules' `STATIC` initializations, in registration order;
2. `ErrorSys()`, found by name (the program's own, else the runtime's), which installs the error block
   ([Errors and pending requests](#errors-and-pending-requests));
3. the `INIT` procedures, module by module in registration order and, within a module, from the last one written to
   the first (as the reference does), each with the command-line arguments;
4. the start function, with the arguments, as Clipper passes them: strings, `NIL` for a parameter without one, and
   arguments past the parameters passed too;
5. the `EXIT` procedures, module by module, each module's in the order written, without arguments.

A request to unwind (a `QUIT`, a `BREAK` with no sequence, an error that ended the program) stops steps 1 to 4; the
request is then dropped (with a `BREAK` value) and step 5 runs, a new request stopping it. Then every table open is
written and closed ([Work areas](#work-areas)), whatever ended the program. The exit code is the process's (any thread's
code sets the same one) and kept throughout: `ErrorLevel( [nNew] )` returns it and sets it from an argument, a number
wrapped to 32 bits, anything else 0 (the default error handler sets 1). Registration order follows the global
constructors, which is the link order on the usual toolchains. Inside an `EXIT` procedure the call stack starts with it
(the reference shows the function that ended the program, such as `__QUIT`, under it).
