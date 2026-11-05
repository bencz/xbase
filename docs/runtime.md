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

- Types: `NIL`, `LOGICAL`, `INT` (64-bit), `DOUBLE`, `STRING`. An all-zero item is `NIL`.
- Copying an item is a plain memory copy; there is no reference counting.
- Strings (`xb_str`: flags, hash, length, bytes) are immutable and NUL-terminated, so they can be shared between items
  and threads and passed to C. Literals emitted by the compiler carry `XB_STR_STATIC`.
- Strings created at run time are never freed yet; the tracing GC will reclaim them.

## Frames

Parameters, `LOCAL`s and temporaries live in item frames taken from a per-context segmented stack
(`xb_frame_push`/`xb_frame_pop`). Segments are never reallocated, so a frame's address is stable for its lifetime; new
frames start as `NIL`. Frames will be the GC roots.

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
`XB_PENDING_QUIT`); compiled code tests the field after each fallible call and unwinds to its caller. The same check
will serve GC safepoints and stays valid across `AWAIT`.

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

Comparing `NIL` with `=`/`==`/`!=` is allowed; other type mismatches raise `BASE` argument errors.

`FOR` uses `xb_for_test`: a non-negative step loops while the counter is not greater than the limit, a negative step
while it is not less; operands that cannot be compared raise the `>` or `<` argument error, a non-numeric step the
`<` error.

Integer multiplication is exact over the whole 64-bit range; Harbour computes it in floating point and loses digits
above 2^53.

## Display

`xb_item_display` formats values the way `?`/`QOut()` print them: `NIL`, `.T.`/`.F.`, strings as they are, and
numbers through `xb_num_to_str` (`runtime/src/numfmt.c`):

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
| `ValType( x )` | `xbf_VALTYPE` | `U`, `L`, `N`, `C` |
| `Len( c )` | `xbf_LEN` | strings only for now |
| `__Quit()` | `xbf___QUIT` | target of `QUIT`/`CANCEL` |

## Program startup

`main` (in `runtime/src/main.c`) runs the platform setup (`xb_platform_init`: on Windows, standard output and error
in binary mode so programs write the same bytes everywhere), creates the context, calls `xb_app_main` (emitted by the
compiler), flushes output and returns the exit code.
