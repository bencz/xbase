/* What the array and hash functions share: ranges, block calls, order and
 * equality of values, a stable sort, deep copies. */
#ifndef XB_COLLECTIONS_H
#define XB_COLLECTIONS_H

#include "xbrt_int.h"

/* The first position (from 1) of the range `[nStart], [nCount]` taken from
 * argument `k`: 1 when it is absent, not a number or 0; 0, no range, when it
 * is negative or past the end of `len` elements. */
uint64_t xb_range_start(uint32_t argc, const xb_item *argv, uint32_t k, size_t len);
/* The count of `[nCount]` (argument `k`) from a position with `rest` elements
 * left: all of them when it is missing, not a number or negative, none when
 * it is 0, else at most `rest`. */
uint64_t xb_range_count(uint32_t argc, const xb_item *argv, uint32_t k, uint64_t rest);

/* Block calls from C: `n` items for a call's arguments and its result, NIL
 * and rooted, and the calling function's frame on the call stack (ASCAN(0)
 * under the block), from xb_calls_begin to xb_calls_end. */
void xb_calls_begin(xb_ctx *ctx, xb_item *call, uint32_t n, xb_frame *frame, const xb_proc *proc);
void xb_calls_end(xb_ctx *ctx, const xb_frame *frame, uint32_t n);
/* Calls `block` with the first `nargs` items of `call`, its result in
 * call[nargs]; 0 when it left a request to unwind (a BREAK, a QUIT). */
int  xb_calls_run(xb_ctx *ctx, const xb_item *block, xb_item *call, uint32_t nargs);

/* A block's answer to "does the first go first?": a logical, a number other
 * than 0, anything else yes. */
int  xb_order_answer(const xb_item *r);
/* The order without a block: values of one kind compare like the operators
 * (strings follow SET EXACT, integers exactly, other numbers as doubles,
 * dates by day, .F. before .T.), other pairs by kind: array, object or hash,
 * block, string, logical, date, number, NIL. */
int  xb_value_less(const xb_ctx *ctx, const xb_item *x, const xb_item *y);
/* Whether `x` matches `search` as AScan finds it: values of one comparable
 * kind (SET EXACT for strings) that are equal, or two NILs. */
int  xb_value_matches(const xb_ctx *ctx, const xb_item *x, const xb_item *search);

/* Sorts the `n` positions of `pos` (with `tmp` as large) by `less`, stably: a
 * top-down merge sort whose left half has n/2 of them and whose right element
 * goes first only when it is less. */
typedef int (*xb_less_fn)(void *arg, size_t i, size_t j);
void xb_merge_sort(size_t *pos, size_t *tmp, size_t n, xb_less_fn less, void *arg);

/* A copy of the array or hash `root` in `ret` (a root) with the arrays and
 * hashes inside copied too, keeping the structure: one reached twice is
 * copied once, so shared and cyclic ones stay so. Other values, objects
 * included, are shared. An explicit list of copies to visit, no recursion. */
void xb_clone_deep(xb_item *ret, const xb_item *root);

#endif
