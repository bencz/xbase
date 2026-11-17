/* Rings and claims (rtl/flows.h): the queues of a runtime object (a Channel's values and waiters, a Mutex's or a
 * Semaphore's waiting tasks), in its variables, changed under its short lock, and the waiting tasks claimed under that
 * lock and completed once it is released. */
#include "rtl/flows.h"

#define RING_FIRST 8u

/* A ring's integer variable `k` (under the object's lock: no other thread reads or writes it meanwhile). */
static uint32_t var(const xb_item *o, uint32_t k) { return xb_object_u32(o, k); }
static void set_var(const xb_item *o, uint32_t k, uint32_t n) { xb_set_int(&((xb_object *)o->v.p)->vars[k], n); }

void xb_ring_init(xb_object *o, xb_ring g) {
  xb_set_nil(&o->vars[g.items]);
  xb_set_int(&o->vars[g.head], 0);
  xb_set_int(&o->vars[g.count], 0);
}

/* The ring's array (under the lock), NULL before its first element. */
static xb_array *ring_array(const xb_item *o, xb_ring g) {
  const xb_item *a = &((xb_object *)o->v.p)->vars[g.items];
  return a->type == XB_T_ARRAY ? a->v.p : NULL;
}

/* Item k of a ring's array, read and written in place under the lock (the write followed by its barrier). */
static void slot_put(xb_array *a, size_t k, const xb_item *v) {
  a->items[k] = *v;
  xb_gc_write_item(a, k);
}

/* The ring's elements, from its head, moved into a new array of `cap` (rooted in the object), from 0. */
static void ring_move(const xb_item *o, xb_ring g, uint32_t cap) {
  xb_array *from = ring_array(o, g);
  uint32_t head = var(o, g.head), n = var(o, g.count);
  size_t was = from ? from->len : 0;
  xb_item grown;
  xb_array *a = xb_array_alloc(cap); /* `a`, on the stack, stays where it is; so does `from` */
  xb_set_array(&grown, a);
  for (uint32_t k = 0; k < n; ++k) a->items[k] = from->items[(head + k) % was];
  xb_gc_write_items(a, 0, n);
  xb_share(&grown);
  xb_object *obj = o->v.p;
  obj->vars[g.items] = grown;
  xb_gc_write(obj);
  set_var(o, g.head, 0);
}

/* The elements that need their place no more (`gone`) dropped from a full ring, the others kept in order from its
 * head. */
static void ring_compact(const xb_item *o, xb_ring g, xb_ring_gone gone, void *arg) {
  xb_array *a = ring_array(o, g);
  uint32_t head = var(o, g.head), n = var(o, g.count), kept = 0;
  size_t cap = a->len;
  xb_item nil;
  xb_set_nil(&nil);
  for (uint32_t k = 0; k < n; ++k) {
    const xb_item *it = &a->items[(head + k) % cap];
    if (gone(arg, it)) continue;
    if (kept != k) slot_put(a, (head + kept) % cap, it);
    ++kept;
  }
  for (uint32_t k = kept; k < n; ++k) a->items[(head + k) % cap] = nil;
  set_var(o, g.count, kept);
}

void xb_ring_set(const xb_item *o, xb_ring g, uint32_t k, const xb_item *v) {
  xb_array *a = ring_array(o, g);
  slot_put(a, (var(o, g.head) + k) % a->len, v);
}

void xb_ring_push(const xb_item *o, xb_ring g, const xb_item *v, xb_ring_gone gone, void *arg) {
  xb_array *a = ring_array(o, g);
  size_t cap = a ? a->len : 0;
  int full = var(o, g.count) == cap;
  if (cap && full && gone) ring_compact(o, g, gone, arg);
  if (full && var(o, g.count) * 2 >= cap) {
    ring_move(o, g, cap ? (uint32_t)(2 * cap) : RING_FIRST);
    a = ring_array(o, g);
    cap = a->len;
  }
  uint32_t head = var(o, g.head), n = var(o, g.count);
  slot_put(a, (head + n) % cap, v);
  set_var(o, g.count, n + 1);
}

void xb_ring_pop(const xb_item *o, xb_ring g, xb_item *out) {
  xb_array *a = ring_array(o, g);
  uint32_t head = var(o, g.head), n = var(o, g.count);
  size_t cap = a->len;
  *out = a->items[head];
  xb_set_nil(&a->items[head]);
  set_var(o, g.head, (uint32_t)((head + 1) % cap));
  set_var(o, g.count, n - 1);
  if (cap > 4 * RING_FIRST && (n - 1) * 4 <= cap) ring_move(o, g, (uint32_t)(cap / 2));
}

int xb_ring_task_gone(void *arg, const xb_item *task) {
  XB_UNUSED(arg);
  return xb_task_is_final(task);
}

int xb_ring_pair_gone(void *arg, const xb_item *pair) {
  XB_UNUSED(arg);
  return xb_task_is_final(&((const xb_array *)pair->v.p)->items[0]);
}

void xb_claims_root(xb_ctx *ctx, xb_claims *c) {
  xb_set_nil(&c->list);
  xb_set_nil(&c->held);
  c->n = 0;
  c->fail = 0;
  xb_gc_root_push(ctx, &c->list);
  xb_gc_root_push(ctx, &c->held);
}

void xb_claims_add(xb_claims *c, const xb_item *task, const xb_item *value) {
  xb_array *a = c->list.type == XB_T_ARRAY ? c->list.v.p : NULL;
  if (!a || 2 * c->n + 2 > a->len) {
    xb_array *grown = xb_array_alloc(a ? 2 * a->len : 8); /* on the stack, stays where it is; `a` too */
    for (uint32_t k = 0; k < 2 * c->n; ++k) grown->items[k] = a->items[k];
    xb_gc_write_items(grown, 0, 2 * c->n);
    xb_set_array(&c->list, grown);
    a = grown;
  }
  a->items[2 * c->n] = *task;
  a->items[2 * c->n + 1] = *value;
  xb_gc_write_items(a, 2 * c->n, 2 * c->n + 2);
  ++c->n;
}

void xb_claims_settle(xb_ctx *ctx, xb_claims *c) {
  for (uint32_t k = 0; k < c->n; ++k) {
    xb_array *a = c->list.v.p; /* again after each: settling may collect */
    xb_item task = a->items[2 * k], value = a->items[2 * k + 1];
    xb_task_settle_claimed(ctx, &task, !c->fail, &value);
  }
  xb_set_nil(&c->list);
  c->n = 0;
}
