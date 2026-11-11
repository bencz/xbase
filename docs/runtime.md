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
- Arrays (`xb_array` in `xbrt.h`: `len`, `cap`, `items`; `types/array.c`, functions in `rtl/arrays.c`) are 1-based.
  Their items start in the array's own allocation and move to a separate heap object when the array grows past it.
  Compiled code reads `len` and `items` to access elements in place.
- Hashes (`xb_hash` in `types/types.h`) map keys to values in insertion order; see [Hashes](#hashes).
- Codeblocks (`xb_block`: function, captured cells) and cells (`xb_cell`: one item at offset 0) are described in
  [Codeblocks](#codeblocks), objects in [Objects and messages](#objects-and-messages).

## Frames

Parameters, `LOCAL`s and temporaries of a compiled function live in its item frame (`xb_frame`: link, what the call
stack shows, slot count, current line, the arguments as received, items), an `alloca` on the machine stack. The function
links it into its context's chain (`xb_ctx.frames`) at entry, with the parameters copied into the first slots (from
`argv`, `NIL` past `argc`, or from registers in a direct entry, [compiler.md](compiler.md#xir-to-llvm)) and the other
slots `NIL`, and unlinks it at exit; every exit goes through one block, and errors return normally (the pending flag),
so the chain is always right. The collector scans the frames of every context's chain. A frame dies with its function:
nothing keeps a pointer to a slot (a variable that outlives it moves into a heap cell first). The program runs on a
thread whose stack is reserved at 256 MB (`XB_PROGRAM_STACK_SIZE` in `core/main.c`; pages are committed as used), so
deep recursion has room; running past it is not detected yet and ends the process.

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
Operators and statements show nothing of their own. `ProcName( [nLevel] )`, `ProcLine( [nLevel] )` and
`ProcFile( [nLevel] )` read it: level 0 is the function calling them, a non-numeric level is 0, a negative one gives
`PROCNAME`, line 0 and `""`, a level past the end `""`, 0 and `""`. `ProcFile` gives the name of the function's source
file (`xb_proc.file`; a codeblock's is its function's), without the directories: the reference keeps the path given to
the compiler, which would put build directories into programs; a runtime function, in C or in xBase, has none (`""`).

`argc` and `argv` in the frame are the arguments as the function received them, stored only by a function or codeblock
that calls `PCount()` or `PValue()` itself (`XB_PROC_ARGS` in its `xb_proc`; nothing else can read them, and the other
calls cost nothing more); they belong to its caller (slots of the caller's frame, or items the runtime keeps rooted),
which keeps them during the call. The function may replace them (`PValue()`), so a caller does not read an argument
again after the call: `HMerge` merges the entry as it was before its filter ran, and each `INIT` procedure receives its
own copy of the command-line arguments. `PCount()` and `PValue( nArg, [xNew] )` (`rtl/lang.c`) read the innermost frame
with a `proc` (a codeblock's own, also a hidden one of the runtime's xBase code; `Do`'s, with `Do`'s own arguments,
while `Do` runs; a frame without `XB_PROC_ARGS` counts no arguments): `PCount()` is `argc`; `PValue()` gives argument
`nArg` (a fraction truncated) for `1 <= nArg <= argc`, `NIL` otherwise: a parameter's current value (the first
`xb_proc.nparams` slots; a function calling `PValue()` keeps its parameters there, see
[compiler.md](compiler.md#calls)), or past the parameters the argument as passed. With `xNew` it stores `xNew` there,
through the reference when one was passed, and returns it.

## Garbage collector

`runtime/src/gc/gc.c` is a precise, non-moving, generational mark-region collector with an Immix heap layout. Strings,
arrays, array item storage, hashes, hash entry stores, codeblocks, cells, reference targets and objects (instances of
classes) are its objects; an 8-byte header (size, kind, state, flags) precedes each one, so object pointers are what the
rest of the runtime and compiled code use.

- Heap: objects up to 8 KB (header included) live in blocks of 32 KB, aligned to their size and divided into lines of
  128 bytes. A block's first lines hold its metadata: per line, the epoch of the last collection that reached an
  object covering it and whether an old object covers it, and a bit per 8 bytes where an object starts (`xb_hblock`).
  Blocks come from chunks of 32 blocks obtained with `malloc` and aligned by hand. Larger objects are allocated
  individually (`XB_GC_HDR_LARGE` in their header).
- Allocation: every thread fills a hole, a run of free lines, by bumping a cursor, without locking; when the object
  does not fit, it takes the block's next hole, then a recyclable block (free lines among live ones), then a free
  block. An object above one line that does not fit the hole goes to an overflow run of the thread instead (holes
  large enough for it, from the same recyclable blocks, then free ones; a recyclable block without one leaves the list
  until the next sweep), so no hole is given up for it and the free lines of recyclable blocks serve objects of every
  size.
- Roots: the frames and temporary C roots (`xb_gc_root_push`, a vector of the context that grows as C calls nest) of
  every registered context, and global item arrays
  registered with `xb_gc_add_root` (the modules' `STATIC`s).
- Generations: marks are sticky. An object marked by a collection stays marked and is *old*; new objects are young.
  A young collection marks from the roots and the remembered set, stops at old objects instead of traversing them,
  and frees only the young objects it did not reach. A full collection clears every mark and traces the whole heap;
  it runs when the old generation reaches twice its size after the previous full one (at least 32 MB).
- Aging: a young object reached by a young collection ages, and is promoted by the next one that reaches it, so
  objects that die soon after a collection (a table whose entries are replaced) die young. An object that is old
  after a young collection and still refers to a young one stays in the remembered set (an array keeps the card
  dirty) until that one is promoted or dies. Aging adapts: it turns off when the bytes promoted at a young collection
  exceed four times the aged bytes that died (aging then only doubles their tracing; reached objects are promoted at
  once), and back on when a full collection finds that more than half of the old bytes had died.
- Young budget: a young collection runs when the allocation since the last collection reaches the budget, at least
  8 MB. The budget doubles after a collection where more than half of the allocation survived (collecting such
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
  leaves the set.
- Card marking: array storage for 1024 items or more carries one card byte per 64 items after its last item. A store
  into such an old array marks the element's card and records the array for its cards only; the young collection
  scans the marked cards (eight card bytes per step) instead of the whole array. Storage that is replaced (the array
  grew) records the whole array.
- Collection: marking uses an explicit stack. Marking an object gives the lines it covers the collection's epoch, and
  making it old (a full collection, a promotion) makes them old lines. A line is then free when it is not old and no
  object reached by this collection covers it: sweeping reads a block's line marks, never its objects, and makes the
  lists of free and recyclable blocks. A young collection sweeps only the blocks that received objects or hold aged
  ones since their last sweep; old lines stay until a full collection, which clears every line and walks the objects
  through the start bits to clear their marks. When the epoch (8 bits) wraps, every line mark is cleared. Large
  objects are freed at once. Memory is not returned to the operating system.
- Frame temporaries are not cleared after each statement, so a value a temporary held stays alive until its slot is
  reused or the function returns.
- Sizes that cannot be allocated (an array longer than `XB_ARRAY_MAX_LEN`, an object above half the address space,
  memory exhausted), a lock that cannot be created and an ABI mismatch
  end the program through `xb_gc_fatal`, the runtime's single fatal exit, with a message on standard error and exit
  code 1.
- Rules for C code: a collection can run inside any allocation, so every object still needed must be reachable from
  a frame or a C root before the next allocation, and a store into a heap object that may have aged since it was
  allocated (any allocation in between) is followed by `xb_gc_write` or `xb_gc_write_item`. Objects never move.
- Threads: global structures are protected by a lock (`xb_mutex`, platform layer); the allocation fast path takes
  none. A collection requires every mutator to be stopped at a safepoint (`XB_PENDING_SAFEPOINT`); while tasks run on
  one thread the allocating thread is the only mutator.

`__GcCollect( [lYoung] )` runs a full collection (a young one with `.T.`) and returns the number of live objects the
last full collection found (tests use it).

## Hashes

`types/hash.c` holds the hash itself, `rtl/hashes.c` the `H*` functions, and `rtl/collections.c` what they share with
the array functions: argument ranges, block calls from C, the order of values, the stable merge sort and deep copies.

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
instance variable. The runtime defines its classes in C (`xb_class`: name and variables); methods and classes written
in xBase are planned. `xb_op_send(ctx, ret, message, argc, argv)` sends a message to `argv[0]` with the arguments
`argv[1 .. argc-1]`; the compiler passes the message upper-cased, and an assignment `o:x := v` sends `_X` with `v`.

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

## References

An item that designates another item is an `XB_T_CELL` (a variable in a cell: `v.p` is the `xb_cell`) or an `XB_T_REF`
(an element: `v.p` is an `xb_ref` holding the collection, the 1-based index, for a string the current character, and
for a hash element taken by `@h[k]` or from a loop the entry's key, see [Hashes](#hashes)).
The two tags are consecutive, so compiled code tests for both with one unsigned comparison. A cell may hold an
`XB_T_REF` (a `FOR EACH` variable); an element never holds either.

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
the names of its file that no module uses, while the program runs, so `xb_ctx.memvars` may grow, and move, whenever
xBase code runs: C code keeps no pointer into it across an error block); each context keeps,
by number, the binding visible now (`xb_ctx.memvars`), and the PRIVATEs alive, in creation order, each with the
frame of the activation that created it and the binding it hid (`xb_ctx.privates`). Both are roots of the collector.

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
  returns nothing leaves it untouched. The callee copies its parameters into its frame; missing arguments are
  `NIL`. An argument passed by reference is an `XB_T_CELL` or `XB_T_REF` item (see
  [References](#references)); a C function receives one only where its declaration says `@`.
- A list with `...` (a call, a send or an array literal) has a length known at run time: compiled code reserves a frame
  of that size on the stack (`llvm.stacksave`/`stackrestore` around it), linked into the chain while the call runs, and
  fills it with the other items and, after them, the function's own arguments past its parameters
  (`xb_args_rest`); a reference among those goes on as it is where the callee takes references (a compiled function, a
  runtime function's `@` parameters: `xb_refs_take`) and as its value elsewhere (a method, an array element; its target
  gone: an error). `Do` passes its arguments on by the same rule.
- Every 32-bit parameter of a runtime entry point is `uint32_t` and every 32-bit result `int32_t`; the compiler
  relies on this to apply the target's argument-extension rules.

## Errors and pending requests

`xb_ctx` starts with the fields compiled code uses (`xbrt.h`): `uint32_t pending`, `int32_t exitcode`,
`xb_frame *frames` and `int32_t set_decimals` (SET DECIMALS, which inline `/` and `%` read). `pending` holds requests.
The runtime sets bits there (`XB_PENDING_BREAK`, `XB_PENDING_QUIT`, `XB_PENDING_SAFEPOINT`). Compiled code tests the
field after each fallible call; when it is set the code calls `xb_pending`, which serves pauses (a safepoint) and
returns non-zero when the function must unwind: to a `BEGIN SEQUENCE` handler of the function, or to its caller
([xir.md](xir.md#control-flow-for-errors-and-exceptions)). The check stays valid across `AWAIT`. `QUIT` raises
`XB_PENDING_QUIT` with exit code 0.

A runtime error (`core/error.c`, `xb_error` and `xb_arg_error` in `core/error.h`) makes an `Error` object (see [Objects
and messages](#objects-and-messages)): subsystem `BASE`, the subcode, the generic code (`error.ch`, shared with xBase
code like `set.ch`) and its description, the operation, severity `ES_ERROR`, `canSubstitute`, and in `args` the operands
of an operator or the arguments of a function (NIL when there are none). Then it evaluates the context's error block
with it (`ErrorBlock( [bNew] )` returns the block and replaces it with a codeblock argument; the block and the `BREAK`
value are roots of the collector). With no error block (a program's `ErrorSys()` set none) the error is unrecoverable:
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
zero gives 0 (as Clipper's default handler does); any other error prints
`Error <subsystem>/<subcode>  <description>: <operation>` (the file name instead of the operation when there is one,
then ` (DOS Error <osCode>)` when there is a system error code; `Warning` for a severity below `ES_ERROR`), then a line
`Called from NAME(line)  ` for each level of the call stack (see [Frames](#frames); the handler's own frames are
hidden, so level 0 is where the error happened), to standard error (`OutErr()`), and ends the program with
`ErrorLevel( 1 )` and `QUIT`. The choice between quitting, retrying and the default (Clipper's `Alert()`) waits for the
terminal layer. An unrecoverable error lists the levels as `Called from NAME(line) in FILE` (without ` in FILE` for a
runtime function).

`Break( [xValue] )` (and the `BREAK [value]` command, `__Break`) stores the value in the context and raises
`XB_PENDING_BREAK`. In a `BEGIN SEQUENCE` handler, `xb_seq_catch` takes a `BREAK` that is the only request pending,
and `xb_seq_value` moves the value out (`RECOVER USING`). With no sequence active the `BREAK` reaches the start
function, and the program ends quietly with exit code 0.

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
element (taking the variable copies the reference, so the base keeps it), and `xb_each_leave` restores the saved value;
for a `STATIC` the compiled code tests for a cell at each call, since a reference taken during the loop designates the
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
- `Round` rounds half away from zero; a negative number of decimals rounds to tens, hundreds, ... As in Clipper, the
  half is added to ten times the scaled value and the result, divided back, is multiplied by `1 + DBL_EPSILON`
  (about one unit in the last place) before truncating, so decimal halves stored just below `.5` (`2.675`) round up
  while `0.4999999999` rounds down. That product is rounded once (`fma`), so a floating-point unit that computes with
  more bits (x87) gets the same result. A scaling that overflows gives a non-finite result (shown as stars). `Int`
  truncates without that adjustment.
- `Sqrt` of a negative number and an `Exp` that overflows give 0; `Log` of a negative number gives minus infinity, like
  `Log( 0 )`.
- `Replicate` raises `String overflow` (1234) when the result size overflows.
- `StrTran( c, cSearch, cReplace, nStart, nCount )`: `nStart` or `nCount` of 0 gives `""`; a negative `nStart`
  replaces nothing, a negative `nCount` replaces every occurrence from `nStart`.
- `Stuff( c, nStart, nDelete, cInsert )`: `nStart` 0 is 1; a negative `nStart` or `nDelete` counts as past the end.
- Trimming from the left, `Val` and `Empty` skip blanks, tabs, carriage returns and line feeds (`xb_is_blank`);
  trimming from the right removes spaces only. Case changes affect ASCII letters only.
- `Empty` is true for `NIL`, `0`, a string of blanks (as above), `.F.`, an empty array and the empty date.
- `Set( n [, x] )` returns the setting before the call. `_SET_EXACT` takes a logical or a string starting with
  `ON`/`OFF`; `_SET_DECIMALS` and `_SET_EPOCH` take a number of 0 or more (a negative one is error 2020);
  `_SET_DATEFORMAT` takes a string (kept whole) and sets `SET CENTURY` on when its first run of `Y`s has 4 or more,
  off otherwise. `__SetCentury( [lOn] )` rewrites that run as `YYYY` or `YY` (or puts it in front when the picture
  has no `Y`) and the picture in upper case. Other values change nothing.
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
  comparison: without `bOrder`, a range of integers only or of doubles only is sorted comparing them in place). `bOrder(
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
  operand counts and codes are the table `xb_bit_function` in `xbrt.h`, which the compiler reads too.
- `Seconds()` gives the seconds since midnight, local time, to the millisecond, with SET DECIMALS decimals;
  `Time()` the local time as `"hh:mm:ss"`. `ElapsedMs()`, an xbase function, gives the milliseconds since the
  program started from a monotonic clock (`xb_platform_elapsed_ms`), for timing: the difference of two calls.

Settings live in the task context (start: `SET EXACT OFF`, `SET DECIMALS TO 2`, date format `mm/dd/yy`,
`SET EPOCH TO 1900`, `SET CENTURY OFF`). Their numbers come from `include/set.ch`, shared with xBase code.

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
- Primitives for them: `__ConsoleWrite( xValue )` writes the display form of a value ([Display](#display)) to the
  console (standard output until the terminal layer exists), `__StdWrite( nHandle, xValue )` to the standard output
  (1) or error (2), flushing the output before writing to the error; `__ArgError( nSubCode )` raises the calling
  function's argument error (`EG_ARG`, its name as the operation, the arguments it received; not substitutable), as
  its C counterparts do.
- Written in xBase: `QOut`, `QQOut`, `OutStd` and `OutErr` (`rtl/console.prg`), `AEval` (`rtl/arrays.prg`),
  `ErrorSys` and the default error handler (`rtl/errorsys.prg`). Measured against their C versions (callgrind,
  x86-64): 200 000 `? i, "abc", .T.` take 267M instructions (220M in C; about 6% more time to `/dev/null`); `AEval`
  over a million elements with `{| x | n += x }`, 121M (116M). `QQOut` keeps its own loop instead of sharing
  `OutStd`'s: passing the values on through one more call costs `QOut` 13% more cycles.

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
- `CDoW`/`CMonth` give English names (`""` for the empty date); `DoW` counts from Sunday = 1. `DoW` works from the
  day number (0 outside the calendar); `CDoW`, as in Clipper, applies the Gregorian weekday formula to the decoded
  year, month and day, so it differs from `DoW` in year 0 and gives `Wednesday` for day numbers outside the calendar.

## Files

The runtime reaches files through the platform layer (`platform/platform.h`): `xb_platform_file_open` (an existing
file, for reading), `xb_platform_file_create` (a new or emptied file, for writing), `xb_platform_file_read`,
`xb_platform_file_write` and `xb_platform_file_close`, on the system's handle as an integer, with paths in UTF-8
(converted to UTF-16 on Windows). A failure gives the code
`FError()` reports: a DOS error number (2 file not found, 3 path not found, 5 access denied, ...), which Windows
reports itself and the POSIX build translates `errno` values to (31, general failure, for those without one).
`xb_platform_path_separator` tells the characters that separate directories (`/`; also `\` and `:` on Windows).
Binary fields of the file formats go through the little-endian helpers of `core/bytes.h`. `SAVE` and `RESTORE`
([Memory variables](#memory-variables)) use this layer; the file functions of the language (`FOpen()`, ...) are not
written yet.

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
in binary mode so programs write the same bytes everywhere; everywhere, the start of the `ElapsedMs` clock) and takes
the command-line arguments in UTF-8 (`xb_platform_args`: as `main` received them on POSIX systems, converted from the
wide command line on Windows). Then, on the program's thread, it creates the context, makes the arguments after the
program's name strings in a frame of its own linked into the context's chain (so the collector sees them), calls
`xb_app_main( ctx, argc, argv )` (emitted by the compiler) with them, flushes output and returns the exit code.
`xb_app_main` checks the ABI and calls `xb_program_run`, which runs, in this order:

1. the modules' `STATIC` initializations, in registration order;
2. `ErrorSys()`, found by name (the program's own, else the runtime's), which installs the error block
   ([Errors and pending requests](#errors-and-pending-requests));
3. the `INIT` procedures, module by module in registration order and, within a module, from the last one written to
   the first (as the reference does), each with the command-line arguments;
4. the start function, with the arguments, as Clipper passes them: strings, `NIL` for a parameter without one, and
   arguments past the parameters passed too;
5. the `EXIT` procedures, module by module, each module's in the order written, without arguments.

A request to unwind (a `QUIT`, a `BREAK` with no sequence, an error that ended the program) stops steps 1 to 4; the
request is then dropped (with a `BREAK` value) and step 5 runs, a new request stopping it. The exit code is kept
throughout: `ErrorLevel( [nNew] )` returns it and sets it from an argument, a number wrapped to 32 bits, anything else
0 (the default error handler sets 1). Registration order follows the global constructors,
which is the link order on the usual toolchains. Inside an `EXIT` procedure the call stack starts with it (the
reference shows the function that ended the program, such as `__QUIT`, under it).
