/*
 * xbrt - xbase runtime: public ABI shared with the xbc compiler.
 *
 * Everything in this header that the compiler mirrors is marked [ABI].
 * The compiler never hard-codes sizes or offsets: it describes these
 * structures as LLVM types and lets the target DataLayout lay them out,
 * exactly as the C compiler does for the runtime. At startup the program's
 * entry (xb_app_main) calls xb_abi_check() so a mismatch is caught instead of
 * corrupting memory.
 *
 * [ABI] Every 32-bit parameter of an entry point used by compiled code is
 * uint32_t and every 32-bit result is int32_t: the compiler relies on this to
 * apply the target's argument-extension rules.
 *
 * Endianness rule: in-memory structures are always native; on-disk formats
 * (DBF, NTX, CDX, ...) are only ever read/written through explicit
 * little/big-endian helpers, never by copying structs.
 */
#ifndef XBRT_H
#define XBRT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XB_ABI_VERSION 11u

/* [ABI] item type tags */
typedef enum xb_type {
  XB_T_NIL     = 0,
  XB_T_LOGICAL = 1,
  XB_T_INT     = 2,   /* numeric, integral (int64)        */
  XB_T_DOUBLE  = 3,   /* numeric, floating point           */
  XB_T_STRING  = 4,
  XB_T_ARRAY   = 5,   /* v.p: runtime array                       */
  XB_T_BLOCK   = 6,   /* v.p: xb_block                             */
  XB_T_CELL    = 7,   /* v.p: xb_cell; in the frame slot of a variable that
                         lives in a cell (captured by a codeblock, or passed
                         by reference), and as an argument passed by
                         reference: that variable */
  XB_T_REF     = 8,   /* v.p: a reference to an element (the variable of a
                         FOR EACH loop, an argument @a[i]) */
  XB_T_OBJECT  = 9,   /* v.p: an object (an instance of a class)   */
  XB_T_DATE    = 10,  /* v.i: Julian day number, 0 for the empty date  */
  XB_T_COUNT_
} xb_type;

/* [ABI] The tags whose v.p is a heap object form one range, so that one
 * unsigned comparison tells whether storing an item can make an old object
 * refer to a young one (the write barrier is needed only then). */
#define XB_T_HEAP_FIRST XB_T_STRING
#define XB_T_HEAP_LAST  XB_T_OBJECT
static inline int xb_type_is_heap(uint32_t type) {
  return type - (uint32_t)XB_T_HEAP_FIRST <= (uint32_t)(XB_T_HEAP_LAST - XB_T_HEAP_FIRST);
}

/* [ABI] string header. Strings are immutable: they can be shared freely
 * between items and threads. `data` is always NUL terminated (len excludes
 * the terminator) so it can be handed to C without copying. */
#define XB_STR_STATIC 0x1u   /* emitted by the compiler, never freed */

typedef struct xb_str xb_str;
#ifndef __cplusplus
/* The compiler mirrors this layout as an LLVM type and never uses the C++ view
 * of it (a flexible array member is not standard C++). */
struct xb_str {
  uint32_t flags;
  uint32_t hash;
  size_t   len;
  char     data[];
};
#endif

/* [ABI] the dynamic value. Copying an item is a plain memcpy: memory is
 * managed by a tracing GC, so there is no reference counting. */
typedef struct xb_item {
  uint32_t type;      /* xb_type                                    */
  uint16_t width;     /* numeric display width (0 = default)        */
  uint16_t dec;       /* numeric decimals                           */
  union {
    int32_t  l;       /* XB_T_LOGICAL                               */
    int64_t  i;       /* XB_T_INT                                   */
    double   d;       /* XB_T_DOUBLE                                */
    xb_str  *s;       /* XB_T_STRING                                */
    void    *p;
  } v;
} xb_item;

/* [ABI] `pending` is tested by compiled code after every call that can
 * fail. When it is non-zero the code calls xb_pending(), which serves
 * requests that only need a pause (a GC safepoint) and returns non-zero when
 * the function must unwind (a BREAK or a QUIT; an error that is not recovered
 * ends in one). The common case costs one
 * load and one predictable branch, works on every target, and survives AWAIT
 * (unlike setjmp/longjmp). */
enum {
  XB_PENDING_NONE      = 0,
  XB_PENDING_BREAK     = 1u << 1,
  XB_PENDING_QUIT      = 1u << 2,
  XB_PENDING_SAFEPOINT = 1u << 3,
};
#define XB_PENDING_UNWIND (XB_PENDING_BREAK | XB_PENDING_QUIT)

/* [ABI] An array: `len` items at `items`, which start in the same
 * allocation (`inline_items`); when it grows past that, they move to an
 * object of their own. Compiled code reads `len` and `items` to access
 * elements in place. */
typedef struct xb_array xb_array;
#ifndef __cplusplus
struct xb_array {
  size_t   len;
  size_t   cap;
  xb_item *items;
  xb_item  inline_items[];
};
#endif

/* [ABI] A compiled function's item frame (parameters, LOCALs, temporaries):
 * on the machine stack, linked into its context's chain while the function
 * runs. The collector scans the frames of every chain; slots are NIL or valid
 * items whenever something can allocate. */
typedef struct xb_frame xb_frame;
#ifndef __cplusplus
struct xb_frame {
  xb_frame *prev;
  uint32_t nslots;
  xb_item slots[];
};
#endif

/* [ABI] Per-task context. Compiled code uses its first fields, which the
 * context starts with in this order: `uint32_t pending` (requests tested after
 * every operation that can fail), `int32_t exitcode`, `xb_frame *frames` (the
 * innermost frame of the chain), `int32_t set_decimals` (SET DECIMALS, the
 * decimals of `/` and `%`, as 16 bits). */
typedef struct xb_ctx xb_ctx;

/* [ABI] dynamic calling convention used by every xBase-callable function:
 * arguments are an array of items owned by the caller, the result is
 * written to `ret`, which the caller sets to NIL before the call. */
typedef void (*xb_func)(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

/* [ABI] Symbols of xBase-callable functions: the upper-cased name with a
 * prefix. The runtime's functions (declared with EXTERN FUNCTION) and those of
 * the program get different prefixes, so a program may define a function of
 * the same name as a runtime one that is not RESERVED without a clash at link
 * time. */
#define XB_RUNTIME_FUNC_PREFIX "xbf_"
#define XB_PROGRAM_FUNC_PREFIX "xbp_"

/* [ABI] A variable captured by a codeblock lives in a cell (value at offset 0)
 * shared by the function and its blocks. */
typedef struct xb_cell {
  xb_item value;
} xb_cell;

/* [ABI] A codeblock: its function and the cells of the variables it captured.
 * The block function receives the block itself. */
typedef struct xb_block xb_block;
typedef void (*xb_block_fn)(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, xb_block *self);
#ifndef __cplusplus
struct xb_block {
  xb_block_fn fn;
  uint32_t ncells;
  xb_cell *cells[];
};
#endif

int32_t  xb_pending(xb_ctx *ctx);

/* The layout the module was compiled for: offsets of xb_str.data,
 * xb_block.cells, xb_frame.slots, xb_ctx.frames, xb_ctx.set_decimals and
 * xb_array.items (after len and cap). */
void     xb_abi_check(uint32_t version, uint32_t item_size, uint32_t str_hdr_size, uint32_t block_cells_offset,
                      uint32_t frame_slots_offset, uint32_t ctx_frames_offset, uint32_t ctx_decimals_offset,
                      uint32_t array_items_offset);

void     xb_op_add(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_sub(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_mul(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_div(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_mod(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_pow(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_neg(xb_ctx *ctx, xb_item *r, const xb_item *a);
void     xb_op_eq(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);      /* =  */
void     xb_op_exacteq(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b); /* == */
void     xb_op_ne(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_lt(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_le(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_gt(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_ge(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_in(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);      /* $  */
void     xb_op_not(xb_ctx *ctx, xb_item *r, const xb_item *a);

/* A double whose decimals are the current SET DECIMALS (constant-folded `/`,
 * `%` on doubles and `^`). */
void     xb_num_default_dec(xb_ctx *ctx, xb_item *r, double value);

/* Non-logical value used as a condition: raises the error, returns 0. */
int32_t  xb_cond_error(xb_ctx *ctx, const xb_item *a);

/* Arrays (1-based). xb_array_new copies `n` items (consecutive frame slots). */
void     xb_array_new(xb_ctx *ctx, xb_item *r, uint32_t n, const xb_item *elems);
void     xb_op_index(xb_ctx *ctx, xb_item *r, const xb_item *base, const xb_item *idx);
void     xb_op_index_set(xb_ctx *ctx, const xb_item *base, const xb_item *idx, const xb_item *value);
/* Sends `message` (upper-cased; "_NAME" assigns the variable NAME) to argv[0]
 * with the arguments argv[1 .. argc-1]; argc is at least 1. */
void     xb_op_send(xb_ctx *ctx, xb_item *r, const char *message, uint32_t argc, xb_item *argv);

/* Write barrier after storing into element `index` of an array: for a heap
 * value in an old array, a large array records the element's card, another
 * the whole array. Compiled code that stores an element in place calls it for
 * heap values only. */
void     xb_gc_write_item(xb_array *a, size_t index);

/* References. An item that designates another item is an XB_T_CELL (a
 * variable in a cell) or an XB_T_REF (an element); xBase code never sees one,
 * compiled code goes through it. The tags are consecutive, so compiled code
 * tests for both with one unsigned comparison.
 * A variable's own item is its frame slot or STATIC storage; it moves into a
 * cell when it is first shared (a LOCAL or parameter captured by a codeblock,
 * any variable passed by reference). A LOCAL or parameter that holds the
 * reference of a FOR EACH loop is not moved: whoever takes it gets the
 * current element, and the variable stays the loop's.
 * xb_cell_of returns the cell a codeblock captures for the frame variable
 * whose own item is `var`; xb_ref_of stores in `r` the reference an argument
 * @var passes (`is_static`: a STATIC, always the variable itself);
 * xb_ref_elem the one @base[idx] passes, with the checks of xb_op_index.
 * A cell may hold an XB_T_REF (a FOR EACH variable); an element never holds
 * either. xb_ref_target returns the item `r` designates in the end (a bound
 * error if the array shrank); xb_ref_written is the write barrier after a
 * store there. */
xb_cell *xb_cell_of(xb_ctx *ctx, xb_item *var);
xb_item *xb_ref_target(xb_ctx *ctx, xb_item *r);
void     xb_ref_written(const xb_item *r);
void     xb_ref_of(xb_ctx *ctx, xb_item *r, xb_item *var, uint32_t is_static);
void     xb_ref_elem(xb_ctx *ctx, xb_item *r, const xb_item *base, const xb_item *idx);

/* FOR EACH: a collection's length (-1 after an error). The loop works on a
 * base item: the variable's cell when it is shared, else `var`, its own item.
 * `in_cell` comes from xb_each_shared, when the loop starts (for a STATIC, at
 * every call: it is one variable for everyone); a variable holding an element
 * passed to it by reference moves into a cell then. xb_each_enter saves the
 * base's value in `saved` and puts a reference to the collection there;
 * xb_each_move points it at element `idx` (1-based); xb_each_leave restores
 * the saved value. */
int64_t  xb_enum_len(xb_ctx *ctx, const xb_item *coll);
uint32_t xb_each_shared(xb_ctx *ctx, xb_item *var);
void     xb_each_enter(xb_ctx *ctx, xb_item *saved, xb_item *var, uint32_t in_cell, const xb_item *coll);
void     xb_each_move(xb_ctx *ctx, xb_item *var, uint32_t in_cell, int64_t idx);
void     xb_each_leave(xb_item *var, uint32_t in_cell, const xb_item *saved);

/* SWITCH: the value must be an integer or a string (else an error, whose
 * handler may substitute another value); xb_switch_check stores in `r` the
 * value the CASEs match exactly. */
void     xb_switch_check(xb_ctx *ctx, xb_item *r, const xb_item *value);
int32_t  xb_switch_match(const xb_item *value, const xb_item *c);

/* Modules. A compiled module registers its initialization and its STATICs
 * before main(); xb_modules_init runs the initializations in order. */
typedef void (*xb_module_init)(xb_ctx *ctx);
void     xb_module_register(xb_module_init init, xb_item *statics, uint32_t nstatics);
void     xb_modules_init(xb_ctx *ctx);

/* Codeblocks. xb_block_new stores the new block in `r` and returns it; the
 * caller fills cells[0 .. ncells-1] (xb_cell_of of each captured variable,
 * obtained first) before anything else can allocate. */
xb_block *xb_block_new(xb_ctx *ctx, xb_item *r, xb_block_fn fn, uint32_t ncells);
void     xb_block_call(xb_ctx *ctx, xb_item *ret, const xb_item *block, uint32_t argc, xb_item *argv);
/* FOR ... TO ... STEP test: 1 = keep looping. */
int32_t  xb_for_test(xb_ctx *ctx, const xb_item *counter, const xb_item *limit, const xb_item *step);

/* BEGIN SEQUENCE. Where an operation of the sequence's body unwinds,
 * xb_seq_catch tells whether the request is a BREAK alone, and takes it (1);
 * otherwise the unwinding goes on (0). xb_seq_value then moves the BREAK value
 * into `r` (RECOVER USING). */
int32_t  xb_seq_catch(xb_ctx *ctx);
void     xb_seq_value(xb_ctx *ctx, xb_item *r);

xb_ctx  *xb_ctx_new(void);
void     xb_ctx_free(xb_ctx *ctx);
uint32_t xb_ctx_pending(const xb_ctx *ctx);
void     xb_ctx_raise(xb_ctx *ctx, uint32_t what);
int      xb_ctx_exitcode(const xb_ctx *ctx);

xb_str  *xb_str_new(const char *p, size_t len);
xb_str  *xb_str_concat(const xb_str *a, const xb_str *b);

/* [ABI] Items are written as two 8-byte words, the header (type, width,
 * decimals) and the value, by the runtime and by compiled code alike: a load
 * is served by the store that wrote it (store-to-load forwarding) only when
 * that store covers it, and compiled code reads items by words; an item
 * written field by field would stall each read. The value bytes a member does
 * not use are zero. The byte order is the host's, found by xb_little_endian
 * (which compilers fold to a constant). */
static inline int xb_little_endian(void) {
  const union {
    uint16_t u;
    uint8_t b[2];
  } probe = {1};
  return probe.b[0] == 1;
}
static inline uint64_t xb_header_word(uint32_t type, uint16_t width, uint16_t dec) {
  return xb_little_endian() ? (uint64_t)type | (uint64_t)width << 32 | (uint64_t)dec << 48
                            : (uint64_t)type << 32 | (uint64_t)width << 16 | (uint64_t)dec;
}
/* The value word of a 4-byte member (a logical, a pointer on 32-bit hosts),
 * which starts at the first byte of the union. */
static inline uint64_t xb_short_value_word(uint32_t v) { return xb_little_endian() ? v : (uint64_t)v << 32; }
static inline uint64_t xb_pointer_word(const void *p) {
  return sizeof p == 8 ? (uint64_t)(uintptr_t)p : xb_short_value_word((uint32_t)(uintptr_t)p);
}
static inline void xb_put(xb_item *it, uint64_t header, uint64_t value) {
  memcpy(it, &header, 8);
  memcpy(&it->v, &value, 8);
}

static inline void xb_set_nil(xb_item *it) { xb_put(it, xb_header_word(XB_T_NIL, 0, 0), 0); }
static inline void xb_set_logical(xb_item *it, int b) {
  xb_put(it, xb_header_word(XB_T_LOGICAL, 0, 0), xb_short_value_word(b ? 1u : 0u));
}
static inline void xb_set_int(xb_item *it, int64_t n) { xb_put(it, xb_header_word(XB_T_INT, 0, 0), (uint64_t)n); }
static inline void xb_set_double(xb_item *it, double d, uint16_t dec) {
  uint64_t bits;
  memcpy(&bits, &d, 8);
  xb_put(it, xb_header_word(XB_T_DOUBLE, 0, dec), bits);
}
static inline void xb_set_str(xb_item *it, xb_str *s) {
  xb_put(it, xb_header_word(XB_T_STRING, 0, 0), xb_pointer_word(s));
}
/* An item of a heap type other than a string (an array, a block, a cell, a reference, an object). */
static inline void xb_set_ptr(xb_item *it, xb_type type, void *p) {
  xb_put(it, xb_header_word(type, 0, 0), xb_pointer_word(p));
}

/* [ABI] Write barrier of the generational collector. Every heap object is
 * preceded by a header; the byte XB_GC_STATE_BACK bytes before the object is
 * its state. An object that survived a collection is old; storing a pointer to
 * a newer object into an old one must record the old one (xb_gc_remember), so
 * that a young collection, which does not scan old objects, still sees the
 * store. Code that stores an item into a heap object (a cell, an array) calls
 * xb_gc_write on it after the store; compiled code inlines the same test. */
#define XB_GC_STATE_BACK     3u
#define XB_GC_STATE_OLD      1u   /* bit: survived a collection */
#define XB_GC_STATE_RECORDED 2u   /* bit: in the remembered set, to be traced whole */
void xb_gc_remember(void *obj);
static inline void xb_gc_write(void *obj) {
  uint8_t state = *((const uint8_t *)obj - XB_GC_STATE_BACK);
  if ((state & (XB_GC_STATE_OLD | XB_GC_STATE_RECORDED)) == XB_GC_STATE_OLD) xb_gc_remember(obj);
}

/* Whether a double converts to int64_t without overflow (false for NaN and
 * infinities). 2^63 is exact in a double; INT64_MAX is not. */
static inline int xb_dbl_fits_i64(double d) { return d >= -9223372036854775808.0 && d < 9223372036854775808.0; }

/* Default display width of the integer part of a number (Clipper rule):
 * 10 columns, 20 when the value does not fit in 10. Shared by the compiler
 * (numeric literals) and the runtime (computed values). */
static inline uint16_t xb_num_int_width(int64_t i) {
  return (uint16_t)((i < -999999999 || i > INT64_C(9999999999)) ? 20 : 10);
}
static inline uint16_t xb_num_dbl_width(double d) {
  return (uint16_t)((d > 9999999999.0 || d < -999999999.0) ? 20 : 10);
}

/* xbase's bit functions (BitAnd, BitOr, BitXor, BitNor, BitNot, BitShl,
 * BitShr) work on the 64-bit two's complement form of integers; BitShr is a
 * logical shift, and a shift count outside 0..63 gives 0. The runtime, the
 * compiler's folding of constant calls and its inline code for integer
 * arguments share these rules. */
typedef enum xb_bit_op { XB_BIT_AND, XB_BIT_OR, XB_BIT_XOR, XB_BIT_NOR, XB_BIT_NOT, XB_BIT_SHL, XB_BIT_SHR } xb_bit_op;
static inline int64_t xb_bit_apply(xb_bit_op op, int64_t a, int64_t b) {
  uint64_t x = (uint64_t)a, y = (uint64_t)b;
  switch (op) {
  case XB_BIT_AND: return (int64_t)(x & y);
  case XB_BIT_OR: return (int64_t)(x | y);
  case XB_BIT_XOR: return (int64_t)(x ^ y);
  case XB_BIT_NOR: return (int64_t)~(x | y);
  case XB_BIT_NOT: return (int64_t)~x;
  case XB_BIT_SHL: return y < 64 ? (int64_t)(x << y) : 0;
  case XB_BIT_SHR: return y < 64 ? (int64_t)(x >> y) : 0;
  }
  return 0;
}

/* Each bit function: its name (upper-cased, as calls are resolved), argument
 * count and BASE error code (xbase's own functions have codes from 9001).
 * Indexed by xb_bit_op; the runtime's functions and the compiler's folding
 * and inline code read this table. */
typedef struct xb_bit_function {
  xb_bit_op op;
  const char *name;
  uint32_t arity;
  int code;
} xb_bit_function;
#define XB_BIT_FUNCTION_COUNT (XB_BIT_SHR + 1)
static inline const xb_bit_function *xb_bit_function_of(xb_bit_op op) {
  static const xb_bit_function table[XB_BIT_FUNCTION_COUNT] = {
      {XB_BIT_AND, "BITAND", 2, 9001}, {XB_BIT_OR, "BITOR", 2, 9002},   {XB_BIT_XOR, "BITXOR", 2, 9003},
      {XB_BIT_NOR, "BITNOR", 2, 9004}, {XB_BIT_NOT, "BITNOT", 1, 9005}, {XB_BIT_SHL, "BITSHL", 2, 9006},
      {XB_BIT_SHR, "BITSHR", 2, 9007},
  };
  return &table[op];
}

/* Display width of an integer constant in the program (a literal or a folded
 * expression): 10 columns plus one per digit beyond nine, 20 for values of
 * -1000000000 or less. Computed integers use xb_num_int_width instead. */
static inline uint16_t xb_num_const_int_width(int64_t i) {
  if (i <= -1000000000) return 20;
  uint16_t w = 10;
  for (; i >= 1000000000; i /= 10) ++w;
  return w;
}

/* Clipper's string `-`: the trailing blanks of `a` move after `b`. Writes
 * la + lb bytes to `out`. */
static inline void xb_str_minus(char *out, const char *a, size_t la, const char *b, size_t lb) {
  size_t keep = la;
  while (keep && a[keep - 1] == ' ') --keep;
  for (size_t i = 0; i < keep; ++i) out[i] = a[i];
  for (size_t i = 0; i < lb; ++i) out[keep + i] = b[i];
  for (size_t i = keep + lb; i < la + lb; ++i) out[i] = ' ';
}

/* Formats a value the way ? / QOut() shows it. Returns a new string. */
xb_str  *xb_item_display(const xb_ctx *ctx, const xb_item *it);

/* [ABI] Program entry, emitted by xbc in the start module: `argv` holds the
 * command-line arguments after the program's name, as strings, which it
 * passes to the start function as its arguments. */
void     xb_app_main(xb_ctx *ctx, uint32_t argc, xb_item *argv);

#ifdef __cplusplus
}
#endif
#endif /* XBRT_H */
