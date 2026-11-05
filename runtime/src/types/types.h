/* Representation and base operations of the value types. */
#ifndef XB_TYPES_H
#define XB_TYPES_H

#include "core/context.h"

/* Uninitialized string of `len` bytes (plus the terminating NUL). */
xb_str *xb_str_alloc(size_t len);

/* An array's items start in the same allocation (`inline_items`); when it
 * grows past that, they move to an XB_GC_ITEMS object. No malloc is involved,
 * so dead arrays cost nothing to free. */
typedef struct xb_array {
  size_t   len;
  size_t   cap;
  xb_item *items;
  xb_item  inline_items[];
} xb_array;

/* Longest array: its items must fit in addressable memory with room to grow. */
#define XB_ARRAY_MAX_LEN (SIZE_MAX / sizeof(xb_item) / 4)

xb_array *xb_array_alloc(size_t len);              /* `len` NIL items */
void      xb_array_resize(xb_array *a, size_t len); /* new items are NIL; `a` must be reachable */
void      xb_set_array(xb_item *it, xb_array *a);
void      xb_array_bound_error(xb_ctx *ctx);          /* reading outside 1 .. Len() */

/* What an XB_T_REF item designates: element `index` of `coll`. For a string,
 * `value` holds the one-character element (assigning it leaves the string
 * unchanged). */
typedef struct xb_ref {
  xb_item coll;
  int64_t index;
  xb_item value;
} xb_ref;

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
