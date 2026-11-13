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

#define XB_ABI_VERSION 40u

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
  XB_T_COUNT_
} xb_type;
/* ValType() of each tag, by tag (a cell or a reference, which ValType never
 * sees, as NIL). */
#define XB_TYPE_LETTERS "ULNNCABUUOHD"
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
 * between items and threads. `data` is always NUL terminated (len excludes
 * the terminator) so it can be handed to C without copying. */
#define XB_STR_STATIC 0x1u   /* not a heap object, never freed: literals, the empty and one-byte strings */
#define XB_STR_APPEND 0x2u   /* made by xb_op_append and seen only by its variable: may grow in place;
                                `hash` then holds the bytes it has room for (its length at least) */

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
};
#define XB_PENDING_UNWIND (XB_PENDING_BREAK | XB_PENDING_QUIT | XB_PENDING_TRAP)

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

/* [ABI] What the call stack shows of a function (ProcName(), the "Called
 * from" lines): its name, "(b)NAME" for a codeblock of NAME; its number of
 * parameters, the first slots of its frame; its flags; and the name of its
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
 * while the body of a codeblock it runs in place (inlined) runs: `argc` and
 * `argv` are not set (its proc has no XB_PROC_ARGS). */
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
 * *stack_limit` (a function whose frame lies below it calls
 * xb_stack_overflow before its body runs, then returns unwinding; NULL: no
 * limit). */
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

/* The layout the module was compiled for: offsets of xb_str.data,
 * xb_block.cells, xb_frame.slots, xb_ctx.frames, xb_ctx.set_decimals,
 * xb_ctx.stack_limit, xb_array.items (after len and cap), xb_ref.value (after coll and index) and
 * xb_hash.entries (after its five counts). */
void     xb_abi_check(uint32_t version, uint32_t item_size, uint32_t str_hdr_size, uint32_t block_cells_offset,
                      uint32_t frame_slots_offset, uint32_t ctx_frames_offset, uint32_t ctx_decimals_offset,
                      uint32_t ctx_stack_limit_offset, uint32_t array_items_offset, uint32_t ref_value_offset,
                      uint32_t hash_entries_offset);
/* The stack error (EG_LIMIT, "Stack overflow"): raised by compiled code whose
 * frame lies below the context's stack_limit (core/ctx.c). */
void     xb_stack_overflow(xb_ctx *ctx);

void     xb_op_add(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
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
 * heap values only. */
void     xb_gc_write_item(xb_array *a, size_t index);
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
/* What a FOR EACH loop goes through, in `r`, once before it starts: the
 * collection itself, or for an object its enumerator (GetEnumerator(), else
 * the object when it answers MoveNext and Current, after Reset() when it has
 * one); with `descend`, that enumerator's elements in an array. */
void     xb_each_source(xb_ctx *ctx, xb_item *r, const xb_item *coll, uint32_t descend);
uint32_t xb_each_shared(xb_ctx *ctx, xb_item *var);
void     xb_each_enter(xb_ctx *ctx, xb_item *saved, xb_item *var, uint32_t in_cell, const xb_item *coll);
void     xb_each_move(xb_ctx *ctx, xb_item *var, uint32_t in_cell, int64_t idx);
void     xb_each_leave(xb_item *var, uint32_t in_cell, const xb_item *saved);
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
/* [ABI] The name of a memory variable a module uses (PRIVATE, PUBLIC,
 * PARAMETERS, MEMVAR, M->, an undeclared name): one per name and module,
 * numbered by the runtime when the module registers (`memvars`). */
typedef struct xb_memvar {
  const char *name;         /* upper-cased */
  uint32_t id;              /* set by xb_module_register */
} xb_memvar;
void     xb_module_register(xb_module_init init, xb_item *statics, uint32_t nstatics, const xb_func *procs,
                            uint32_t ninit, uint32_t nexit, const xb_symbol *symbols, uint32_t nsymbols,
                            xb_memvar *memvars, uint32_t nmemvars);

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
  XB_ALIGN8 uint64_t key; /* generation << XB_FIELD_CACHE_INDEX_BITS | index; 0: none */
} xb_field_cache;
#define XB_FIELD_CACHE_INDEX_BITS 16
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
 * (XB_SCOPE_*), and XB_MEMBER_READONLY for a variable assigned only from the
 * methods of its class and of its subclasses. The friends (classes, whose own
 * methods, and functions, which with the codeblocks they make) reach the
 * class's HIDDEN and PROTECTED members and assign its READONLY ones. `init`
 * returns an array with the initial values (INIT) of the variables, then of
 * the class variables, evaluated once, the first time the class is used; each
 * new object gets its own copy of an array or a hash, each subclass the value
 * itself. `destructor` (a method of no arguments) runs once an object of the
 * class, or of a subclass, is unreachable. `runtime` is the runtime's, NULL
 * until then. */
#define XB_SCOPE_EXPORTED   0u
#define XB_SCOPE_PROTECTED  1u  /* the class's methods and its subclasses' */
#define XB_SCOPE_HIDDEN     2u  /* the class's own methods */
#define XB_SCOPE_MASK       3u
#define XB_MEMBER_READONLY  4u
#define XB_MEMBER_SHARED    8u  /* a class variable whose subclasses share it */
#define XB_CLASS_STATIC     1u  /* xb_class_def flags: STATIC CLASS */
#define XB_CLASS_SCALAR     2u  /* SCALAR CLASS: the class of the values of a type, named as ClassName() names it */
/* What ClassName() gives for values that are not objects: the names of the types' classes (SCALAR CLASS). */
#define XB_TYPE_NAME_NIL       "NIL"
#define XB_TYPE_NAME_LOGICAL   "LOGICAL"
#define XB_TYPE_NAME_NUMERIC   "NUMERIC"
#define XB_TYPE_NAME_CHARACTER "CHARACTER"
#define XB_TYPE_NAME_ARRAY     "ARRAY"
#define XB_TYPE_NAME_BLOCK     "BLOCK"
#define XB_TYPE_NAME_DATE      "DATE"
#define XB_TYPE_NAME_HASH      "HASH"
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
  uint32_t flags;           /* XB_CLASS_STATIC */
  void *runtime;
} xb_class_def;
void     xb_classes_register(xb_class_def *defs, uint32_t n);
/* The class function NAME(): a new object of the class, its variables at their
 * initial values (:New() then initializes it). */
void     xb_class_instance(xb_ctx *ctx, xb_item *r, xb_class_def *def);
/* The messages of the operators a class overloads (OPERATOR "+" ...): an
 * object that is the left (or only) operand receives the right operand as its
 * argument; [] receives the index, and the value when it assigns. */
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
/* [ABI] A message sent by compiled code: from a method of class `from` (its
 * scope decides what the method may reach, and a variable is first the one
 * `from` sees), or from elsewhere (NULL); with `super`, the method or variable
 * of `from`'s ancestors (Super:message). `cache` is the send's own, zeroed
 * storage, which the runtime alone reads and writes: the members the message
 * reached for the classes of the last XB_SEND_CACHE_WAYS objects of different
 * classes it went to, the most recent first (a polymorphic inline cache), so
 * that the next message to an object of one of those classes skips the lookup
 * and the checks. Threads share it: `version` is odd while one changes it (a
 * sequence lock, types/class.c). Every field is pointer-sized: the compiler
 * emits it as XB_SEND_CACHE_WORDS words. */
#define XB_SEND_CACHE_WAYS 4
#define XB_SEND_CACHE_WORDS (1 + 3 * XB_SEND_CACHE_WAYS)
typedef struct xb_send_cache {
  size_t version;
  void *cls[XB_SEND_CACHE_WAYS];
  void *member[XB_SEND_CACHE_WAYS];
  size_t slot[XB_SEND_CACHE_WAYS];
} xb_send_cache;
void     xb_op_send_from(xb_ctx *ctx, xb_item *r, const char *message, xb_class_def *from, uint32_t super,
                         xb_send_cache *cache, uint32_t argc, xb_item *argv);

/* [ABI] Generators (types/generator.c): a function with YIELD returns an
 * enumerator (xb_gen_new: `resume`, the function's resume function, and its
 * arguments, which become slots 1 .. argc of the resume function's frame);
 * the resume function, whose argument (frame slot 0) is the enumerator,
 * starts with xb_gen_restore (its frame's other `nslots` - 1 slots back from
 * the enumerator; the state to resume in: 0 at the start), saves them and the
 * element with xb_gen_yield before it returns .T. (`state` goes on after that
 * YIELD), and calls xb_gen_done before it returns .F. at the end. */
void     xb_gen_new(xb_ctx *ctx, xb_item *r, xb_func resume, uint32_t argc, const xb_item *argv);
int32_t  xb_gen_restore(xb_ctx *ctx, const xb_item *gen, xb_item *slots, uint32_t nslots);
void     xb_gen_yield(xb_ctx *ctx, const xb_item *gen, const xb_item *slots, uint32_t nslots, uint32_t state,
                      const xb_item *value);
void     xb_gen_done(xb_ctx *ctx, const xb_item *gen);

/* The macro operator (macro/run.c): `text` holds an expression, compiled when
 * the code runs; it sees memory variables and functions by name, never the
 * caller's LOCALs. xb_macro_get stores its value in `r` (&text);
 * xb_macro_set assigns `value` to what it designates (&text := value);
 * xb_macro_call calls the function it names (&text( args ), references as
 * in Do()); xb_macro_private and xb_macro_public create the variable it
 * names (PRIVATE &text, `mark` the activation's, and PUBLIC &text). A text
 * that is not a string is error 1065, one that is not an expression (or a
 * name, or something assignable) error 1449; both may be substituted. */
void     xb_macro_get(xb_ctx *ctx, xb_item *r, const xb_item *text);
void     xb_macro_set(xb_ctx *ctx, const xb_item *text, const xb_item *value);
void     xb_macro_call(xb_ctx *ctx, xb_item *r, const xb_item *text, uint32_t argc, xb_item *argv);
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
  char     data[2];
} xb_str1;
extern const xb_str1 xb_str_bytes[256];

/* A string of `len` bytes from `p` (the shared empty or one-byte string for
 * up to one byte), and the concatenation of two. */
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
  /* Other threads set bits of the byte (atomically): it is read atomically too. */
#if defined(__GNUC__) || defined(__clang__)
  uint8_t state = __atomic_load_n((const uint8_t *)obj - XB_GC_STATE_BACK, __ATOMIC_RELAXED);
#else
  uint8_t state = *((const volatile uint8_t *)obj - XB_GC_STATE_BACK);
#endif
  if ((state & (XB_GC_STATE_OLD | XB_GC_STATE_RECORDED)) == XB_GC_STATE_OLD) xb_gc_remember(obj);
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
static inline uint32_t xb_hash_bytes_as(const char *p, size_t n, int upper) {
  uint64_t h = UINT64_C(0x9e3779b97f4a7c15) ^ (uint64_t)n;
  while (n > 0) {
    size_t take = n < 8 ? n : 8;
    uint64_t w = 0;
    for (size_t i = 0; i < take; ++i) {
      unsigned char c = (unsigned char)(upper ? xb_ascii_upper(p[i]) : p[i]);
      w |= (uint64_t)c << (8 * i);
    }
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
 * 10 columns, 20 when the value does not fit in 10. Shared by the compiler
 * (numeric literals) and the runtime (computed values). */
static inline uint16_t xb_num_int_width(int64_t i) {
  return (uint16_t)((i < -999999999 || i > INT64_C(9999999999)) ? 20 : 10);
}
static inline uint16_t xb_num_dbl_width(double d) {
  return (uint16_t)((d > 9999999999.0 || d < -999999999.0) ? 20 : 10);
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
 * resolved; NULL without one), the symbol of its operator (NULL without one),
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
      {XB_BIT_AND, "BITAND", "&", 2, 9001},  {XB_BIT_OR, "BITOR", "|", 2, 9002},
      {XB_BIT_XOR, "BITXOR", "^^", 2, 9003}, {XB_BIT_NOR, "BITNOR", NULL, 2, 9004},
      {XB_BIT_NOT, "BITNOT", "~", 1, 9005},  {XB_BIT_SHL, "BITSHL", "<<", 2, 9006},
      {XB_BIT_SHR, "BITSHR", NULL, 2, 9007}, {XB_BIT_SAR, NULL, ">>", 2, 9008},
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
