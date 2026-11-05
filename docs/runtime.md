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

- Types: `NIL`, `LOGICAL`, `INT` (64-bit), `DOUBLE`, `STRING`, `ARRAY`, `BLOCK`, `DATE`, and two kinds the language
  never shows, which designate another item (see [References](#references)): `CELL`, a variable in a cell, and `REF`,
  an element. An all-zero item is `NIL`.
- Copying an item is a plain memory copy; there is no reference counting.
- Strings (`xb_str`: flags, hash, length, bytes) are immutable and NUL-terminated, so they can be shared between items
  and threads and passed to C. Literals emitted by the compiler carry `XB_STR_STATIC` and are not heap objects.
- Arrays (`types/array.c`, functions in `rtl/arrays.c`) are 1-based. Their items start in the array's own allocation and
  move to a separate heap object when the array grows past it.
- Codeblocks (`xb_block`: function, captured cells) and cells (`xb_cell`: one item at offset 0) are described in
  [Codeblocks](#codeblocks).

## Frames

Parameters, `LOCAL`s and temporaries of a compiled function live in its item frame (`xb_frame`: link, slot count,
items), an `alloca` on the machine stack. The function links it into its context's chain (`xb_ctx.frames`) at entry,
with the parameters copied from `argv` (`NIL` past `argc`) and the other slots `NIL`, and unlinks it at exit; every
exit goes through one block, and errors return normally (the pending flag), so the chain is always right. The
collector scans the frames of every context's chain. A frame dies with its function: nothing keeps a pointer to a slot
(a variable that outlives it moves into a heap cell first). The program runs on a thread whose stack is reserved at
256 MB (`XB_PROGRAM_STACK_SIZE` in `core/main.c`; pages are committed as used), so deep recursion has room; running
past it is not detected yet and ends the process.

## Garbage collector

`runtime/src/gc/gc.c` is a precise, non-moving, generational mark and lazy-sweep collector. Strings, arrays, array
item storage, codeblocks, cells and reference targets are its objects; an 8-byte header (size, kind, state) precedes
each one, so object pointers are what the rest of the runtime and compiled code use.

- Allocation: objects up to 2048 bytes (header included) come from 64 KB pages of 27 size classes. Every thread has a
  local heap with one page per class and allocates from its free list without locking. Larger objects are allocated
  individually.
- Roots: the frames and temporary C roots (`xb_gc_root_push`) of every registered context, and global item arrays
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
  calls `xb_gc_write(obj)` after storing into a heap object (a grown array's item storage, a reference's character)
  and `xb_gc_write_item(array, index)` after storing an array element; compiled code tests the byte inline after
  storing into a captured variable's cell and calls `xb_gc_remember`, and calls `xb_ref_written` after a store
  through a reference (a `FOR EACH` variable, a parameter passed by reference). The barrier comes after the store,
  with no allocation in between, and an item a call produces never goes straight into a heap object: it goes through
  a frame slot first (a root). A recorded object is traced by the next young collection and leaves the set.
- Card marking: array storage for 1024 items or more carries one card byte per 64 items after its last item. A store
  into such an old array marks the element's card and records the array for its cards only; the young collection
  scans the marked cards (eight card bytes per step) instead of the whole array. Storage that is replaced (the array
  grew) records the whole array.
- Collection: marking uses an explicit stack. Large objects are freed at once. After a young collection pages are
  swept lazily when an allocation needs one (the search resumes where the previous one stopped), and only pages that
  received objects since their last sweep need it, since old objects do not die there. A full collection sweeps
  every page at once and gives pages whose objects all died back to a pool shared by all size classes. Pages are not
  returned to the operating system.
- Frame temporaries are not cleared after each statement, so a value a temporary held stays alive until its slot is
  reused or the function returns.
- Sizes that cannot be allocated (an array longer than `XB_ARRAY_MAX_LEN`, an object above half the address space,
  memory exhausted), more temporary roots than `XB_GC_MAX_ROOTS`, a lock that cannot be created and an ABI mismatch
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

## Codeblocks

A variable that a codeblock captures, or that is passed by reference, moves into a cell when that first happens: its
value moves into a new cell and an `XB_T_CELL` item is left in its slot or `STATIC` storage (a parameter passed by
reference already holds its caller's). `xb_cell_of` returns the cell a codeblock captures; for a `LOCAL` or
parameter that holds the reference of a `FOR EACH` loop it returns a new cell holding the current element, and the
variable stays the loop's. The function and every block
that captured it share the cell, which outlives the function. A codeblock is a compiled function
`(ctx, ret, argc, argv, self)` plus the captured cells (`self->cells[]`); `xb_block_new` creates it and the compiled
code stores the cells. `Eval( bBlock, ... )` and `xb_block_call` run it; the caller sets the result to `NIL` first.

## References

An item that designates another item is an `XB_T_CELL` (a variable in a cell: `v.p` is the `xb_cell`) or an `XB_T_REF`
(an element: `v.p` is an `xb_ref` holding the collection, the 1-based index, and for a string the current character).
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
  cell, the array, or the reference itself for a string's character.

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
- The caller evaluates the arguments into consecutive slots of its own frame and sets `ret` to `NIL`; a callee that
  returns nothing leaves it untouched. The callee copies its parameters into its frame; missing arguments are
  `NIL`. An argument passed by reference is an `XB_T_CELL` or `XB_T_REF` item (see
  [References](#references)); a C function receives one only where its declaration says `@`.
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

`FOR EACH` (`core/control.c`) enumerates arrays (their items) and strings (their characters as one-character strings);
`xb_enum_len` raises `Argument error: array access` (1068) for any other collection. Indexes are 1-based. The loop
variable holds an `XB_T_REF` item (see [References](#references)); for a string the reference holds a copy of the
character, so assigning it leaves the string unchanged. The loop works on a base item: the variable's cell when it is
shared at entry (`xb_each_shared`; a parameter holding an element passed by reference moves into a cell then), else
the variable's own item. `xb_each_enter` saves the base's value and puts the loop's reference there, `xb_each_move`
points it at the next element (taking the variable copies the reference, so the base keeps it), and `xb_each_leave`
restores the saved value; for a `STATIC` the compiled code tests for a cell at each call, since a reference taken
during the loop designates the `STATIC` itself. `SWITCH` uses `xb_switch_check`, which raises
`Argument error: SWITCH` (3104) for a value that is not an integer or a string, and `xb_switch_match`.

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

The functions of the language live in `runtime/src/rtl/`, by area; [coverage.md](coverage.md) lists which exist.
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
  half is added to ten times the scaled value and the result raised by one unit in the last place before truncating,
  so decimal halves stored just below `.5` (`2.675`) round up while `0.4999999999` rounds down. A scaling that
  overflows gives a non-finite result (shown as stars). `Int` truncates without that adjustment.
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
- `ASort( a, [nStart], [nCount], [bOrder] )` sorts in place and returns the array (`NIL` for anything else). `nStart`
  0 or missing is 1, one past the end or negative sorts nothing; `nCount` missing, 0, negative or reaching past the
  end means the rest. The sort is stable (a merge sort of positions: equal elements keep their order).
  `bOrder( x, y )` says whether `x` goes first (a number: whether it is not 0; any other value: yes); without it,
  values of one kind compare like the operators (strings follow SET EXACT, integers exactly, dates by day, `.F.`
  before `.T.`) and other pairs by kind: array, block, string, logical, date, number, `NIL`. The block's arguments are
  kept as roots while it runs, and moving the elements goes through the write barrier.
- The bit functions, xbase's own: `BitAnd`, `BitOr`, `BitXor`, `BitNor` (two numbers), `BitNot` (one), `BitShl` and
  `BitShr` (a number and a shift count) work on the 64-bit two's complement form of integers (`xb_bit_apply` in
  `xbrt.h`): numbers are truncated to integers (saturated; NaN is 0), `BitShr` is a logical shift and a count
  outside 0..63 gives 0. Anything else than a number is an argument error. xbase's own functions have `BASE` codes
  from 9001 (`BitAnd` 9001, `BitOr` 9002, `BitXor` 9003, `BitNor` 9004, `BitNot` 9005, `BitShl` 9006, `BitShr` 9007);
  the names, argument counts and codes are the table `xb_bit_function` in `xbrt.h`, which the compiler reads too.
- `Seconds()` gives the seconds since midnight, local time, to the millisecond, with SET DECIMALS decimals;
  `Time()` the local time as `"hh:mm:ss"`. `ElapsedMs()`, an xbase function, gives the milliseconds since the
  program started from a monotonic clock (`xb_platform_elapsed_ms`), for timing: the difference of two calls.

Settings live in the task context (start: `SET EXACT OFF`, `SET DECIMALS TO 2`, date format `mm/dd/yy`,
`SET EPOCH TO 1900`, `SET CENTURY OFF`). Their numbers come from `include/set.ch`, shared with xBase code.

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

## Program startup

Before `main`, the global constructor of every module with `STATIC`s calls `xb_module_register( init, statics, n )`
(`core/module.c`), which records the module's initialization function and registers its statics as a GC root.

`main` (in `runtime/src/core/main.c`) runs the platform setup (`xb_platform_init`: on Windows, standard output and error
in binary mode so programs write the same bytes everywhere; everywhere, the start of the `ElapsedMs` clock), then on the
program's thread creates the context, calls `xb_app_main` (emitted by the compiler), flushes output and returns the exit
code. `xb_app_main` checks the ABI, runs the registered module initializations in registration order (`xb_modules_init`,
which stops at the first error) and calls the start function.
