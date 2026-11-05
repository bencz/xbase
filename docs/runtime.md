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

- Types: `NIL`, `LOGICAL`, `INT` (64-bit), `DOUBLE`, `STRING`, `ARRAY`, `BLOCK`, and `CELL`, which only appears in
  the frame slot of a variable a codeblock captured. An all-zero item is `NIL`.
- Copying an item is a plain memory copy; there is no reference counting.
- Strings (`xb_str`: flags, hash, length, bytes) are immutable and NUL-terminated, so they can be shared between items
  and threads and passed to C. Literals emitted by the compiler carry `XB_STR_STATIC` and are not heap objects.
- Arrays (`types/array.c`, functions in `rtl/arrays.c`) are 1-based. Their items start in the array's own allocation and
  move to a separate heap object when the array grows past it.
- Codeblocks (`xb_block`: function, captured cells) and cells (`xb_cell`: one item at offset 0) are described in
  [Codeblocks](#codeblocks).

## Frames

Parameters, `LOCAL`s and temporaries live in item frames taken from a per-context segmented stack
(`xb_frame_push`/`xb_frame_pop`). Segments are never reallocated, so a frame's address is stable for its lifetime; new
frames start as `NIL`. Frames are GC roots.

## Garbage collector

`runtime/src/gc/gc.c` is a precise, non-moving mark and lazy-sweep collector. Strings, arrays, array item storage,
codeblocks and cells are its objects; an 8-byte header (size, kind, mark) precedes each one, so object pointers are
what the rest of the runtime and compiled code use.

- Allocation: objects up to 2048 bytes (header included) come from 64 KB pages of 27 size classes. Every thread has a
  local heap with one page per class and allocates from its free list without locking. Larger objects are allocated
  individually.
- Roots: the frames and temporary C roots (`xb_gc_root_push`) of every registered context, which includes suspended
  tasks.
- Collection: starts when the bytes allocated since the last one reach the live heap size (at least 8 MB). Marking
  uses an explicit stack. Large objects are freed at once; pages are swept lazily when an allocation needs one (the
  search resumes where the previous one stopped), and a page found empty at a collection returns to a pool of free
  pages. Pages are not returned to the operating system.
- Frame temporaries are not cleared after each statement, so a value a temporary held stays alive until its slot is
  reused or the function returns.
- Sizes that cannot be allocated (an array longer than `XB_ARRAY_MAX_LEN`, memory exhausted) end the program through
  `xb_gc_fatal` with a message on standard error and exit code 1.
- Rule for C code: a collection can run inside any allocation, so every object still needed must be reachable from a
  frame or a C root before the next allocation. Objects never move.
- Threads: global structures are protected by a lock (`xb_mutex`, platform layer); the allocation fast path takes
  none. A collection requires every mutator to be stopped at a safepoint (`XB_PENDING_SAFEPOINT`); while tasks run on
  one thread the allocating thread is the only mutator.

`__GcCollect()` runs a collection and returns the number of live objects (tests use it).

## Codeblocks

A `LOCAL` or parameter that a codeblock uses is *boxed*: at function entry `xb_cell_box` moves it into a cell and
leaves an `XB_T_CELL` item in its slot. The function and every block that captured it share the cell, which outlives
the function. A codeblock is a compiled function `(ctx, ret, argc, argv, self)` plus the captured cells
(`self->cells[]`); `xb_block_new` creates it and the compiled code stores the cells. `Eval( bBlock, ... )` and
`xb_block_call` run it; the caller sets the result to `NIL` first.

## Calling convention

Every xBase-callable function, compiled or written in C, has the dynamic convention:

```c
void xbf_NAME(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
```

- `NAME` is the upper-cased xBase name.
- The caller evaluates the arguments into consecutive slots of its own frame and sets `ret` to `NIL`; a callee that
  returns nothing leaves it untouched. The callee copies its parameters into its frame (`xb_args_copy`); missing
  arguments are `NIL`.
- Every 32-bit parameter of a runtime entry point is `uint32_t` and every 32-bit result `int32_t`; the compiler
  relies on this to apply the target's argument-extension rules.

## Errors and pending requests

`xb_ctx` starts with `uint32_t pending`. The runtime sets bits there (`XB_PENDING_ERROR`, `XB_PENDING_BREAK`,
`XB_PENDING_QUIT`, `XB_PENDING_SAFEPOINT`). Compiled code tests the field after each fallible call; when it is set the
code calls `xb_pending`, which serves pauses (a safepoint) and returns non-zero when the function must unwind to its
caller. The check stays valid across `AWAIT`.

Runtime errors are currently fatal: `xb_rt_error` prints `Error <subsystem>/<code>  <description>: <operation>` to
standard error, sets exit code 1 and raises `XB_PENDING_ERROR`. `QUIT` raises `XB_PENDING_QUIT` with exit code 0.

## Operators

`xb_op_*(ctx, result, a, b)`; the result may alias either operand.

| Operation | Behavior |
|---|---|
| `+` `-` on integers | 64-bit; on overflow a double with 0 decimals |
| `*` on integers | 64-bit, exact; on overflow a double with `SET DECIMALS` decimals |
| `+` `-` `*` on numbers | double; decimals: the larger of both for `+`/`-`, the sum for `*` |
| `/` | always a double with `SET DECIMALS` (2) decimals; zero divisor is an error |
| `%` | always a double with `SET DECIMALS` decimals (remainder of the integers, or `fmod`); zero divisor is an error |
| `^` | double with `SET DECIMALS` decimals |
| `+` on strings | concatenation |
| `-` on strings | concatenation moving the left operand's trailing blanks to the end |
| `=` on strings | with `SET EXACT OFF`, compares only the right operand's length (`"abc" = "ab"` is true) |
| `==` | exact comparison; on strings byte-for-byte |
| `<` `<=` `>` `>=` `!=` | numbers, strings (same rule as `=`), logicals (`.F.` < `.T.`) |
| `$` | substring test; an empty left operand is false |
| `.NOT.` | logicals only |

Comparing `NIL` with `=`/`==`/`!=` is allowed; other type mismatches raise `BASE` argument errors. `==` on two arrays
or two codeblocks tests identity; other comparisons of them are argument errors.

Element access `a[i]` (`xb_op_index`) and assignment (`xb_op_index_set`) raise `Argument error: array access` (1068)
or `array assign` (1069) when the base is not an array or the index not a number, and `Bound error` (1132/1133)
outside `1 .. Len( a )`.

`xb_num_default_dec(ctx, r, value)` makes a double with SET DECIMALS decimals; the compiler uses it for folded
expressions whose decimals are decided at run time. Items keep 16 bits of SET DECIMALS, as in Clipper.

`FOR` uses `xb_for_test`: a non-negative step loops while the counter is not greater than the limit, a negative step
while it is not less; operands that cannot be compared raise the `>` or `<` argument error, a non-numeric step the
`<` error.

Integer multiplication is exact over the whole 64-bit range; Harbour computes it in floating point and loses digits
above 2^53.

## Display

`xb_item_display` formats values the way `?`/`QOut()` print them: `NIL`, `.T.`/`.F.`, strings as they are, arrays
and codeblocks as nothing, and
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

| xBase | C symbol | Notes |
|---|---|---|
| `QOut( ... )` | `xbf_QOUT` | newline, then the arguments separated by a blank |
| `QQOut( ... )` | `xbf_QQOUT` | the arguments without a newline |
| `ValType( x )` | `xbf_VALTYPE` | `U`, `L`, `N`, `C`, `A`, `B` |
| `Len( x )` | `xbf_LEN` | strings and arrays |
| `Array( n1 [, n2 ...] )` | `xbf_ARRAY` | nested arrays of `NIL`; a non-numeric argument returns `NIL`, a negative one is error 1131 |
| `AAdd( a, x )` | `xbf_AADD` | appends and returns `x` |
| `ASize( a, n )` | `xbf_ASIZE` | resizes (new items are `NIL`) and returns `a` |
| `ATail( a )` | `xbf_ATAIL` | the last item, `NIL` for an empty array |
| `AClone( a )` | `xbf_ACLONE` | copies the array and its nested arrays; an array reached twice is copied once, so shared and cyclic structures are kept |
| `Eval( b [, args...] )` | `xbf_EVAL` | runs a codeblock |
| `__Quit()` | `xbf___QUIT` | target of `QUIT`/`CANCEL` |
| `Set( n [, x] )` | `xbf_SET` | returns the setting before the call. `_SET_EXACT`: a logical, or a string starting with `ON`/`OFF` in any case; other values change nothing. `_SET_DECIMALS`: a number of 0 or more (a negative one is error 2020); other types change nothing. |

Settings live in the task context (`SET EXACT` off and `SET DECIMALS` 2 at start). Their numbers come from
`include/set.ch`, shared with xBase code.

## Program startup

`main` (in `runtime/src/core/main.c`) runs the platform setup (`xb_platform_init`: on Windows, standard output and error
in binary mode so programs write the same bytes everywhere), creates the context, calls `xb_app_main` (emitted by the
compiler), flushes output and returns the exit code.
