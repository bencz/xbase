/* Representation and base operations of the value types. */
#ifndef XB_TYPES_H
#define XB_TYPES_H

#include "core/context.h"

/* Uninitialized string of `len` bytes (plus the terminating NUL). */
xb_str *xb_str_alloc(uint64_t len); /* 64-bit: checked before it is narrowed to size_t (32-bit hosts) */
/* -1, 0 or 1 as the comparison operators order two values (strings follow SET
 * EXACT), XB_ORDER_UNORDERED for a NaN (xb_order_is in xbrt.h), or 2 when they
 * cannot be compared (core/ops.c). */
int xb_compare(const xb_ctx *ctx, const xb_item *a, const xb_item *b);

/* xb_array is in xbrt.h. Its items move to an XB_GC_ITEMS object when it
 * grows past its own allocation; no malloc is involved, so dead arrays cost
 * nothing to free. */

/* Card marking. Storage for XB_ARRAY_CARD_MIN items or more carries one card
 * byte per 2^XB_ARRAY_CARD_SHIFT items, right after the last item it has room
 * for. A store into an old array marks the element's card, and a young
 * collection rescans only the marked cards instead of the whole array. */
#define XB_ARRAY_CARD_SHIFT 6
#define XB_ARRAY_CARD_MIN   1024u
static inline size_t xb_array_card_count(size_t cap) {
  return cap >= XB_ARRAY_CARD_MIN ? (cap + ((size_t)1 << XB_ARRAY_CARD_SHIFT) - 1) >> XB_ARRAY_CARD_SHIFT : 0;
}
static inline uint8_t *xb_array_cards(const xb_array *a) {
  return a->cap >= XB_ARRAY_CARD_MIN ? (uint8_t *)(a->items + a->cap) : NULL;
}

/* Longest array: its items must fit in addressable memory with room to grow. */
#define XB_ARRAY_MAX_LEN (SIZE_MAX / sizeof(xb_item) / 4)
/* Longest string: its size with the header must not wrap. */
#define XB_STR_MAX_LEN (SIZE_MAX / 2)

/* The 0-based index of the 1-based position `n` in `a`, or -1 outside it. */
static inline int64_t xb_array_index(const xb_array *a, int64_t n) {
  return n >= 1 && (uint64_t)n <= a->len ? n - 1 : -1;
}

/* Lengths are 64-bit so that a length past XB_ARRAY_MAX_LEN is caught before
 * it is narrowed to size_t (on 32-bit hosts). */
xb_array *xb_array_alloc(uint64_t len);              /* `len` NIL items */
void      xb_array_resize(xb_array *a, uint64_t len); /* new items are NIL; `a` must be reachable */
void      xb_set_array(xb_item *it, xb_array *a);
void      xb_array_bound_error(xb_ctx *ctx, const xb_item *base, const xb_item *idx); /* reading outside 1 .. Len() */

/* Stores in `r` a new reference to element `index` of `coll` (which must stay
 * reachable meanwhile: a frame slot). xb_ref_copy stores in `r` a new
 * reference to the element `ref` (a reachable XB_T_REF item) designates. */
void xb_ref_init(xb_item *r, const xb_item *coll, int64_t index, uint32_t loop);
void xb_ref_copy(xb_item *r, const xb_item *ref);

/* Whether `it` holds the reference of a FOR EACH loop. */
static inline int xb_is_loop_ref(const xb_item *it) {
  return it->type == XB_T_REF && ((const xb_ref *)it->v.p)->loop;
}

/* Moves the value of the variable whose own item is `var` into a new cell
 * (unless it holds one) and returns the cell. */
xb_cell *xb_cell_move(xb_item *var);

/* What an instance variable of a class accepts. A value of another type is
 * ignored by XB_VAR_ARRAY, _LOGICAL and _STRING; the numeric rules raise an
 * argument error for it, and keep a number truncated toward zero, wrapped to
 * 32 bits signed or 16 bits unsigned (0 for a double beyond 64 bits).
 * XB_VAR_READ takes no assignment (the message to assign it is not found);
 * the runtime alone sets it. */
typedef enum xb_var_rule {
  XB_VAR_ANY,
  XB_VAR_READ,
  XB_VAR_ARRAY,
  XB_VAR_LOGICAL,
  XB_VAR_STRING,
  XB_VAR_INT32,
  XB_VAR_UINT16,
} xb_var_rule;

typedef struct xb_class_var {
  const char *name;             /* upper-cased */
  xb_var_rule rule;
} xb_class_var;

/* A class: one the runtime defines in C, or one compiled from xBase
 * (`compiled`, types/class.c), whose variables all take any value. */
typedef struct xb_class {
  const char *name;             /* upper-cased, as ClassName() returns it */
  uint32_t nvars;
  const xb_class_var *vars;     /* in the order of an object's items */
  struct cls *compiled;         /* NULL for the runtime's own classes */
} xb_class;

/* An instance of a class: one item per variable, each starting as its rule's
 * empty value (NIL, .F., "" or 0). */
typedef struct xb_object {
  const xb_class *cls;
  uint32_t nvars;
  xb_item vars[];
} xb_object;

xb_object *xb_object_new(const xb_class *cls);
void       xb_set_object(xb_item *it, xb_object *o);
/* A message to an object of a compiled class (types/class.c); one it does not
 * answer is the class's error ("Message not found"). Returns 1. */
int        xb_class_send(xb_ctx *ctx, xb_item *r, const char *message, xb_class_def *from, uint32_t super,
                         uint32_t argc, xb_item *argv);
/* Runs the destructors of the objects the collector found unreachable
 * (xb_gc_next_finalized), each class of the object's line that has one, the
 * object's class first; they may collect. The requests pending before (and
 * the BREAK value) wait meanwhile; an error, BREAK or TRAP in a destructor
 * ends it alone, and a QUIT stays and leaves the other objects queued. */
void       xb_classes_finalize(xb_ctx *ctx);
/* A message to a value of a type a compiled class is associated with
 * (ClassAssociate): 0 when there is none. xb_class_associate associates the
 * class of `o` with the type named `type` (as ClassName() names it): 1, 0 for
 * an unknown type, -1 when the class has instance variables. */
int        xb_class_scalar_send(xb_ctx *ctx, xb_item *r, const char *message, xb_class_def *from, uint32_t super,
                                uint32_t argc, xb_item *argv);
int        xb_class_associate(const xb_object *o, const char *type);
/* A message to argv[0] as if it were an object of the class whose objects the class function `classFunction` makes
 * (its methods, Self being argv[0]); `*def` keeps that class from the first message on (NULL before it). 0 when the
 * function makes no object of a compiled class, or the class does not answer the message. */
int        xb_class_send_as(xb_ctx *ctx, xb_item *r, xb_func classFunction, xb_class_def **def, const char *message,
                            uint32_t argc, xb_item *argv);
/* The name ClassName() gives values of a type that is not an object's
 * ("NUMERIC", "CHARACTER" ...), NULL for internal types. */
const char *xb_type_class_name(xb_type type);
/* Generators (types/generator.c): whether `it` is a generator's enumerator;
 * one step (MoveNext: .T. while it yields an element, its CURRENT variable);
 * the messages it answers besides its CURRENT variable (1 when it did). */
extern const xb_class xb_generator_class;
static inline int xb_is_generator(const xb_item *it) {
  return it->type == XB_T_OBJECT && ((const xb_object *)it->v.p)->cls == &xb_generator_class;
}
int        xb_gen_move_next(xb_ctx *ctx, const xb_item *gen);
const xb_item *xb_gen_current(const xb_item *gen);
int        xb_gen_message(xb_ctx *ctx, xb_item *r, const char *message, uint32_t argc, xb_item *argv);
/* What every value answers (ClassName), else the error of a message no
 * class has: BASE/1004, or BASE/1005 for an assignment. */
void       xb_object_message(xb_ctx *ctx, xb_item *r, const char *message, uint32_t argc, xb_item *argv);
/* An operator the class of `a` overloads (`op`, an XB_OPERATOR_* message),
 * sent to `a` with `b` and `c` (NULL when absent): 0, and nothing done, when
 * `a` is not an object of a compiled class with that operator. */
int      xb_class_operator(xb_ctx *ctx, xb_item *r, const char *op, const xb_item *a, const xb_item *b,
                           const xb_item *c);
/* Reflection on an object of a compiled class: whether its class, or one it
 * inherits from, is `name` (upper-cased); whether it answers `message` with a
 * method (`method`) or a variable, in any scope; the messages of its EXPORTED
 * variables (or methods; not the assigning "_NAME" ones, nor operators), in
 * the order of its table, stored in `out` when it is not NULL, their count
 * returned. */
int      xb_class_kind_of(const xb_object *o, const char *name);
int      xb_class_has(const xb_object *o, const char *message, int method);
uint32_t xb_class_names(const xb_object *o, int methods, const char **out);
/* "CLASS:MESSAGE" in `buf`, cut to fit: how an error names a message to an object. */
#define XB_MESSAGE_TEXT_SIZE 128
const char *xb_message_text(char buf[XB_MESSAGE_TEXT_SIZE], const char *cls, const char *message);

/* Hashes: keys (strings, numbers, dates) to values, in insertion order. The
 * entries sit in one array, in the order they came, with a hash of each key;
 * an index of open addressing (linear probing, at most half full, entry + 1
 * per slot, 0 when free) finds them. A deleted entry leaves a hole (its key
 * NIL) until the array is compacted, when it fills up with enough holes.
 * Positions count live entries: without holes a position is an entry; with
 * holes a Fenwick tree of the live entries (`tree`, built when a position is
 * first needed, kept up to date until the next compaction) finds one in
 * O(log cap). Entries, hashes, index and tree are one XB_GC_HSTORE object,
 * which the hash's tracing marks; `cap` is a power of two. A number key that
 * equals an integer is that integer (1 and 1.0 are one key); strings compare
 * by bytes, or letter case aside. */
#define XB_HASH_IGNORECASE 0x1u
#define XB_HASH_MAX_LEN (UINT32_C(1) << 30) /* entries; the index has twice as many slots */
/* xb_hentry and xb_hash: xbrt.h. */

xb_hash *xb_hash_alloc(void); /* empty, without a store */
void     xb_set_hash(xb_item *it, xb_hash *h);
/* Whether `key` can be a key (a string, a number or a date). */
int      xb_hash_key_ok(const xb_item *key);
/* The entry of `key` (a valid key), or -1. */
int64_t  xb_hash_find(const xb_hash *h, const xb_item *key);
/* Sets `key` (a valid key) to `value`, adding it last when new; `h` must be
 * reachable, `key` and `value` too (rooted). */
void     xb_hash_set(xb_hash *h, const xb_item *key, const xb_item *value);
/* Removes the entry of `key`; 0 when there is none. */
int      xb_hash_del(xb_hash *h, const xb_item *key);
/* The entry at 1-based position `pos` (1 .. len), through the tree when
 * there are holes; NULL outside. */
xb_hentry *xb_hash_at(xb_hash *h, int64_t pos);
/* The 1-based position of `key` (a valid key), or 0. */
int64_t  xb_hash_pos(xb_hash *h, const xb_item *key);
void     xb_hash_del_at(xb_hash *h, int64_t pos);
void     xb_hash_clear(xb_hash *h);
/* Moves the live entries over the holes, keeping their order (positions stay
 * the same), so that position k is entries[k - 1]. */
void     xb_hash_compact(xb_hash *h);
/* Room for `n` entries without growing. */
void     xb_hash_reserve(xb_hash *h, uint32_t n);
/* Fills the empty `dst` (reachable) with the entries of `src`, in order,
 * and its flags; the values themselves are not copied. */
void     xb_hash_copy(xb_hash *dst, const xb_hash *src);
/* Switches letter case aside (XB_HASH_IGNORECASE) on or off; the index is
 * rebuilt. Keys that become equal merge into the first of them, which keeps
 * its spelling and position and takes the value of the last. */
void     xb_hash_set_flags(xb_hash *h, uint32_t flags);
/* Rehashes the keys and rebuilds the index after the caller reordered the
 * entries of a hash without holes (xb_hash_compact first). */
void     xb_hash_reindex(xb_hash *h);

/* The empty string, never collected. */
xb_str *xb_str_empty(void);

/* The letter ValType() gives a value's type. */
char xb_type_letter(const xb_item *it);
/* A string of `len` bytes (unset) that may grow in place up to `room`
 * (XB_STR_APPEND, the room in `hash`; without the flag when the room cannot
 * be recorded); xb_str_share ends the growth. */
xb_str *xb_str_growing(uint64_t len, uint64_t room);
void    xb_str_share(xb_str *s);

/* Dates are Julian day numbers (0: the empty date), years 0 to 9999. Text
 * buffers include the terminating NUL: "YYYYMMDD", and a date by a SET DATE
 * FORMAT picture, which is cut at 10 characters. */
#define XB_DATE_DIGITS_SIZE 9
#define XB_DATE_TEXT_SIZE   11
int64_t xb_date_encode(int year, int month, int day);   /* 0 if invalid */
void    xb_date_decode(int64_t jd, int *year, int *month, int *day);
void    xb_set_date(xb_item *it, int64_t jd);
void    xb_date_to_digits(int64_t jd, char out[XB_DATE_DIGITS_SIZE]);
size_t  xb_date_format(int64_t jd, const char *fmt, char out[XB_DATE_TEXT_SIZE]);
/* The same for the eight characters of `digits` as YYYYMMDD (Transform()
 * formats "99999999" this way for a picture's template). */
size_t  xb_date_format_digits(const char *digits, const char *fmt, char out[XB_DATE_TEXT_SIZE]);
int     xb_date_epoch_year(int year, int epoch);
int64_t xb_date_parse(const char *text, size_t len, const char *fmt, int epoch);
int64_t xb_date_today(void);

/* Day arithmetic wraps instead of overflowing; out-of-range results are
 * invalid dates, as in Clipper. */
static inline int64_t xb_add_days(int64_t jd, int64_t days) { return (int64_t)((uint64_t)jd + (uint64_t)days); }

#endif
