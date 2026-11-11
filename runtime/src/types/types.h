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

/* What an XB_T_REF item designates: element `index` of `coll`. For a string,
 * `value` holds the one-character element (assigning it leaves the string
 * unchanged). For a hash, the entry of `key` when it is not NIL (@h[k], the
 * current element of a loop taken by a block or a reference), else the entry
 * at position `index` (a FOR EACH loop). `loop`: the reference of a FOR EACH
 * loop, else an element passed by reference (@a[i]). */
typedef struct xb_ref {
  xb_item coll;
  int64_t index;
  xb_item value;
  xb_item key;
  uint32_t loop;
} xb_ref;

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
 * 32 bits signed or 16 bits unsigned (0 for a double beyond 64 bits). */
typedef enum xb_var_rule {
  XB_VAR_ANY,
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

/* A class defined by the runtime. */
typedef struct xb_class {
  const char *name;             /* upper-cased, as ClassName() returns it */
  uint32_t nvars;
  const xb_class_var *vars;     /* in the order of an object's items */
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
int     xb_date_epoch_year(int year, int epoch);
int64_t xb_date_parse(const char *text, size_t len, const char *fmt, int epoch);
int64_t xb_date_today(void);

/* Day arithmetic wraps instead of overflowing; out-of-range results are
 * invalid dates, as in Clipper. */
static inline int64_t xb_add_days(int64_t jd, int64_t days) { return (int64_t)((uint64_t)jd + (uint64_t)days); }

#endif
