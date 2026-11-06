/* Runtime support of FOR EACH and SWITCH. */
#include "xbrt_int.h"

#include <string.h>

/* FOR EACH iterates arrays (their items) and strings (one-character strings);
 * indexes are 1-based. */
int64_t xb_enum_len(xb_ctx *ctx, const xb_item *coll) {
  if (coll->type == XB_T_ARRAY) return (int64_t)((const xb_array *)coll->v.p)->len;
  if (coll->type == XB_T_STRING) return (int64_t)coll->v.s->len;
  if (coll->type == XB_T_HASH) return (int64_t)((const xb_hash *)coll->v.p)->len;
  xb_error(ctx, NULL, EG_ARG, 1068, "array access", 0, 1, coll);
  return -1;
}

/* A variable that holds an element passed to it by reference is shared like
 * a variable in a cell: it moves into one, and the loop works inside it. */
uint32_t xb_each_shared(xb_ctx *ctx, xb_item *var) {
  XB_UNUSED(ctx);
  if (var->type == XB_T_REF && !xb_is_loop_ref(var)) xb_cell_move(var);
  return var->type == XB_T_CELL;
}

/* The item a FOR EACH loop works on (see xb_each_enter in xbrt.h). */
static xb_item *each_base(xb_item *var, uint32_t in_cell) {
  return in_cell ? &((xb_cell *)var->v.p)->value : var;
}

/* A store into the base is a store into the cell when the variable is shared. */
static void each_written(xb_item *var, uint32_t in_cell) {
  if (in_cell) xb_gc_write(var->v.p);
}

void xb_each_enter(xb_ctx *ctx, xb_item *saved, xb_item *var, uint32_t in_cell, const xb_item *coll) {
  XB_UNUSED(ctx);
  xb_item *base = each_base(var, in_cell);
  *saved = *base;
  xb_ref_init(base, coll, 0, 1); /* `coll` and `saved` are frame slots, `base` is reachable: all rooted */
  each_written(var, in_cell);
}

void xb_each_move(xb_ctx *ctx, xb_item *var, uint32_t in_cell, int64_t idx) {
  xb_item *base = each_base(var, in_cell); /* still holds the loop's reference: taking the variable copies it */
  xb_ref *ref = base->v.p;
  ref->index = idx;
  if (ref->coll.type == XB_T_STRING) {
    const xb_str *s = ref->coll.v.s;
    if (idx >= 1 && (uint64_t)idx <= s->len) {
      xb_set_str(&ref->value, xb_str_new(s->data + idx - 1, 1));
      xb_gc_write(ref);
    }
  } else {
    xb_ref_target(ctx, base); /* an array that shrank under the loop: bound error */
  }
}

void xb_each_leave(xb_item *var, uint32_t in_cell, const xb_item *saved) {
  *each_base(var, in_cell) = *saved;
  each_written(var, in_cell);
}

void xb_each_key(xb_ctx *ctx, xb_item *r, const xb_item *coll, int64_t idx) {
  if (coll->type != XB_T_HASH) {
    xb_set_int(r, idx);
    return;
  }
  const xb_hentry *e = xb_hash_at(coll->v.p, idx);
  if (e) {
    *r = e->key;
    return;
  }
  xb_item i;
  xb_set_int(&i, idx);
  xb_array_bound_error(ctx, coll, &i); /* the hash shrank under the loop */
}

/* SWITCH compares integers and strings only. */
void xb_switch_check(xb_ctx *ctx, xb_item *r, const xb_item *value) {
  if (value->type == XB_T_INT || value->type == XB_T_STRING) *r = *value;
  else xb_arg_error(ctx, r, 3104, "SWITCH", 1, value);
}

int32_t xb_switch_match(const xb_item *value, const xb_item *c) {
  if (value->type != c->type) return 0;
  if (value->type == XB_T_INT) return value->v.i == c->v.i;
  const xb_str *a = value->v.s, *b = c->v.s;
  return a->len == b->len && memcmp(a->data, b->data, a->len) == 0;
}
