/* Runtime support of FOR EACH, SWITCH and `...`. */
#include "xbrt_int.h"

#include <string.h>

/* Whether the object, of a compiled class, answers `message`: with a method, or with a variable too (`orVar`). */
static int answers(const xb_item *it, const char *message, int orVar) {
  if (it->type != XB_T_OBJECT || !((const xb_object *)it->v.p)->cls->compiled) return 0;
  return xb_class_has(it->v.p, message, 1) || (orVar && xb_class_has(it->v.p, message, 0));
}

/* `message` sent to `obj` with no arguments, its result in `r`; 0 after an error. */
static int send0(xb_ctx *ctx, xb_item *r, const xb_item *obj, const char *message) {
  xb_item self = *obj;
  xb_gc_root_push(ctx, &self);
  xb_op_send(ctx, r, message, 1, &self);
  xb_gc_root_pop(ctx, 1);
  return !(ctx->pending && xb_pending(ctx));
}

/* An enumerator's next element: MoveNext() is .T.; 0 at the end or after an error. */
static int move_next(xb_ctx *ctx, const xb_item *e) {
  if (xb_is_generator(e)) return xb_gen_move_next(ctx, e);
  xb_item moved;
  xb_set_nil(&moved);
  xb_gc_root_push(ctx, &moved);
  int ok = send0(ctx, &moved, e, "MOVENEXT") && moved.type == XB_T_LOGICAL && moved.v.l;
  xb_gc_root_pop(ctx, 1);
  return ok;
}

/* The current element of an enumerator; 0 after an error. */
static int current(xb_ctx *ctx, xb_item *r, const xb_item *e) {
  if (xb_is_generator(e)) {
    *r = *xb_gen_current(e);
    return 1;
  }
  return send0(ctx, r, e, "CURRENT");
}

/* An enumerator's elements, in an array in `r` (which may be `e`'s own item: `e` is copied first). */
static void gather(xb_ctx *ctx, xb_item *r, const xb_item *e) {
  xb_item from = *e, element;
  xb_set_nil(&element);
  xb_gc_root_push(ctx, &from);
  xb_gc_root_push(ctx, &element);
  xb_set_array(r, xb_array_alloc(0));
  while (move_next(ctx, &from) && current(ctx, &element, &from)) {
    xb_array *a = r->v.p;
    xb_array_resize(a, a->len + 1);
    a->items[a->len - 1] = element;
    xb_gc_write_item(a, a->len - 1);
  }
  xb_gc_root_pop(ctx, 2);
}

/* An object goes through its enumerator: GetEnumerator(), or the object itself when it is one (MoveNext; Reset()
 * first, when it has one); for DESCEND, the enumerator's elements in an array, gone through backwards. */
void xb_each_source(xb_ctx *ctx, xb_item *r, const xb_item *coll, uint32_t descend) {
  *r = *coll;
  if (coll->type != XB_T_OBJECT) return;
  if (xb_is_generator(coll)) {
    if (descend) gather(ctx, r, coll);
    return;
  }
  if (answers(coll, "GETENUMERATOR", 0)) {
    if (!send0(ctx, r, coll, "GETENUMERATOR")) return;
  } else if (answers(coll, "MOVENEXT", 0) && answers(coll, "CURRENT", 1) && answers(coll, "RESET", 0)) {
    xb_item ignored;
    xb_set_nil(&ignored);
    xb_gc_root_push(ctx, &ignored);
    int ok = send0(ctx, &ignored, coll, "RESET");
    xb_gc_root_pop(ctx, 1);
    if (!ok) return;
  }
  if (!xb_is_generator(r) && (!answers(r, "MOVENEXT", 0) || !answers(r, "CURRENT", 1))) {
    xb_error(ctx, NULL, EG_ARG, 1068, "array access", 0, 1, coll); /* no collection, as for any other value */
    return;
  }
  if (descend) gather(ctx, r, r);
}

/* An enumerator's length so far, or the error of a value that is no collection. */
XB_NOINLINE static int64_t enum_len_other(xb_ctx *ctx, const xb_item *coll) {
  if (xb_is_generator(coll) || answers(coll, "MOVENEXT", 0)) return move_next(ctx, coll) ? INT64_MAX : 0;
  xb_error(ctx, NULL, EG_ARG, 1068, "array access", 0, 1, coll);
  return -1;
}

/* FOR EACH iterates arrays (their items), strings (one-character strings),
 * hashes (their values) and enumerators (each MoveNext() that is .T. one more
 * element: a length as large as can be, else 0); indexes are 1-based. */
int64_t xb_enum_len(xb_ctx *ctx, const xb_item *coll) {
  if (coll->type == XB_T_ARRAY) return (int64_t)((const xb_array *)coll->v.p)->len;
  if (coll->type == XB_T_STRING) return (int64_t)coll->v.s->len;
  if (coll->type == XB_T_HASH) return (int64_t)((const xb_hash *)coll->v.p)->len;
  return enum_len_other(ctx, coll);
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

/* An enumerator's Current, into the loop reference's own item. Kept apart, so
 * that the paths of arrays and strings stay without a frame of their own. */
XB_NOINLINE static void move_current(xb_ctx *ctx, xb_ref *ref) {
  xb_item value;
  xb_set_nil(&value);
  xb_gc_root_push(ctx, &value);
  if (current(ctx, &value, &ref->coll)) {
    ref->value = value; /* the reference is the loop's: alive, and objects do not move */
    xb_gc_write(ref);
  }
  xb_gc_root_pop(ctx, 1);
}

void xb_each_move(xb_ctx *ctx, xb_item *var, uint32_t in_cell, int64_t idx) {
  xb_item *base = each_base(var, in_cell); /* still holds the loop's reference: taking the variable copies it */
  xb_ref *ref = base->v.p;
  ref->index = idx;
  if (ref->coll.type == XB_T_ARRAY || ref->coll.type == XB_T_HASH) {
    xb_ref_target(ctx, base); /* an array that shrank under the loop: bound error */
  } else if (ref->coll.type == XB_T_STRING) {
    const xb_str *s = ref->coll.v.s;
    if (idx >= 1 && (uint64_t)idx <= s->len) {
      xb_set_str(&ref->value, xb_str_new(s->data + idx - 1, 1));
      xb_gc_write(ref);
    }
  } else {
    move_current(ctx, ref);
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

int32_t xb_args_rest(xb_ctx *ctx, xb_item *dst, xb_item *rest, uint32_t n, uint32_t at, uint32_t refs) {
  for (uint32_t k = 0; k < n; ++k) {
    if (xb_refs_take(refs, at + k) || (rest[k].type != XB_T_CELL && rest[k].type != XB_T_REF)) {
      dst[k] = rest[k];
      continue;
    }
    const xb_item *v = xb_ref_target(ctx, &rest[k]);
    if (ctx->pending && xb_pending(ctx)) return 0;
    dst[k] = *v;
  }
  return 1;
}
