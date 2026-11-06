/* Representation and base operations of the value types. */
#ifndef XB_TYPES_H
#define XB_TYPES_H

#include "core/context.h"

/* Uninitialized string of `len` bytes (plus the terminating NUL). */
xb_str *xb_str_alloc(uint64_t len); /* 64-bit: checked before it is narrowed to size_t (32-bit hosts) */
/* -1, 0 or 1 as the comparison operators order two values (strings follow SET
 * EXACT), or 2 when they cannot be compared (core/ops.c). */
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
void      xb_array_bound_error(xb_ctx *ctx);          /* reading outside 1 .. Len() */

/* What an XB_T_REF item designates: element `index` of `coll`. For a string,
 * `value` holds the one-character element (assigning it leaves the string
 * unchanged). `loop`: the reference of a FOR EACH loop, else an element passed
 * by reference (@a[i]). */
typedef struct xb_ref {
  xb_item coll;
  int64_t index;
  xb_item value;
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

/* The empty string, never collected. */
xb_str *xb_str_empty(void);

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
