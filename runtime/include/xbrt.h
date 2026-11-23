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

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "xbmath.h" /* ^, Exp(), Log(): the same doubles on every system */

#ifdef __cplusplus
extern "C" {
#endif

#define XB_ABI_VERSION 63u

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
  XB_T_HASH    = 10,  /* v.p: a hash (keys to values, in insertion order) */
  XB_T_DATE    = 11,  /* v.i: Julian day number, 0 for the empty date  */
  XB_T_SYMBOL  = 12,  /* v.p: the xb_funcref of a function reference
                         (@Name()), static: never collected */
  XB_T_COUNT_
} xb_type;
/* ValType() of each tag, by tag (a cell or a reference, which ValType never
 * sees, as NIL). */
#define XB_TYPE_LETTERS "ULNNCABUUOHDS"
/* [ABI] The `width` of a string item that is a memo field's value: ValType()
 * "M". Every copy of the item keeps it; a string made from it is plain. */
#define XB_STR_MEMO 1u
#define XB_MEMO_LETTER 'M'

/* [ABI] The tags whose v.p is a heap object form one range, so that one
 * unsigned comparison tells whether storing an item can make an old object
 * refer to a young one (the write barrier is needed only then). */
#define XB_T_HEAP_FIRST XB_T_STRING
#define XB_T_HEAP_LAST  XB_T_HASH
static inline int xb_type_is_heap(uint32_t type) {
  return type - (uint32_t)XB_T_HEAP_FIRST <= (uint32_t)(XB_T_HEAP_LAST - XB_T_HEAP_FIRST);
}

/* [ABI] string header. Strings are immutable: they can be shared freely
 * between items and threads. A plain string's bytes follow its header and are
 * NUL terminated (len excludes the terminator), so they can be handed to C
 * without copying; a view's (XB_STR_VIEW) are the first `len` bytes of a
 * buffer, which a longer view of it may go on past, so C code that reads up
 * to a NUL takes them through xb_item_cstr (types/types.h). */
#define XB_STR_STATIC 0x1u   /* not a heap object, never freed: literals, the empty and one-byte strings */
#define XB_STR_APPEND 0x2u   /* made by xb_op_append and seen only by its variable: may grow in place;
                                `hash` then holds the bytes it has room for (its length at least) */
#define XB_STR_VIEW   0x4u   /* a view of a buffer (xb_strbuf): where the bytes would be, the buffer's
                                address; never empty, never XB_STR_APPEND nor XB_STR_STATIC */

typedef struct xb_str xb_str;
typedef struct xb_strbuf xb_strbuf;
#ifndef __cplusplus
/* The compiler mirrors these layouts as LLVM types and never uses the C++ view
 * of them (a flexible array member is not standard C++). */
struct xb_str {
  uint32_t flags;
  uint32_t hash;
  size_t   len;
  char     bytes[];
};

/* [ABI] The bytes that views share, a heap object of its own: `cap` bytes of
 * room, the first `used` written (the tip), followed by a NUL. An appending
 * `+` (xb_op_add_grow) whose left operand is a view ending at the tip writes
 * the right operand's bytes there and moves the tip (xb_str_add): views of one
 * buffer are each a prefix of what was written, and bytes below the tip never
 * change. */
struct xb_strbuf {
  size_t cap;  /* room for the bytes and the NUL that follows the tip */
  size_t used; /* changed only by an atomic compare-and-swap: two threads may append to one view */
  char   bytes[];
};

/* The buffer of a view. */
static inline xb_strbuf *xb_str_view_buffer(const xb_str *s) {
  xb_strbuf *b;
  memcpy(&b, s->bytes, sizeof b);
  return b;
}

/* Sets the buffer of a view: one being made, or moved by the collector. */
static inline void xb_str_view_set_buffer(xb_str *s, xb_strbuf *b) { memcpy(s->bytes, &b, sizeof b); }

/* The bytes of a string: those after the header, or a view's in its buffer.
 * Every read goes through here, never through the member. */
static inline const char *xb_str_data(const xb_str *s) {
  return s->flags & XB_STR_VIEW ? xb_str_view_buffer(s)->bytes : s->bytes;
}

/* The writable bytes of a string being built: one the caller has just
 * allocated and not yet shared, or an XB_STR_APPEND string that only its
 * variable sees. Such a string is never a view. */
static inline char *xb_str_buffer(xb_str *s) { return s->bytes; }
#endif

/* [ABI] the dynamic value. Copying an item is a plain memcpy: memory is
 * managed by a tracing GC, so there is no reference counting. */
typedef struct xb_item {
  uint32_t type;      /* xb_type                                    */
  uint16_t width;     /* numeric display width (0 = default); a
                         string's XB_STR_MEMO                       */
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
 * fail and in every loop (on each back edge, or once every 1024 passes of an
 * innermost loop without calls), with an atomic load: other threads set
 * bits there (a collection's safepoint request, objects to finalize), and
 * every change is an atomic read-modify-write. When it is non-zero the code
 * calls xb_pending_gc(), which parks the thread while another thread's
 * collection runs (a safepoint) and runs the destructors of the objects to
 * finalize, and returns non-zero when the function must unwind (a BREAK or a
 * QUIT; an error that is not recovered ends in one; an error while Type()
 * evaluates its text ends that evaluation, a TRAP no BEGIN SEQUENCE catches).
 * Parking and destructors may collect, so after an operation whose error may
 * substitute its result while that result waits where the collector does not
 * see it, the code calls xb_pending() instead, which never collects nor parks
 * and leaves both to a later test (the runtime's C code calls it too). The
 * common case costs one load and one predictable branch, works on every
 * target, and survives AWAIT (unlike setjmp/longjmp). */
enum {
  XB_PENDING_NONE      = 0,
  XB_PENDING_BREAK     = 1u << 1,
  XB_PENDING_QUIT      = 1u << 2,
  XB_PENDING_SAFEPOINT = 1u << 3,
  XB_PENDING_TRAP      = 1u << 4,
  XB_PENDING_FINALIZE  = 1u << 5, /* objects to finalize: xb_pending_gc runs their destructors */
  XB_PENDING_FAIL      = 1u << 6, /* a task fails (its value in break_value): no sequence takes it, its end does */
};
#define XB_PENDING_UNWIND (XB_PENDING_BREAK | XB_PENDING_QUIT | XB_PENDING_TRAP | XB_PENDING_FAIL)

/* [ABI] An array: `len` elements at `data`, which start in the same
 * allocation (`inline_items`); when it grows past that, they move to an
 * object of their own. That storage always has room for `cap` items, and
 * the form of the elements, bits XB_GC_STATE_ELEMS of the array's GC state
 * byte (xb_gc_write below), says how they are stored in it:
 * - XB_ELEMS_ITEMS: as items, any values;
 * - XB_ELEMS_NIL: not at all, every element is NIL;
 * - XB_ELEMS_LOGICAL: one byte each, 0 for NIL, XB_ELEMS_FALSE, XB_ELEMS_TRUE;
 * - XB_ELEMS_INT: an int64_t each, XB_ELEMS_INT_NIL for NIL.
 * The compact forms hold only items whose header word is their type's with no
 * width or decimals (xb_header_word( type, 0, 0 )): NIL, a logical, an
 * integer other than XB_ELEMS_INT_NIL. A store of any other value turns the
 * array into one of items first, in place (the room is there: it allocates
 * nothing); nothing turns it back. A new array of the program (Array( n )
 * of one dimension, a literal) starts as XB_ELEMS_NIL, which the first value
 * stored in it makes logical, integer or items; the runtime's own arrays, and every array other threads may reach,
 * hold items. Compiled code reads `len`, `data` and the state byte to access
 * elements in place: one test of the byte tells a local array of items (the
 * usual one) from all the others. */
enum {
  XB_ELEMS_ITEMS   = 0,
  XB_ELEMS_NIL     = 1,
  XB_ELEMS_LOGICAL = 2,
  XB_ELEMS_INT     = 3,
};
#define XB_ELEMS_INT_NIL INT64_MIN
#define XB_ELEMS_FALSE   1 /* XB_ELEMS_LOGICAL's bytes; 0 is NIL */
#define XB_ELEMS_TRUE    2
typedef struct xb_array xb_array;
#ifndef __cplusplus
struct xb_array {
  size_t   len;
  size_t   cap;
  void    *data;
  xb_item  inline_items[];
};
#endif

/* [ABI] What the call stack shows of a function (ProcName(), the "Called
 * from" lines): its name, "(b)NAME" for a codeblock of NAME; its number of
 * parameters, the first slots of its frame when it keeps its arguments
 * (XB_PROC_ARGS; a typed copy may keep a parameter out of its frame); its
 * flags; and the name of its
 * source file, without directories (ProcFile(); NULL for the runtime's
 * functions). Compiled code emits one constant per function. */
typedef struct xb_proc {
  const char *name;
  uint32_t nparams;
  uint32_t flags;
  const char *file;
} xb_proc;

/* xb_proc.flags. XB_PROC_ARGS: the frame's `argc` and `argv` hold the
 * arguments received; only a function or codeblock that calls PCount() or
 * PValue() stores them, nothing else can read them. XB_PROC_HIDDEN: a STATIC
 * function or a codeblock of the runtime's own xBase code, which the call
 * stack does not show (PCount() and PValue() still read its frame). */
#define XB_PROC_ARGS 1u
#define XB_PROC_HIDDEN 2u
/* XB_PROC_METHOD: a method, whose first argument is the object (Self):
 * PCount() and PValue() count the arguments after it. */
#define XB_PROC_METHOD 8u
/* xb_proc.flags the runtime alone sets: XB_PROC_TRANSPARENT, a frame that
 * evaluates a macro, which is not an activation of its own (PRIVATEs it
 * creates belong to the one below it, RELEASE ALL acts there). */
#define XB_PROC_TRANSPARENT 4u
/* The start of a codeblock's xb_proc.name, before the name of the function
 * that made it (what the call stack shows). */
#define XB_BLOCK_PREFIX "(b)"

/* [ABI] A compiled function's item frame (parameters, LOCALs, temporaries):
 * on the machine stack, linked into its context's chain while the function
 * runs. The collector scans the frames of every chain; slots are NIL or valid
 * items whenever something can allocate. `proc` (NULL: a frame the call stack
 * does not show) and `line`, the source line of the statement running, set
 * before each call and before an operation can raise an error, make the call
 * stack. `argc` and `argv` (with XB_PROC_ARGS) are the arguments as the
 * function received them (PCount(), PValue(), the arguments past its
 * parameters), owned by its caller, which keeps them alive and rooted during
 * the call; the callee may replace them, so the caller does not read them
 * again after the call. The runtime links frames without slots or arguments
 * for its own functions while what they do can be seen: an error block they
 * run, a codeblock they call. Compiled code links one too, over its own,
 * while the body of a function or codeblock it runs in place (inlined) runs:
 * `argc` and `argv` are not set (its proc has no XB_PROC_ARGS). */
typedef struct xb_frame xb_frame;
#ifndef __cplusplus
struct xb_frame {
  xb_frame *prev;
  const xb_proc *proc;
  uint32_t nslots;
  uint32_t line;
  xb_item *argv;
  uint32_t argc;
  xb_item slots[];
};
#endif

/* [ABI] A thread's context (core/context.h). Compiled code uses its first
 * fields, which the context starts with in this order: `uint32_t pending`
 * (requests tested after every operation that can fail), `int32_t
 * set_decimals` (SET DECIMALS, the decimals of `/` and `%`, as 16 bits),
 * `xb_frame *frames` (the innermost frame of the chain), `const char
 * *stack_limit` (a function whose frame lies below it calls xb_stack_deep
 * and, unless that lets it go on, xb_stack_overflow before its body runs,
 * then returns unwinding; NULL: no limit), `xb_item *lent` (the arguments a
 * compiled caller lends for the call it makes, NULL after it: a compiled
 * function with a direct entry called with them as its argv reads the items it
 * has parameters for, sets them to NIL and `lent` to NULL, so that it alone
 * holds what only they held; one without a direct entry ends the loan. Runtime
 * code may pass the argv it received on whole only if it reads none of it after
 * the call, as a send does; otherwise it sets `lent` to NULL first). */
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
/* [ABI] The other functions of a module of the runtime written in xBase (its methods, generators' resume functions,
 * STATIC functions), which a program reaches once the compiler ran a body of the runtime's in place (a method of a
 * runtime class, docs/xir.md): this prefix, the module's name, '$', and the function's name with ':' and '.' as '$'. */
#define XB_LIBRARY_FUNC_PREFIX "xbl_"
/* [ABI] The public functions of an xbase library (xbc --library, docs/projects.md): this prefix, the library's name,
 * '$', the function's name, so that neither the program's functions nor another library's clash with them; and its
 * stamp, xbx_LIBRARY$source$HASH (HASH the FNV-1a 64 of its sources), which every program compiled with it requires. */
#define XB_XBLIB_FUNC_PREFIX "xbx_"
/* The format of a module file (.xbm, docs/projects.md), which xbc reads and writes and the runtime's build writes for
 * the runtime. */
#define XB_MODULE_FILE_FORMAT 1

/* [ABI] Whether a class of the program answers the messages to values of type t before the runtime's own class of the
 * type (a SCALAR CLASS of the program, or a class ClassAssociate() associated with the type): 0 while the runtime's
 * alone does. Compiled code that calls a method of the runtime's class of a type directly (its Enumerable methods, say)
 * tests it first: a byte each, set once and never cleared, read unordered. */
extern uint8_t xb_scalar_replaced[XB_T_COUNT_];

/* [ABI] A variable captured by a codeblock lives in a cell (value at offset 0)
 * shared by the function and its blocks. */
typedef struct xb_cell {
  xb_item value;
} xb_cell;

/* [ABI] What an XB_T_REF item designates: element `index` of `coll`. For a
 * string, `value` holds the one-character element (assigning it leaves the
 * string unchanged); for any other value that is no array or hash, `value` is
 * the element. For a hash, the entry of `key` when it is not NIL (@h[k], the
 * current element of a loop taken by a block or a reference), else the entry
 * at position `index` (a FOR EACH loop). `loop`: the reference of a FOR EACH
 * loop, else an element passed by reference (@a[i]). Compiled code reads the
 * element of an array, or of a hash without holes, in place, and moves a
 * loop's reference to the next one (types/ref.c finds the others). */
typedef struct xb_ref {
  xb_item coll;
  int64_t index;
  xb_item value;
  xb_item key;
  uint32_t loop;
} xb_ref;

/* [ABI] A hash (types/hash.c): its entries in order, in `entries`; without
 * holes (`used` == `len`), position k is entry k - 1, which compiled code
 * reads in place. */
typedef struct xb_hentry {
  xb_item key;
  xb_item value;
} xb_hentry;

typedef struct xb_hash {
  uint32_t len;       /* live entries */
  uint32_t used;      /* entries taken, holes included */
  uint32_t cap;       /* entries that fit */
  uint32_t flags;
  uint32_t imask;     /* index slots - 1 */
  xb_hentry *entries; /* the store; NULL while cap is 0 */
  uint32_t *hashes;   /* in the store, after the entries */
  uint32_t *index;    /* in the store, after the hashes */
  uint32_t *tree;     /* in the store, after the index; NULL while not in use */
} xb_hash;

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
int32_t  xb_pending_gc(xb_ctx *ctx);

/* The layout the module was compiled for: offsets of xb_str.bytes,
 * xb_block.cells, xb_frame.slots, xb_ctx.frames, xb_ctx.set_decimals,
 * xb_ctx.stack_limit, xb_ctx.lent, xb_array.data (after len and cap), xb_ref.value (after coll and index),
 * xb_hash.entries (after its five counts), xb_object.vars (after cls and nvars)
 * and xb_strbuf.bytes (after cap and used). */
void     xb_abi_check(uint32_t version, uint32_t item_size, uint32_t str_hdr_size, uint32_t block_cells_offset,
                      uint32_t frame_slots_offset, uint32_t ctx_frames_offset, uint32_t ctx_decimals_offset,
                      uint32_t ctx_stack_limit_offset, uint32_t ctx_lent_offset, uint32_t array_data_offset,
                      uint32_t ref_value_offset, uint32_t hash_entries_offset, uint32_t object_vars_offset,
                      uint32_t strbuf_bytes_offset);
/* A frame (its address) below the context's stack_limit: xb_stack_deep gives
 * 1 when that limit was only the mark of a deep stack (core/ctx.c), whose
 * pages go back to the system once it is shallow again, and the frame lies
 * above the stack error's limit, which the context now has: the function goes
 * on; 0: the stack error is due, which xb_stack_overflow raises (EG_LIMIT,
 * "Stack overflow"). xb_stack_deep reads and writes the context alone (the
 * compiler declares it so). */
int32_t  xb_stack_deep(xb_ctx *ctx, uintptr_t frame);
void     xb_stack_overflow(xb_ctx *ctx);

void     xb_op_add(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
/* a + b whose result goes back where `a` came from: `x += b` or `x := x + b + ...` on an element, a hash value, an
 * instance variable, a memory variable, a STATIC or a captured LOCAL (the compiler marks it). The same as xb_op_add,
 * except that a long string result is a view with room to grow (xb_str_add, types/types.h): such appends copy O(n)
 * bytes in all, not O(n^2). xb_op_add gives a string of the exact size. */
void     xb_op_add_grow(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_sub(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
/* ++ and -- (also the step of a FOR without STEP): a + 1 and a - 1 for a
 * number or a date; anything else is their own argument error (1086, 1087),
 * with the operand as its only argument. */
void     xb_op_inc(xb_ctx *ctx, xb_item *r, const xb_item *a);
void     xb_op_dec(xb_ctx *ctx, xb_item *r, const xb_item *a);
void     xb_op_mul(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_div(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_mod(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_pow(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
/* [ABI] e^x and ln( x ) of doubles (xbmath.h): what Exp() and Log() compute, called by the code xbc makes inline for
 * them; they read and write no memory. */
double   xb_exp(double x);
double   xb_log(double x);
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
/* var := var + b for a variable that nothing else sees (a LOCAL the compiler
 * keeps in its frame): a string the call made earlier (XB_STR_APPEND) takes
 * b's bytes in its spare room; another string result is made with room to
 * grow; anything else is xb_op_add. Compiled code does the first case in
 * place, and clears XB_STR_APPEND (and `hash`, the room) when it reads the
 * variable otherwise, so a string that escapes is immutable. */
void     xb_op_append(xb_ctx *ctx, xb_item *var, const xb_item *b);
/* The order of two compared values: -1 (less), 0 (equal), 1 (greater), or
 * XB_ORDER_UNORDERED when a number is NaN, which IEEE 754 orders with
 * nothing. xb_order_is tells whether an order satisfies a comparison that
 * holds for the orders it names: `<` is (1, 0, 0), `<=` (1, 1, 0), `=` and
 * `==` (0, 1, 0), `>=` (0, 1, 1), `>` (0, 0, 1); `!=` is not `=`. So every
 * comparison with NaN is false but `!=`. Shared by the runtime's operators
 * and the compiler's constant folding. */
#define XB_ORDER_UNORDERED 3
static inline int xb_order_double(double x, double y) {
  return x < y ? -1 : x > y ? 1 : x == y ? 0 : XB_ORDER_UNORDERED;
}
static inline int xb_order_is(int order, int less, int equal, int greater) {
  return (order == -1 && less) || (order == 0 && equal) || (order == 1 && greater);
}

/* The bit operators & | ^^ << >> and ~ (xb_bit_apply's rules; numbers are
 * taken as integers like the bit functions take them). */
void     xb_op_bitand(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_bitor(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_bitxor(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_shl(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_shr(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_bitnot(xb_ctx *ctx, xb_item *r, const xb_item *a);

/* A double whose decimals are the current SET DECIMALS (constant-folded `/`,
 * `%` on doubles and `^`). */
void     xb_num_default_dec(xb_ctx *ctx, xb_item *r, double value);

/* Non-logical value used as a condition: raises the error, returns 0. */
int32_t  xb_cond_error(xb_ctx *ctx, const xb_item *a);
/* [ABI] The truth of the operand `a` of a .NOT. in a condition, `a` no
 * logical: an object whose class overloads .NOT. gives the negation of its
 * result's truth, 0 or 1 (the caller negates it back); a result that is no
 * logical, and anything but such an object, raise the condition's error:
 * XB_COND_NOT_FAILED, after which the caller unwinds, as after
 * xb_cond_error. */
#define XB_COND_NOT_FAILED 2
int32_t  xb_cond_not(xb_ctx *ctx, const xb_item *a);

/* Arrays (1-based). xb_array_new copies `n` items (consecutive frame slots). */
void     xb_array_new(xb_ctx *ctx, xb_item *r, uint32_t n, const xb_item *elems);
void     xb_op_index(xb_ctx *ctx, xb_item *r, const xb_item *base, const xb_item *idx);
void     xb_op_index_set(xb_ctx *ctx, const xb_item *base, const xb_item *idx, const xb_item *value);
/* A hash literal: `n` keys and values alternating in `kv` (2n consecutive
 * frame slots); a key that is not a string, a number or a date is an error. */
void     xb_hash_new(xb_ctx *ctx, xb_item *r, uint32_t n, const xb_item *kv);

/* Sends `message` (upper-cased; "_NAME" assigns the variable NAME) to argv[0]
 * with the arguments argv[1 .. argc-1]; argc is at least 1. */
void     xb_op_send(xb_ctx *ctx, xb_item *r, const char *message, uint32_t argc, xb_item *argv);

/* Write barrier after storing into element `index` of an array: for a heap
 * value in an old array, a large array records the element's card, another
 * the whole array. Compiled code that stores an element in place calls it for
 * heap values only. xb_gc_write_items is the barrier after storing items
 * `from` .. `to` - 1: each card they cover recorded when one of its items among
 * them refers to an object; compiled code calls it when the array's state byte
 * is old and not recorded (xb_gc_needs_record). */
void     xb_gc_write_item(xb_array *a, size_t index);
void     xb_gc_write_items(xb_array *a, size_t from, size_t to);
/* [ABI] Temporary roots of a context, as many as nested calls need (a vector that grows; growing it is the only
 * allocation, and never a collection): C code, and compiled code while it makes a codeblock's cells. */
void     xb_gc_root_push(xb_ctx *ctx, xb_item *it);
void     xb_gc_root_pop(xb_ctx *ctx, unsigned n);

/* References. An item that designates another item is an XB_T_CELL (a
 * variable in a cell) or an XB_T_REF (an element); xBase code never sees one,
 * compiled code goes through it. The tags are consecutive, so compiled code
 * tests for both with one unsigned comparison.
 * A variable's own item is its frame slot or STATIC storage; it moves into a
 * cell when it is first shared (a LOCAL or parameter captured by a codeblock,
 * any variable passed by reference). A LOCAL or parameter that holds the
 * reference of a FOR EACH loop is not moved: whoever takes it gets the
 * current element, and the variable stays the loop's.
 * xb_cell_of returns the cell a codeblock captures for the variable whose own
 * item is `var` (`is_static`: a STATIC's, shared storage once threads run);
 * xb_ref_of stores in `r` the reference an argument @var passes (`is_static`:
 * a STATIC, always the variable itself);
 * xb_ref_elem the one @base[idx] passes, with the checks of xb_op_index.
 * A cell may hold an XB_T_REF (a FOR EACH variable); an element never holds
 * either. Compiled code reads and writes a local cell's value in place (a
 * cell's value is at offset 0, its write barrier inline); everything else (a
 * shared cell, an element) goes through xb_ref_load and xb_ref_store, given
 * the item's type (XB_T_CELL or XB_T_REF) and pointer: the whole item, under
 * the object's counter or lock when it is shared, its write barrier made (a
 * bound error if the element is gone). With the type XB_T_NIL the pointer is a
 * STATIC's own item, in the program's storage: read and written whole under
 * its stripe, through the cell or element it holds (xb_storage_shared). */
xb_cell *xb_cell_of(xb_ctx *ctx, xb_item *var, uint32_t is_static);
/* A new cell holding a copy of `value`: a codeblock's capture of a variable by its value (one never assigned after
 * the block is made), which the block alone refers to. */
xb_cell *xb_cell_new(xb_ctx *ctx, const xb_item *value);
/* [ABI] Nonzero once a second thread has attached: the program's storage (the STATICs, the class variables, the memory
 * variables visible then) is shared from then on (docs/concurrency.md, publication), so compiled code reads and writes
 * a STATIC through xb_ref_load and xb_ref_store with the type XB_T_NIL. Set while every other thread is stopped. */
extern uint32_t xb_storage_shared;
void     xb_ref_load(xb_ctx *ctx, uint32_t type, void *target, xb_item *dst);
void     xb_ref_store(xb_ctx *ctx, uint32_t type, void *target, const xb_item *v);
void     xb_ref_of(xb_ctx *ctx, xb_item *r, xb_item *var, uint32_t is_static);
void     xb_ref_elem(xb_ctx *ctx, xb_item *r, const xb_item *base, const xb_item *idx);

/* FOR EACH: a collection's length (-1 after an error). The loop works on a
 * base item: the variable's cell when it is shared, else `var`, its own item.
 * `in_cell` comes from xb_each_shared, when the loop starts (for a STATIC, at
 * every call: it is one variable for everyone; once the program's storage is
 * shared, it moves into a cell then); a variable holding an element passed to
 * it by reference moves into a cell then. `how` has XB_EACH_IN_CELL for that,
 * and XB_EACH_STATIC for a STATIC (whose own item, shared storage once threads
 * run, is read and written whole). xb_each_enter saves the base's value in
 * `saved` and puts a reference to the collection there; xb_each_move points it
 * at element `idx` (1-based); xb_each_leave restores the saved value. */
#define XB_EACH_IN_CELL 1u
#define XB_EACH_STATIC  2u
int64_t  xb_enum_len(xb_ctx *ctx, const xb_item *coll);
/* What a FOR EACH loop goes through, in `r`, once before it starts: the
 * collection itself, or for an object its enumerator (GetEnumerator(), else
 * the object when it answers MoveNext and Current, after Reset() when it has
 * one); with `descend`, that enumerator's elements in an array. */
void     xb_each_source(xb_ctx *ctx, xb_item *r, const xb_item *coll, uint32_t descend);
uint32_t xb_each_shared(xb_ctx *ctx, xb_item *var, uint32_t is_static);
void     xb_each_enter(xb_ctx *ctx, xb_item *saved, xb_item *var, uint32_t how, const xb_item *coll);
void     xb_each_move(xb_ctx *ctx, xb_item *var, uint32_t how, int64_t idx);
void     xb_each_leave(xb_item *var, uint32_t how, const xb_item *saved);
/* FOR EACH k => v: the key of the hash entry at `idx` (1-based), the index
 * itself for an array or a string. */
void     xb_each_key(xb_ctx *ctx, xb_item *r, const xb_item *coll, int64_t idx);

/* SWITCH: the value must be an integer or a string (else an error, whose
 * handler may substitute another value); xb_switch_check stores in `r` the
 * value the CASEs match exactly. */
void     xb_switch_check(xb_ctx *ctx, xb_item *r, const xb_item *value);
int32_t  xb_switch_match(const xb_item *value, const xb_item *c);

/* Whether argument position k is in `refs`, a mask of the positions that
 * receive references as passed (bit k: position k, bit 31: position 31 and
 * after); the others receive values. */
static inline int xb_refs_take(uint32_t refs, uint32_t k) { return (int)((refs >> (k < 31 ? k : 31)) & 1u); }

/* `...` in a call, a send or an array literal: copies the `n` items of `rest`
 * (the arguments the function received past its parameters) to `dst`, where
 * they are the list's positions `at` and on. A reference goes as it is to a
 * position in `refs` (xb_refs_take) and as its value elsewhere. `dst` may be
 * `rest` itself. Returns 0 when a reference's target is gone and the error
 * asks to unwind. */
int32_t  xb_args_rest(xb_ctx *ctx, xb_item *dst, xb_item *rest, uint32_t n, uint32_t at, uint32_t refs);

/* Modules. A compiled module registers, before main(), the function that
 * initializes its STATICs (which become GC roots) and its INIT and EXIT
 * procedures: `procs` holds `ninit` INIT procedures, then `nexit` EXIT ones,
 * each in source order. xb_program_run (called by the start module's
 * xb_app_main) runs the STATICs' initializations, ErrorSys() (found by name:
 * the program's own, else the runtime's), the INIT procedures (module by
 * module in registration order, each module's from its last; they receive
 * the command-line arguments), the start function, then the EXIT procedures
 * (module by module, each module's in order), also after a QUIT, a BREAK or
 * an error that ended the program. A request to unwind stops the
 * initializations, ErrorSys() and the INIT procedures (the start function does
 * not run) and the EXIT procedures. `symbols` are the module's public
 * functions, by upper-cased name, for lookups by name at run time (Do()). */
typedef void (*xb_module_init)(xb_ctx *ctx);
typedef struct xb_symbol {
  const char *name;
  xb_func fn;
} xb_symbol;
/* [ABI] What a function reference designates (@Name(), an XB_T_SYMBOL item's
 * v.p): the function's upper-cased name, its entry, and whether it is a
 * runtime function (written in C), which receives references only where
 * `byref` has a bit (bit k: argument k, the last bit for the rest); a compiled
 * function receives them all. Compiled code emits one, constant, for each
 * function it references; the symbol table holds one for each name (macros).
 * xb_funcref_call calls it with arguments, as Eval(), Do() and :exec() do. */
typedef struct xb_funcref {
  const char *name;
  xb_func fn;
  uint32_t native;
  uint32_t byref;
} xb_funcref;
void xb_funcref_call(xb_ctx *ctx, xb_item *ret, const xb_funcref *f, uint32_t argc, xb_item *argv);
/* The arguments the function receives as references (bits, as `byref`). */
static inline uint32_t xb_funcref_refs(const xb_funcref *f) { return f->native ? f->byref : UINT32_MAX; }
/* [ABI] The name of a memory variable a module uses (PRIVATE, PUBLIC,
 * PARAMETERS, MEMVAR, M->, an undeclared name): one per name and module,
 * numbered by the runtime when the module registers (`memvars`). */
typedef struct xb_memvar {
  const char *name;         /* upper-cased */
  uint32_t id;              /* set by xb_module_register */
} xb_memvar;
/* [ABI] A module's level, which orders the modules whatever order the system runs their constructors in: a module of
 * the runtime is 0, one of an xbase library 1 + the highest level of the libraries it uses (its module file's
 * `level`), one of the program XB_LEVEL_PROGRAM. Modules initialize (STATICs, then INIT procedures) from the lowest
 * level up and run their EXIT procedures from the highest down, registration order within a level; of the functions
 * (and classes) of one name, the highest level's is the name's (the program's over a library's). */
#define XB_LEVEL_PROGRAM UINT32_MAX
void     xb_module_register(xb_module_init init, xb_item *statics, uint32_t nstatics, const xb_func *procs,
                            uint32_t ninit, uint32_t nexit, const xb_symbol *symbols, uint32_t nsymbols,
                            xb_memvar *memvars, uint32_t nmemvars, uint32_t level);

/* Memory variables (core/memvar.c), per context. The name designates the
 * variable visible now: the latest PRIVATE of that name still alive (one
 * created by an activation running now), else the PUBLIC. A PRIVATE ends with
 * the activation (function or codeblock call) that created it: one that may
 * create some takes the mark at its start and releases to it at its end, also
 * when it unwinds, and the variables the PRIVATEs hid show again. Errors and
 * requests to unwind are in the pending flag, as for the operators.
 * - get: the value; no variable is error 1003 (Variable does not exist),
 *   which its block may retry. ref: what an argument @name passes, the same way.
 * - set: assigns it; with none visible, a PRIVATE of the activation running
 *   now is created first (PRIVATEs nest: the latest are its own).
 * - private: PRIVATE name: a new variable (NIL), unless the activation, whose
 *   mark is `mark`, has one of that name already. public: PUBLIC name: a new
 *   variable (.F.), unless one of that name is visible.
 * - param: PARAMETERS: a PRIVATE of the activation holding `arg`, the
 *   variable itself when it was passed by reference. */
uint32_t xb_mv_mark(xb_ctx *ctx);
void     xb_mv_release(xb_ctx *ctx, uint32_t mark);
void     xb_mv_get(xb_ctx *ctx, xb_item *r, const xb_memvar *m);
void     xb_mv_ref(xb_ctx *ctx, xb_item *r, const xb_memvar *m);
void     xb_mv_set(xb_ctx *ctx, const xb_memvar *m, const xb_item *v);
void     xb_mv_private(xb_ctx *ctx, const xb_memvar *m, uint32_t mark);
void     xb_mv_public(xb_ctx *ctx, const xb_memvar *m);
void     xb_mv_param(xb_ctx *ctx, const xb_memvar *m, const xb_item *arg);
void     xb_program_run(xb_ctx *ctx, xb_func start, uint32_t argc, xb_item *argv);

/* [ABI] Fields of work areas (rdd/field.c). A field reference of compiled code has its own cache, zeroed: the table
 * where it last found the field (the work area's generation) and the field's position there, in one 64-bit word read
 * and written at once (threads share it; the compiler aligns it to 8 bytes), so a reference to the same table looks
 * for nothing. get and set reach the field `name` of the work area of `alias` (NULL: the current
 * one), or of the one the item `area`, when not NULL, names (an alias or a number); a missing alias (1002) or field
 * (1003) is an error the block may retry; on the blank record after the last a write does nothing. mv_field reads
 * an undeclared name: the current work area's field of that name, else the memory variable (as xb_mv_get). */
#ifdef __cplusplus
#define XB_ALIGN8 alignas(8)
#else
#define XB_ALIGN8 _Alignas(8)
#endif
typedef struct xb_field_cache {
  XB_ALIGN8 uint64_t key; /* the runtime's (rdd/field.c); compiled code only makes it, zero */
} xb_field_cache;
void     xb_field_get(xb_ctx *ctx, xb_item *r, const xb_item *area, const char *alias, const char *name,
                      xb_field_cache *cache);
void     xb_field_set(xb_ctx *ctx, const xb_item *area, const char *alias, const char *name, xb_field_cache *cache,
                      const xb_item *v);
void     xb_mv_field(xb_ctx *ctx, xb_item *r, const xb_memvar *m, xb_field_cache *cache);

/* [ABI] Classes compiled from xBase (types/class.c). A module describes each
 * of its classes (xb_class_def) and registers them when it registers
 * (xb_classes_register; a STATIC class, XB_CLASS_STATIC, is not registered:
 * it is known to its module only). A parent is the module's own class when it
 * has one of that name (`def`), else the class registered under the name,
 * found when the class is first used. A method is a function of the xBase
 * convention whose argv[0] is the object (Self); a variable is read by its
 * name as a message and assigned by "_NAME", an ACCESS method answers "NAME"
 * and an ASSIGN one "_NAME". A class variable (`classVars`) holds one value
 * for the class, another for each subclass, unless it is XB_MEMBER_SHARED:
 * then the class and its subclasses share it. A member's `flags`: the scope
 * (XB_SCOPE_*), and XB_MEMBER_READONLY for a variable assigned as if one
 * scope narrower (EXPORTED as PROTECTED, PROTECTED as HIDDEN). The friends (classes, whose own
 * methods, and functions, which with the codeblocks they make) reach the
 * class's HIDDEN and PROTECTED members and assign its READONLY ones. `init`
 * returns an array with the initial values (INIT) of the variables, then of
 * the class variables, evaluated once, the first time the class is used; each
 * new object gets its own copy of an array or a hash, each subclass the value
 * itself. `destructor` (a method of no arguments) runs once an object of the
 * class, or of a subclass, is unreachable. `interfaces` names the interfaces
 * the class implements (IMPLEMENTS; their `def` the module's own, else NULL):
 * an interface is a def of XB_CLASS_INTERFACE whose methods (no `fn`) are the
 * messages it requires and whose parents are the interfaces it inherits.
 * XB_MEMBER_ABSTRACT marks a method declared without a body (its `fn` raises
 * the error), XB_MEMBER_FINAL one no subclass may declare again. A class makes
 * objects when it is not ABSTRACT and breaks none of those rules
 * (xb_class_check, xbclass.h). `runtime` is the runtime's, NULL until then. */
#define XB_SCOPE_EXPORTED   0u
#define XB_SCOPE_PROTECTED  1u  /* the class's methods and its subclasses' */
#define XB_SCOPE_HIDDEN     2u  /* the class's own methods */
#define XB_SCOPE_MASK       3u
#define XB_MEMBER_READONLY  4u
#define XB_MEMBER_SHARED    8u  /* a class variable whose subclasses share it */
#define XB_MEMBER_ABSTRACT  16u /* a method without a body: a subclass that makes objects implements it */
#define XB_MEMBER_FINAL     32u /* a method no subclass declares again */
#define XB_CLASS_STATIC     1u  /* xb_class_def flags: STATIC CLASS */
#define XB_CLASS_SCALAR     2u  /* SCALAR CLASS: the class of the values of a type, named as ClassName() names it */
#define XB_CLASS_RUNTIME    4u  /* a class of the runtime (xbc --runtime): a program's SCALAR CLASS of a type answers
                                   first, whatever order the modules register in */
#define XB_CLASS_ABSTRACT   8u  /* ABSTRACT CLASS: it makes no objects (its subclasses do) */
#define XB_CLASS_FINAL      16u /* FINAL CLASS: no class inherits from it */
#define XB_CLASS_INTERFACE  32u /* INTERFACE: the messages a class that IMPLEMENTS it answers; no objects, no
                                   parent of a class */
/* What ClassName() gives for values that are not objects: the names of the types' classes (SCALAR CLASS). */
#define XB_TYPE_NAME_NIL       "NIL"
#define XB_TYPE_NAME_LOGICAL   "LOGICAL"
#define XB_TYPE_NAME_NUMERIC   "NUMERIC"
#define XB_TYPE_NAME_CHARACTER "CHARACTER"
#define XB_TYPE_NAME_ARRAY     "ARRAY"
#define XB_TYPE_NAME_BLOCK     "BLOCK"
#define XB_TYPE_NAME_DATE      "DATE"
#define XB_TYPE_NAME_HASH      "HASH"
#define XB_TYPE_NAME_SYMBOL    "SYMBOL"
/* The class name of a type's values (one of the above), or NULL for a tag no value shows (a cell, a reference) or an
 * object (its own class). */
static inline const char *xb_type_class_name(uint32_t type) {
  switch (type) {
  case XB_T_NIL: return XB_TYPE_NAME_NIL;
  case XB_T_LOGICAL: return XB_TYPE_NAME_LOGICAL;
  case XB_T_INT: case XB_T_DOUBLE: return XB_TYPE_NAME_NUMERIC;
  case XB_T_STRING: return XB_TYPE_NAME_CHARACTER;
  case XB_T_ARRAY: return XB_TYPE_NAME_ARRAY;
  case XB_T_BLOCK: return XB_TYPE_NAME_BLOCK;
  case XB_T_DATE: return XB_TYPE_NAME_DATE;
  case XB_T_HASH: return XB_TYPE_NAME_HASH;
  case XB_T_SYMBOL: return XB_TYPE_NAME_SYMBOL;
  default: return NULL;
  }
}
typedef struct xb_class_var_def {
  const char *name;         /* upper-cased */
  uint32_t flags;
} xb_class_var_def;
typedef struct xb_class_method_def {
  const char *message;      /* upper-cased; "_NAME" for an ASSIGN */
  xb_func fn;
  uint32_t flags;
} xb_class_method_def;
typedef struct xb_class_parent {
  const char *name;         /* upper-cased */
  struct xb_class_def *def; /* the module's own class of the name, else NULL */
} xb_class_parent;
typedef struct xb_class_def {
  const char *name;         /* upper-cased */
  const xb_class_parent *parents;
  uint32_t nparents;
  const xb_class_var_def *vars;
  uint32_t nvars;
  const xb_class_method_def *methods;
  uint32_t nmethods;
  const xb_class_var_def *classVars;
  uint32_t nclassVars;
  const char *const *friendClasses;   /* upper-cased names */
  uint32_t nfriendClasses;
  const char *const *friendFunctions;
  uint32_t nfriendFunctions;
  xb_func init;             /* NULL: every variable starts as NIL */
  xb_func destructor;       /* DESTRUCTOR, or NULL */
  const xb_class_parent *interfaces; /* IMPLEMENTS */
  uint32_t ninterfaces;
  uint32_t flags;           /* XB_CLASS_* */
  void *runtime;
} xb_class_def;
void     xb_classes_register(xb_class_def *defs, uint32_t n, uint32_t level);
/* The class function NAME(): a new object of the class, its variables at their
 * initial values (:New() then initializes it). */
void     xb_class_instance(xb_ctx *ctx, xb_item *r, xb_class_def *def);
/* The messages of the operators a class overloads (OPERATOR "+" ...): an
 * object that is the left (or only) operand receives the right operand as its
 * argument; [] receives the index, and the value when it assigns. "u-" is
 * unary minus (OPERATOR "-" UNARY); .AND. and .OR. receive the right side's
 * value, which they evaluate only for an object with the operator. None is a
 * name a method can have. */
#define XB_OPERATOR_ADD      "+"
#define XB_OPERATOR_SUB      "-"
#define XB_OPERATOR_MUL      "*"
#define XB_OPERATOR_DIV      "/"
#define XB_OPERATOR_MOD      "%"
#define XB_OPERATOR_POW      "^"
#define XB_OPERATOR_EQ       "="
#define XB_OPERATOR_EXACT_EQ "=="
#define XB_OPERATOR_NE       "!="
#define XB_OPERATOR_LT       "<"
#define XB_OPERATOR_LE       "<="
#define XB_OPERATOR_GT       ">"
#define XB_OPERATOR_GE       ">="
#define XB_OPERATOR_IN       "$"
#define XB_OPERATOR_NOT      "!"
#define XB_OPERATOR_INC      "++"
#define XB_OPERATOR_DEC      "--"
#define XB_OPERATOR_INDEX    "[]"
#define XB_OPERATOR_NEG      "u-"
#define XB_OPERATOR_AND      ".AND."
#define XB_OPERATOR_OR       ".OR."
#define XB_OPERATOR_BITAND   "&"
#define XB_OPERATOR_BITOR    "|"
#define XB_OPERATOR_BITXOR   "^^"
#define XB_OPERATOR_SHL      "<<"
#define XB_OPERATOR_SHR      ">>"
#define XB_OPERATOR_BITNOT   "~"
/* [ABI] Whether `a` is an object whose class (as a cast shows it) overloads
 * operator `op` (an XB_OPERATOR_* message): .AND. and .OR. test it on their
 * left side before anything else. */
int      xb_class_has_operator(const xb_item *a, const char *op);
/* [ABI] A message sent by compiled code: from a method of class `from` (its
 * scope decides what the method may reach, and a variable is first the one
 * `from` sees), or from elsewhere (NULL); with `super`, the method or variable
 * of `from`'s ancestors (Super:message). `cache` (NULL: none) is the send's
 * own, zeroed storage, which the runtime fills: the members the message
 * reached for the classes (xb_object.cls) of the last XB_SEND_CACHE_WAYS objects of different
 * classes it went to, the most recent first (a polymorphic inline cache), so
 * that the next message to an object of one of those classes skips the lookup
 * and the checks. Threads share it: `version` is odd while one changes it (a
 * sequence lock, types/class.c). Every field is pointer-sized: the compiler
 * emits it as XB_SEND_CACHE_WORDS words. For compiled code, which performs
 * the most recent way in place for an object sent to as itself (an item width
 * of 0: no cast), `kind` tells what the member is: XB_SEND_GET reads instance
 * variable `slot` and XB_SEND_SET writes it (with the argument) when the object
 * is local (not shared), XB_SEND_METHOD calls `fn`; XB_SEND_OTHER is left to
 * the runtime. XB_SEND_CACHE_* are the word positions of each array's first way. */
#define XB_SEND_CACHE_WAYS 8
#define XB_SEND_CACHE_WORDS (1 + 5 * XB_SEND_CACHE_WAYS)
#define XB_SEND_CACHE_CLS 1
#define XB_SEND_CACHE_SLOT (1 + 2 * XB_SEND_CACHE_WAYS)
#define XB_SEND_CACHE_KIND (1 + 3 * XB_SEND_CACHE_WAYS)
#define XB_SEND_CACHE_FN (1 + 4 * XB_SEND_CACHE_WAYS)
enum { XB_SEND_OTHER = 0, XB_SEND_GET = 1, XB_SEND_SET = 2, XB_SEND_METHOD = 3 };
typedef struct xb_send_cache {
  size_t version;
  void *cls[XB_SEND_CACHE_WAYS];
  void *member[XB_SEND_CACHE_WAYS];
  size_t slot[XB_SEND_CACHE_WAYS];
  size_t kind[XB_SEND_CACHE_WAYS];
  size_t fn[XB_SEND_CACHE_WAYS]; /* an xb_func as an integer (uintptr_t) */
} xb_send_cache;

/* [ABI] Instance variable `slot` of object `obj` (an XB_T_OBJECT item), read into `r`, or assigned `v` (the value
 * also into `r`), when it may be shared: under its lock (core/shared.h). Compiled code calls them for a send resolved
 * when compiling (the object's class known) on a shared object; it reads and writes a local one in place. */
void     xb_op_object_get(xb_item *r, const xb_item *obj, uint32_t slot);
void     xb_op_object_set(xb_item *r, const xb_item *obj, uint32_t slot, const xb_item *v);

/* [ABI] An object, an instance of a class: its class (a runtime structure, types/types.h) and its instance
 * variables, `nvars` items in the class's order. Compiled code reads `cls`, and reads and writes the items in place. */
typedef struct xb_class xb_class;
typedef struct xb_object xb_object;
#ifndef __cplusplus /* a flexible array member (the compiler mirrors the layout itself) */
struct xb_object {
  const xb_class *cls;
  uint32_t nvars;
  xb_item vars[];
};
#endif
void     xb_op_send_from(xb_ctx *ctx, xb_item *r, const char *message, xb_class_def *from, uint32_t super,
                         xb_send_cache *cache, uint32_t argc, xb_item *argv);

/* [ABI] Generators (types/generator.c): a function with YIELD returns an
 * enumerator (xb_gen_new: `resume`, the function's resume function, and its
 * arguments, which become slots 1 .. argc of the resume function's frame);
 * the resume function, whose argument (frame slot 0) is the enumerator,
 * starts with xb_gen_restore (its frame's other `nslots` - 1 slots back from
 * the enumerator; the state to resume in: 0 at the start), saves them and the
 * element with xb_gen_yield before it returns .T. (`state` goes on after that
 * YIELD), and calls xb_gen_done before it returns .F. at the end. The
 * enumerator's variables, which compiled code reads and writes in place while
 * it is local (XB_GC_STATE_SHARED clear): the element the last step yielded;
 * the state to resume in (an integer, XB_GEN_ENDED once it has ended and while
 * a step runs); the frame between steps (always an array, whose item k is slot
 * k: the arguments until the first YIELD saves them all, nothing after the
 * end); the resume function. */
#define XB_GEN_CURRENT 0u
#define XB_GEN_STATE   1u
#define XB_GEN_FRAME   2u
#define XB_GEN_RESUME  3u
#define XB_GEN_VARS    4u
#define XB_GEN_ENDED   (-1)
extern const xb_class xb_generator_class; /* the class of every generator's enumerator */
void     xb_gen_new(xb_ctx *ctx, xb_item *r, xb_func resume, uint32_t argc, const xb_item *argv);
int32_t  xb_gen_restore(xb_ctx *ctx, const xb_item *gen, xb_item *slots, uint32_t nslots);
void     xb_gen_yield(xb_ctx *ctx, const xb_item *gen, const xb_item *slots, uint32_t nslots, uint32_t state,
                      const xb_item *value);
void     xb_gen_done(xb_ctx *ctx, const xb_item *gen);

/* [ABI] Asynchronous functions (rtl/tasks.c, docs/concurrency.md#asynchronous-functions). Calling one runs its
 * first step at once, on the caller's thread (xb_async_new, from its entry `entry`: `resume`, the function's resume
 * function, and its arguments, slots 1 .. argc of the resume function's frame); `*r` is its task: made when the step
 * suspends, or made ended. The resume function, whose argument (frame slot 0) is the task, NIL in the first step,
 * starts with xb_async_restore (its frame's other `nslots` - 1 slots: the arguments, or the frame saved; the point to
 * resume at: 0 at the start). At an AWAIT of `*v`, xb_await_ready tells whether it goes on at once (no task, or one
 * that ended); if not, the function saves its slots with xb_async_suspend (`point` goes on after that AWAIT, `awaited`
 * the task) and returns: the task resumes once `awaited` ends, on a worker. xb_await_result gives the value (a task's
 * result; a failure or a cancellation raised as Result raises them). RETURN returns the task's result. xb_await_wait
 * is AWAIT outside an asynchronous function (the start procedure): it waits, then as xb_await_result.
 * AWAIT of a call to an asynchronous function: xb_await_fuse(entry) right before the call (NULL `entry`: a send, whose
 * compiled method is the entry) asks that function's first step, when it ends without suspending, for its result itself
 * instead of a task (no task made); xb_await_fused right after the call tells whether `*r` is that result (1) or a task
 * (0), and ends the request (an error ends it too).
 * An asynchronous generator (an ASYNC function with YIELD; rtl/agen.c): calling it makes its enumerator
 * (xb_agen_new: `resume` and the arguments kept) and runs nothing; its resume function is an asynchronous function's,
 * whose task drives the body, and a YIELD saves its slots with xb_async_yield (`point` goes on after it, `*value` the
 * element) and returns: the body goes on at the next MoveNextAsync(). */
void     xb_async_new(xb_ctx *ctx, xb_item *r, xb_func entry, xb_func resume, uint32_t argc, xb_item *argv);
void     xb_agen_new(xb_ctx *ctx, xb_item *r, xb_func resume, uint32_t argc, const xb_item *argv);
void     xb_async_yield(xb_ctx *ctx, const xb_item *slots, uint32_t nslots, uint32_t point, const xb_item *value);
int32_t  xb_async_restore(xb_ctx *ctx, xb_item *slots, uint32_t nslots);
void     xb_async_suspend(xb_ctx *ctx, const xb_item *slots, uint32_t nslots, uint32_t point, const xb_item *awaited);
int32_t  xb_await_ready(const xb_item *v);
void     xb_await_result(xb_ctx *ctx, xb_item *r, const xb_item *v);
void     xb_await_wait(xb_ctx *ctx, xb_item *r, const xb_item *v);
void     xb_await_fuse(xb_ctx *ctx, xb_func entry);
int32_t  xb_await_fused(xb_ctx *ctx);

/* The macro operator (macro/run.c): `text` holds an expression, compiled when
 * the code runs; it sees memory variables and functions by name, never the
 * caller's LOCALs. xb_macro_get stores its value in `r` (&text);
 * xb_macro_set assigns `value` to what it designates (&text := value);
 * xb_macro_ref stores in `r` a reference to the variable or element it
 * designates (@&text, an argument passed by reference);
 * xb_macro_call calls the function it names (&text( args ), references as
 * in Do()); xb_macro_send sends the message it names (o:&text( args ),
 * argv[0] the text, then the object and the arguments; `assign`: the
 * assignment o:&text := value, `from` and `super` as in xb_op_send_from;
 * no inline cache); xb_macro_private and xb_macro_public create the variable it
 * names (PRIVATE &text, `mark` the activation's, and PUBLIC &text). A text
 * that is not a string is error 1065, one that is not an expression (or a
 * name, or something assignable) error 1449; both may be substituted. */
void     xb_macro_get(xb_ctx *ctx, xb_item *r, const xb_item *text);
void     xb_macro_set(xb_ctx *ctx, const xb_item *text, const xb_item *value);
void     xb_macro_ref(xb_ctx *ctx, xb_item *r, const xb_item *text);
void     xb_macro_call(xb_ctx *ctx, xb_item *r, const xb_item *text, uint32_t argc, xb_item *argv);
void     xb_macro_send(xb_ctx *ctx, xb_item *r, xb_class_def *from, uint32_t super, uint32_t assign, uint32_t argc,
                       xb_item *argv);
void     xb_macro_private(xb_ctx *ctx, const xb_item *text, uint32_t mark);
void     xb_macro_public(xb_ctx *ctx, const xb_item *text);
/* Clipper's text substitution, for a string literal with `&name` in it and
 * for the text of a macro before it is compiled: each `&name` or `&name.`
 * whose name is a memory variable holding a string becomes that string (the
 * scan goes on through what was inserted); the rest stays as written. */
void     xb_macro_text(xb_ctx *ctx, xb_item *r, const xb_item *text);

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
/* BEGIN SEQUENCE WITH handler: xb_seq_with makes the handler (a block; any
 * other value changes nothing) the error block and gives the one it replaces,
 * which xb_seq_restore puts back however the body ends. ALWAYS, on the way out
 * of an unwinding: xb_seq_suspend sets the requests to unwind aside (returned;
 * the BREAK value is taken with xb_seq_value), and xb_seq_resume makes them
 * pending again with that value, unless the ALWAYS code made a request of its
 * own, which replaces them (a QUIT set aside stays). */
void     xb_seq_with(xb_ctx *ctx, xb_item *r, const xb_item *handler);
void     xb_seq_restore(xb_ctx *ctx, const xb_item *old);
uint32_t xb_seq_suspend(xb_ctx *ctx);
void     xb_seq_resume(xb_ctx *ctx, uint32_t requests, const xb_item *value);

xb_ctx  *xb_ctx_new(void);
void     xb_ctx_free(xb_ctx *ctx);
void     xb_ctx_raise(xb_ctx *ctx, uint32_t what);

/* [ABI] The 256 strings of one byte, never collected (XB_STR_STATIC): what
 * xb_str_new and xb_str_concat return for a one-byte result (Chr(),
 * SubStr( c, n, 1 ), ...) instead of a new string; compiled code takes them
 * from this table. The layout of xb_str with a two-byte payload. */
typedef struct xb_str1 {
  uint32_t flags;
  uint32_t hash;
  size_t   len;
  char     bytes[2];
} xb_str1;
extern const xb_str1 xb_str_bytes[256];

/* A string of `len` bytes from `p` (the shared empty or one-byte string for
 * up to one byte), and the concatenation of two. */
xb_str  *xb_str_new(const char *p, size_t len);
xb_str  *xb_str_concat(const xb_str *a, const xb_str *b);
/* [ABI] Left( s, n ), Right( s, n ) and SubStr( s, start [, count] ) in bytes, of a string and numbers already taken
 * as integers (xb_num_to_i64): `s` itself when the result is all of it, else a new string (the shared one for up to
 * a byte). They raise no error: compiled code that knows its arguments' types calls them in place of the functions,
 * which call them too (rtl/strings.c). */
xb_str  *xb_str_left(xb_str *s, int64_t n);
xb_str  *xb_str_right(xb_str *s, int64_t n);
xb_str  *xb_str_sub(xb_str *s, int64_t start, int has_count, int64_t count);

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
#define XB_GC_STATE_SHARED   0x20u /* bit: reachable by more than one flow (docs/concurrency.md) */
#define XB_GC_STATE_ELEMS_SHIFT 6u
#define XB_GC_STATE_ELEMS_MASK 3u
#define XB_GC_STATE_ELEMS    (XB_GC_STATE_ELEMS_MASK << XB_GC_STATE_ELEMS_SHIFT) /* bits: an array's form */
void xb_gc_remember(void *obj);
/* The barrier's test on an object's state byte: an old object not yet recorded is recorded. */
static inline int xb_gc_needs_record(uint8_t state) {
  return (state & (XB_GC_STATE_OLD | XB_GC_STATE_RECORDED)) == XB_GC_STATE_OLD;
}
static inline void xb_gc_write(void *obj) {
  /* Other threads set bits of the byte (atomically): it is read atomically too. */
#if defined(__GNUC__) || defined(__clang__)
  uint8_t state = __atomic_load_n((const uint8_t *)obj - XB_GC_STATE_BACK, __ATOMIC_RELAXED);
#else
  uint8_t state = *((const volatile uint8_t *)obj - XB_GC_STATE_BACK);
#endif
  if (xb_gc_needs_record(state)) xb_gc_remember(obj);
}

/* Whether a double converts to int64_t without overflow (false for NaN and
 * infinities). 2^63 is exact in a double; INT64_MAX is not. */
static inline int xb_dbl_fits_i64(double d) { return d >= -9223372036854775808.0 && d < 9223372036854775808.0; }
/* A double as an integer: truncated toward zero, saturated to 64 bits, NaN
 * as 0 (how functions and the bit operators take a number as an integer). */
static inline int64_t xb_dbl_to_i64(double d) {
  if (xb_dbl_fits_i64(d)) return (int64_t)d;
  if (d != d) return 0;
  return d > 0 ? INT64_MAX : INT64_MIN;
}

/* ASCII letter case: other bytes pass unchanged (keywords, letter case aside
 * in hash keys; the compiler hashes literal keys with the same rule). */
static inline char xb_ascii_upper(char c) { return (char)(c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c); }
static inline char xb_ascii_lower(char c) { return (char)(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c); }

/* [ABI] Hashing (hash keys). xb_hash_mix64 scrambles 64 bits (the finalizer
 * of MurmurHash3). xb_hash_bytes is the hash of a string's bytes, read in a
 * fixed byte order so that every target computes the same value: the
 * compiler stores it in the `hash` field of the strings it emits, the
 * runtime computes it on demand for the others. It is never 0, which marks
 * a hash not computed yet. xb_hash_bytes_as hashes the bytes as given, or
 * with ASCII letters upper-cased (`upper`: keys compared letter case aside). */
static inline uint64_t xb_hash_mix64(uint64_t x) {
  x ^= x >> 33;
  x *= UINT64_C(0xff51afd7ed558ccd);
  x ^= x >> 33;
  x *= UINT64_C(0xc4ceb9fe1a85ec53);
  x ^= x >> 33;
  return x;
}
/* Bytes p[0] .. p[n - 1] (n at most 8) as a little-endian word; with `upper`, ASCII letters upper-cased. Eight bytes
 * without `upper` are a fixed pattern compilers make one load (and a byte swap on a big-endian target). */
static inline uint64_t xb_hash_word(const char *p, size_t n, int upper) {
  uint64_t w = 0;
  if (n == 8 && !upper) {
    for (size_t i = 0; i < 8; ++i) w |= (uint64_t)(unsigned char)p[i] << (8 * i);
    return w;
  }
  for (size_t i = 0; i < n; ++i) {
    unsigned char c = (unsigned char)(upper ? xb_ascii_upper(p[i]) : p[i]);
    w |= (uint64_t)c << (8 * i);
  }
  return w;
}
static inline uint32_t xb_hash_bytes_as(const char *p, size_t n, int upper) {
  uint64_t h = UINT64_C(0x9e3779b97f4a7c15) ^ (uint64_t)n;
  while (n > 0) {
    size_t take = n < 8 ? n : 8;
    uint64_t w = xb_hash_word(p, take, upper);
    h = (h ^ (w * UINT64_C(0x87c37b91114253d5))) * UINT64_C(0x4cf5ad432745937f);
    h = (h << 31) | (h >> 33);
    p += take;
    n -= take;
  }
  h = xb_hash_mix64(h);
  uint32_t r = (uint32_t)(h ^ (h >> 32));
  return r ? r : 1u;
}
static inline uint32_t xb_hash_bytes(const char *p, size_t n) { return xb_hash_bytes_as(p, n, 0); }

/* Default display width of the integer part of a number (Clipper rule):
 * 10 columns, 20 when the value does not fit in 10 (above XB_NUM_FITS_MAX or
 * below XB_NUM_FITS_MIN). Shared by the compiler (numeric literals, and the
 * width code it generates for Int()) and the runtime (computed values). */
#define XB_NUM_WIDTH 10
#define XB_NUM_WIDE_WIDTH 20
#define XB_NUM_FITS_MAX INT64_C(9999999999)
#define XB_NUM_FITS_MIN INT64_C(-999999999)
static inline uint16_t xb_num_int_width(int64_t i) {
  return (uint16_t)((i < XB_NUM_FITS_MIN || i > XB_NUM_FITS_MAX) ? XB_NUM_WIDE_WIDTH : XB_NUM_WIDTH);
}
static inline uint16_t xb_num_dbl_width(double d) {
  return (uint16_t)((d > (double)XB_NUM_FITS_MAX || d < (double)XB_NUM_FITS_MIN) ? XB_NUM_WIDE_WIDTH : XB_NUM_WIDTH);
}

/* xbase's bit functions (BitAnd, BitOr, BitXor, BitNor, BitNot, BitShl,
 * BitShr) and bit operators (& | ^^ ~ << >>) work on the 64-bit two's
 * complement form of integers. BitShr is a logical shift, >> an arithmetic
 * one (XB_BIT_SAR: the sign fills the vacated bits); a shift count outside
 * 0..63 shifts every bit out (0, or -1 for >> of a negative number). The
 * runtime, the compiler's folding of constants and its inline code share
 * these rules. */
typedef enum xb_bit_op {
  XB_BIT_AND, XB_BIT_OR, XB_BIT_XOR, XB_BIT_NOR, XB_BIT_NOT, XB_BIT_SHL, XB_BIT_SHR, XB_BIT_SAR
} xb_bit_op;
static inline int64_t xb_bit_apply(xb_bit_op op, int64_t a, int64_t b) {
  uint64_t x = (uint64_t)a, y = (uint64_t)b;
  uint64_t sign = a < 0 ? ~UINT64_C(0) : 0; /* SAR without relying on >> of a negative int64_t */
  switch (op) {
  case XB_BIT_AND: return (int64_t)(x & y);
  case XB_BIT_OR: return (int64_t)(x | y);
  case XB_BIT_XOR: return (int64_t)(x ^ y);
  case XB_BIT_NOR: return (int64_t)~(x | y);
  case XB_BIT_NOT: return (int64_t)~x;
  case XB_BIT_SHL: return y < 64 ? (int64_t)(x << y) : 0;
  case XB_BIT_SHR: return y < 64 ? (int64_t)(x >> y) : 0;
  case XB_BIT_SAR: return (int64_t)(y < 64 ? ((x ^ sign) >> y) ^ sign : sign);
  }
  return 0;
}

/* Each bit operation: the name of its function (upper-cased, as calls are
 * resolved; NULL without one), the symbol of its operator (NULL without one;
 * the message a class's overload of it answers, XB_OPERATOR_*),
 * its operand count and the BASE error code of both (xbase's own codes start
 * at 9001). Indexed by xb_bit_op; the runtime's functions and operators and
 * the compiler's folding and inline code read this table. */
typedef struct xb_bit_function {
  xb_bit_op op;
  const char *name;
  const char *symbol;
  uint32_t arity;
  int code;
} xb_bit_function;
#define XB_BIT_OP_COUNT (XB_BIT_SAR + 1)
static inline const xb_bit_function *xb_bit_function_of(xb_bit_op op) {
  static const xb_bit_function table[XB_BIT_OP_COUNT] = {
      {XB_BIT_AND, "BITAND", XB_OPERATOR_BITAND, 2, 9001}, {XB_BIT_OR, "BITOR", XB_OPERATOR_BITOR, 2, 9002},
      {XB_BIT_XOR, "BITXOR", XB_OPERATOR_BITXOR, 2, 9003}, {XB_BIT_NOR, "BITNOR", NULL, 2, 9004},
      {XB_BIT_NOT, "BITNOT", XB_OPERATOR_BITNOT, 1, 9005}, {XB_BIT_SHL, "BITSHL", XB_OPERATOR_SHL, 2, 9006},
      {XB_BIT_SHR, "BITSHR", NULL, 2, 9007},               {XB_BIT_SAR, NULL, XB_OPERATOR_SHR, 2, 9008},
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

/* 64-bit integer arithmetic reporting overflow (returns non-zero) instead of
 * invoking undefined behavior. */
static inline int xb_add_overflow(int64_t a, int64_t b, int64_t *r) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_add_overflow(a, b, r);
#else
  if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return 1;
  *r = a + b;
  return 0;
#endif
}

static inline int xb_sub_overflow(int64_t a, int64_t b, int64_t *r) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_sub_overflow(a, b, r);
#else
  if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b)) return 1;
  *r = a - b;
  return 0;
#endif
}

static inline int xb_mul_overflow(int64_t a, int64_t b, int64_t *r) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_mul_overflow(a, b, r);
#else
  if (a > 0 ? (b > 0 ? a > INT64_MAX / b : b < INT64_MIN / a)
            : (b > 0 ? a < INT64_MIN / b : (a != 0 && b < INT64_MAX / a)))
    return 1;
  *r = a * b;
  return 0;
#endif
}

/* A number known before the code runs, as the folding of constants computes
 * with it (the compiler's, on its syntax tree and its xIR, and the macro
 * compiler's): an integer with its display width (0: a computed one, as the
 * run-time operators make them, else a program constant's,
 * xb_num_const_int_width), or a double with the default width and its
 * decimals, XB_DEC_AT_RUN when they are SET DECIMALS when the code runs (a
 * quotient, a remainder, a power: the code sets them then). */
#define XB_DEC_AT_RUN 255
typedef struct xb_folded {
  int is_int;
  int64_t i;
  double d;
  uint16_t width, dec;
} xb_folded;
typedef enum xb_fold_op { XB_FOLD_ADD, XB_FOLD_SUB, XB_FOLD_MUL, XB_FOLD_DIV, XB_FOLD_MOD, XB_FOLD_POW } xb_fold_op;

static inline xb_folded xb_folded_int(int64_t v, int computed) {
  xb_folded r = {1, v, 0.0, 0, 0};
  if (!computed) r.width = xb_num_const_int_width(v);
  return r;
}
static inline xb_folded xb_folded_double(double v, uint16_t dec) {
  xb_folded r = {0, 0, v, 0, dec};
  return r;
}

/* `a op b` into *r, or 0 when it is left to run time (a division by zero).
 * Without `runtime`, as literals fold (Clipper's compile-time rules: an exact
 * division of two integers is an integer, an integer product that overflows a
 * double with no decimals, a quotient's decimals SET DECIMALS when the code
 * runs); with it, as the run-time operator makes it from two values (an
 * integer is a computed one; /, %, ^ and an integer product that overflows,
 * whose decimals are SET DECIMALS, and a product with more decimals than a
 * literal holds, give 0). */
static inline int xb_fold_numbers(xb_fold_op op, const xb_folded *a, const xb_folded *b, int runtime, xb_folded *r) {
  int ints = a->is_int && b->is_int;
  int computed = runtime || (a->is_int && a->width == 0) || (b->is_int && b->width == 0);
  double x = a->is_int ? (double)a->i : a->d, y = b->is_int ? (double)b->i : b->d;
  int64_t v;
  switch (op) {
  case XB_FOLD_ADD:
  case XB_FOLD_SUB:
    if (!ints)
      *r = xb_folded_double(op == XB_FOLD_ADD ? x + y : x - y, (uint16_t)(a->dec > b->dec ? a->dec : b->dec));
    else if (!(op == XB_FOLD_ADD ? xb_add_overflow(a->i, b->i, &v) : xb_sub_overflow(a->i, b->i, &v)))
      *r = xb_folded_int(v, computed);
    else
      *r = xb_folded_double(op == XB_FOLD_ADD ? x + y : x - y, 0);
    return 1;
  case XB_FOLD_MUL:
    if (ints) {
      if (!xb_mul_overflow(a->i, b->i, &v)) *r = xb_folded_int(v, computed);
      else if (runtime) return 0; /* the run-time operator's double has SET DECIMALS decimals */
      else *r = xb_folded_double(x * y, 0);
      return 1;
    }
    if (runtime && a->dec + b->dec >= XB_DEC_AT_RUN) return 0; /* decimals no literal holds */
    *r = xb_folded_double(x * y, (uint16_t)(a->dec + b->dec < XB_DEC_AT_RUN ? a->dec + b->dec : XB_DEC_AT_RUN));
    return 1;
  case XB_FOLD_DIV:
    if (runtime || y == 0.0) return 0; /* a division by zero is the run-time error */
    if (ints && !(a->i == INT64_MIN && b->i == -1) && a->i % b->i == 0) *r = xb_folded_int(a->i / b->i, computed);
    else *r = xb_folded_double(x / y, XB_DEC_AT_RUN);
    return 1;
  case XB_FOLD_MOD:
    if (runtime || y == 0.0) return 0;
    if (ints) *r = xb_folded_int(b->i == -1 ? 0 : a->i % b->i, computed); /* INT64_MIN % -1 is undefined in C */
    else *r = xb_folded_double(fmod(x, y), XB_DEC_AT_RUN);
    return 1;
  case XB_FOLD_POW:
    if (runtime) return 0;
    *r = xb_folded_double(xb_math_pow(x, y), XB_DEC_AT_RUN);
    return 1;
  }
  return 0;
}

/* The comparisons folding computes, of an order (-1, 0, 1: xb_order_is): = and == (the same for two numbers or
 * logicals), !=, <, <=, >, >=. */
typedef enum xb_fold_cmp { XB_FOLD_EQ, XB_FOLD_NE, XB_FOLD_LT, XB_FOLD_LE, XB_FOLD_GT, XB_FOLD_GE } xb_fold_cmp;
static inline int xb_fold_compare(xb_fold_cmp op, int order) {
  switch (op) {
  case XB_FOLD_EQ: return xb_order_is(order, 0, 1, 0);
  case XB_FOLD_NE: return !xb_order_is(order, 0, 1, 0);
  case XB_FOLD_LT: return xb_order_is(order, 1, 0, 0);
  case XB_FOLD_LE: return xb_order_is(order, 1, 1, 0);
  case XB_FOLD_GT: return xb_order_is(order, 0, 0, 1);
  case XB_FOLD_GE: return xb_order_is(order, 0, 1, 1);
  }
  return 0;
}
/* `a op b` when either is NIL (and neither an object, whose class may define the operator), into *r (0: an operator
 * other than = == != fails): = and == are true when both are NIL, != when one is not (the runtime's xb_op_eq,
 * xb_op_exacteq, xb_op_ne, and the folding of constants). */
static inline int xb_fold_nil(xb_fold_cmp op, int aNil, int bNil, int *r) {
  if (op != XB_FOLD_EQ && op != XB_FOLD_NE) return 0;
  *r = (aNil == bNil) == (op == XB_FOLD_EQ);
  return 1;
}
/* The order of two numbers: integers exactly, else as doubles (xb_order_double). */
static inline int xb_folded_order(const xb_folded *a, const xb_folded *b) {
  if (a->is_int && b->is_int) return (a->i > b->i) - (a->i < b->i);
  return xb_order_double(a->is_int ? (double)a->i : a->d, b->is_int ? (double)b->i : b->d);
}

/* Byte k of `s` (`n` bytes) read from its start, or from its end with `back` (the reversed string): the two-way search
 * below finds the last occurrence as the first one of the reversed strings. */
static inline unsigned char xb_find_at(const char *s, size_t n, size_t k, int back) {
  return (unsigned char)s[back ? n - 1 - k : k];
}

/* The critical factorization of x (`lx` bytes, read as xb_find_at reads them): where x splits for the two-way search,
 * and its period in `*period` (Crochemore and Perrin's maximal suffixes for both byte orders). */
static inline size_t xb_find_factor(const char *x, size_t lx, int back, size_t *period) {
  size_t suffix[2], per[2];
  for (int order = 0; order < 2; ++order) {
    size_t ms = (size_t)-1, j = 0, k = 1, p = 1; /* ms: the maximal suffix's start - 1 (-1: none yet) */
    while (j + k < lx) {
      unsigned char a = xb_find_at(x, lx, j + k, back), b = xb_find_at(x, lx, ms + k, back);
      if (order ? b < a : a < b) {
        j += k;
        k = 1;
        p = j - ms;
      } else if (a == b) {
        if (k != p) {
          ++k;
        } else {
          j += p;
          k = 1;
        }
      } else {
        ms = j++;
        k = p = 1;
      }
    }
    suffix[order] = ms + 1;
    per[order] = p;
  }
  int pick = suffix[1] > suffix[0];
  *period = per[pick];
  return suffix[pick];
}

/* The first occurrence of x in y (both read as xb_find_at reads them): its offset, or (size_t)-1. The two-way search
 * (linear in |y| whatever the bytes: no needle and haystack make it quadratic), for needles longer than a few bytes. */
static inline size_t xb_find_two_way(const char *x, size_t lx, const char *y, size_t ly, int back) {
  size_t period, suffix = xb_find_factor(x, lx, back, &period);
  int periodic = suffix + period <= lx;
  for (size_t i = 0; periodic && i < suffix; ++i)
    periodic = xb_find_at(x, lx, i, back) == xb_find_at(x, lx, i + period, back);
  if (periodic) { /* the needle repeats with its period: what matched already is remembered (`memory`) */
    size_t memory = 0, j = 0;
    while (j <= ly - lx) {
      size_t i = suffix > memory ? suffix : memory;
      while (i < lx && xb_find_at(x, lx, i, back) == xb_find_at(y, ly, i + j, back)) ++i;
      if (i < lx) {
        j += i - suffix + 1;
        memory = 0;
        continue;
      }
      i = suffix;
      while (i > memory && xb_find_at(x, lx, i - 1, back) == xb_find_at(y, ly, i - 1 + j, back)) --i;
      if (i <= memory) return j;
      j += period;
      memory = lx - period;
    }
    return (size_t)-1;
  }
  period = (suffix > lx - suffix ? suffix : lx - suffix) + 1;
  for (size_t j = 0; j <= ly - lx;) {
    size_t i = suffix;
    while (i < lx && xb_find_at(x, lx, i, back) == xb_find_at(y, ly, i + j, back)) ++i;
    if (i < lx) {
      j += i - suffix + 1;
      continue;
    }
    i = suffix;
    while (i > 0 && xb_find_at(x, lx, i - 1, back) == xb_find_at(y, ly, i - 1 + j, back)) --i;
    if (i == 0) return j;
    j += period;
  }
  return (size_t)-1;
}

/* Where the bytes x occur in y: the 1-based position of the first occurrence (`last`: of the last one), 0 when
 * nowhere or x is empty (At(), RAt(), `$`, StrTran()). A short needle is looked for with memchr and memcmp (fastest
 * for it, and a short needle bounds the work at each place); a longer one with the two-way search. */
#define XB_FIND_SHORT 16u
static inline size_t xb_bytes_find(const char *x, size_t lx, const char *y, size_t ly, int last) {
  if (lx == 0 || lx > ly) return 0;
  if (lx > XB_FIND_SHORT) {
    size_t at = xb_find_two_way(x, lx, y, ly, last);
    if (at == (size_t)-1) return 0;
    return last ? ly - lx - at + 1 : at + 1;
  }
  const char *end = y + ly - lx + 1; /* candidates are before `end` */
  if (last) {
    for (const char *p = end; p-- > y;)
      if (*p == *x && memcmp(p + 1, x + 1, lx - 1) == 0) return (size_t)(p - y) + 1;
    return 0;
  }
  for (const char *p = y; p < end; ++p) {
    p = (const char *)memchr(p, *x, (size_t)(end - p));
    if (!p) return 0;
    if (memcmp(p + 1, x + 1, lx - 1) == 0) return (size_t)(p - y) + 1;
  }
  return 0;
}

/* `x op y` of two strings when SET EXACT cannot change it, into *r (0: left to run time): == compares the bytes;
 * = and != the bytes of strings of one length (SET EXACT changes only how a longer string compares with a shorter
 * one); $ finds x in y (an empty x nowhere). */
typedef enum xb_fold_str { XB_FOLD_STR_EXACT, XB_FOLD_STR_EQ, XB_FOLD_STR_NE, XB_FOLD_STR_IN } xb_fold_str;
static inline int xb_fold_strings(xb_fold_str op, const char *x, size_t lx, const char *y, size_t ly, int *r) {
  if (op == XB_FOLD_STR_IN) {
    *r = xb_bytes_find(x, lx, y, ly, 0) != 0;
    return 1;
  }
  if (op != XB_FOLD_STR_EXACT && lx != ly) return 0;
  int same = lx == ly && (lx == 0 || memcmp(x, y, lx) == 0);
  *r = op == XB_FOLD_STR_NE ? !same : same;
  return 1;
}

/* -n, folded as `xb_fold_numbers` folds (with `runtime`, as the run-time
 * operator makes it): a double keeps its decimals, -INT64_MIN is a double. */
static inline xb_folded xb_fold_negate(const xb_folded *n, int runtime) {
  if (!n->is_int) return xb_folded_double(-n->d, n->dec);
  if (n->i == INT64_MIN) return xb_folded_double(-(double)n->i, 0);
  return xb_folded_int(-n->i, runtime || n->width == 0);
}

/* Display width of a numeric literal's integer part, written with
 * `intDigits` integer digits and `dec` decimals: an integer is a program
 * constant; a double with 10 or more integer digits keeps its own width.
 * Shared by the compiler and the macro compiler. */
static inline uint16_t xb_num_literal_width(int isDouble, int64_t ival, double dval, size_t intDigits, uint16_t dec) {
  if (!isDouble) return xb_num_const_int_width(ival);
  if (intDigits < 10) return xb_num_dbl_width(dval);
  return (uint16_t)(intDigits + (dec == 0 ? 1 : 0));
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

/* The rules of the string functions that the compiler also evaluates on constants (opt/Fold.cpp), so that a call
 * with literal arguments gives the value the run-time function gives:
 * - xb_is_blank: the blanks that leading trimming, Empty() and Val() skip;
 * - xb_trim_range: the part trimming keeps of `len` bytes (`which`: XB_TRIM_LEADING drops leading blanks,
 *   XB_TRIM_TRAILING trailing spaces), from *from;
 * - xb_substr_range: SubStr()'s units of a text of `len` units from `start` (negative: counted from the end; before
 *   the first: the first), `count` of them (`hasCount` 0: to the end): how many (0: none), from *first (0-based);
 * - xb_take_count: how many units Left() and Right() take of `units` for `n`: none for n <= 0;
 * - xb_count_arg: a repetition count (Replicate(), Space()): none when not positive;
 * - xb_chr_byte: Chr()'s byte, the code's low 8 bits; xb_asc_code: Asc()'s code, 0 for "";
 * - xb_upper_folds: whether the language evaluates Upper() of a literal when compiling: when it holds ASCII letters,
 *   digits and spaces only (the compiler, and the macro compiler, then treat its result as a literal). */
static inline int xb_is_blank(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
enum { XB_TRIM_LEADING = 1, XB_TRIM_TRAILING = 2 };
static inline size_t xb_trim_range(const char *p, size_t len, int which, size_t *from) {
  size_t a = 0, b = len;
  if (which & XB_TRIM_LEADING)
    while (a < b && xb_is_blank(p[a])) ++a;
  if (which & XB_TRIM_TRAILING)
    while (b > a && p[b - 1] == ' ') --b;
  *from = a;
  return b - a;
}
static inline int64_t xb_substr_range(int64_t len, int64_t start, int hasCount, int64_t count, int64_t *first) {
  if (start < 0) start += len + 1;
  if (start < 1) start = 1;
  if (!hasCount) count = len;
  *first = start - 1;
  if (start > len || count <= 0) return 0;
  return count > len - start + 1 ? len - start + 1 : count;
}
static inline size_t xb_take_count(int64_t n, size_t units) {
  if (n <= 0) return 0;
  return (uint64_t)n > units ? units : (size_t)n;
}
static inline uint64_t xb_count_arg(int64_t n) { return n > 0 ? (uint64_t)n : 0; }
static inline char xb_chr_byte(int64_t code) { return (char)(unsigned char)(code & 0xFF); }
static inline int64_t xb_asc_code(const char *p, size_t len) { return len ? (unsigned char)p[0] : 0; }
static inline int xb_upper_folds(const char *p, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    char c = p[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ')) return 0;
  }
  return 1;
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
