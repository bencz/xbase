/* References, cells, objects and generators when what they reach is shared (types/ref.c, types/block.c, types/class.c,
 * types/generator.c), threads at once, collections (full ones, which move objects, too) running meanwhile. Writers set
 * an element of a shared array through a reference to it (XB_T_REF), the value of a shared cell (XB_T_CELL), a hash's
 * value through a cell that holds a reference to its key's entry, and a compiled class's variable by messages,
 * alternating integers and doubles, while readers read them back the same ways: no reader sees an integer of a double's
 * bits or a tiny double (an item read torn). Then threads step one shared enumerator: as many steps succeed as it has
 * numbers (no step runs twice, none is lost: a MoveNext while another thread runs a step answers .F. and touches
 * nothing), each Current read is one of its numbers or, once it has ended, NIL (another thread may have stepped between
 * a MoveNext and the Current that follows it: the pair is not one operation), and then it has ended. */
#include "check.h"
#include "core/shared.h"
#include "xbrt_int.h"

#include <stdio.h>
#include <stdlib.h>

void xbp_NUMBERS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbp_NEWBOX(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

#define WRITERS 2
#define READERS 2
#define ROUNDS 30000u
#define STEPPERS 3
#define LAST 20000

#define SHARED 5
static xb_item shared[SHARED]; /* an array, a reference to its element, a cell, a cell holding a hash entry's
                                * reference, an object */
static uint32_t bad, busy, steps, values;

typedef struct worker {
  xb_ctx *ctx;
  xb_item v, args[2];
} worker;

static void begin(worker *w, void *base) {
  xb_gc_thread_attach(base);
  w->ctx = xb_ctx_new();
  xb_set_nil(&w->v);
  xb_set_nil(&w->args[0]);
  xb_set_nil(&w->args[1]);
  xb_gc_root_push(w->ctx, &w->v);
  xb_gc_root_push(w->ctx, &w->args[0]);
  xb_gc_root_push(w->ctx, &w->args[1]);
}

static void end(worker *w) {
  xb_gc_root_pop(w->ctx, 3);
  xb_ctx_free(w->ctx);
  xb_gc_thread_detach();
}

static void poll(worker *w, uint32_t n) {
  if (xb_pending_get(w->ctx)) xb_pending_gc(w->ctx);
  if (n % 8192u == 4096u) xb_gc_collect(); /* full: objects may move */
  else if (n % 2048u == 0) xb_gc_collect_young();
}

static int valid(const xb_item *v) {
  if (v->type == XB_T_INT) return (uint64_t)v->v.i < ((uint64_t)1 << 32);
  return v->type == XB_T_DOUBLE && v->v.d >= 0.5;
}

static void writer(void *arg) {
  worker w;
  begin(&w, &arg);
  uint32_t me = (uint32_t)(size_t)arg;
  for (uint32_t j = 0; j < ROUNDS; ++j) {
    if ((j / 8u + me) & 1u) xb_set_int(&w.v, (int64_t)j);
    else xb_set_double(&w.v, 0.5 + (double)j, 1);
    uint32_t at = 1 + j % 4u;
    if (at < 4) {
      xb_ref_set(w.ctx, &shared[at], &w.v);
    } else {
      w.args[0] = shared[4];
      w.args[1] = w.v;
      xb_op_send(w.ctx, &w.v, "_V", 2, w.args);
    }
    poll(&w, j);
  }
  xb_atomic_add32(&busy, (uint32_t)-1, XB_RELEASE);
  end(&w);
}

static void reader(void *arg) {
  worker w;
  begin(&w, &arg);
  for (uint32_t n = 0; xb_atomic_load32(&busy, XB_ACQUIRE); ++n) {
    uint32_t at = 1 + n % 4u;
    if (at < 4) {
      xb_ref_get(w.ctx, &shared[at], &w.v);
    } else {
      w.args[0] = shared[4];
      xb_op_send(w.ctx, &w.v, "V", 1, w.args);
    }
    if (!valid(&w.v)) xb_atomic_add32(&bad, 1, XB_RELAXED);
    poll(&w, n);
  }
  end(&w);
}

static void stepper(void *arg) {
  worker w;
  begin(&w, &arg);
  for (uint32_t n = 0; xb_atomic_load32(&busy, XB_ACQUIRE) && n < 100000000u; ++n) { /* a step lost: no hang */
    if (xb_gen_move_next(w.ctx, &shared[0])) {
      xb_gen_current(&shared[0], &w.v);
      if (w.v.type == XB_T_INT && w.v.v.i >= 1 && w.v.v.i <= LAST) xb_atomic_add32(&values, 1, XB_RELAXED);
      else if (w.v.type != XB_T_NIL) xb_atomic_add32(&bad, 1, XB_RELAXED); /* NIL: it ended meanwhile */
      if (xb_atomic_add32(&steps, 1, XB_RELAXED) + 1 == LAST) xb_atomic_store32(&busy, 0, XB_RELEASE);
    }
    poll(&w, n);
  }
  end(&w);
}

int main(void) {
  xb_platform_init();
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_ctx *ctx = xb_ctx_new();
  for (int k = 0; k < SHARED; ++k) xb_set_nil(&shared[k]);
  xb_gc_add_root(shared, SHARED);
  xb_set_array(&shared[0], xb_array_alloc(4));
  xb_set_int(&xb_array_items(shared[0].v.p)[1], 0); /* read before the first write */
  xb_ref_init(&shared[1], &shared[0], 2, 0); /* @a[2] */
  xb_item var;
  xb_set_int(&var, 0);
  xb_set_ptr(&shared[2], XB_T_CELL, xb_cell_move(ctx, &var, 0)); /* a variable in a cell */
  xb_item hash, key, zero;
  xb_set_hash(&hash, xb_hash_alloc());
  xb_gc_root_push(ctx, &hash);
  xb_set_str(&key, xb_str_new("key", 3));
  xb_gc_root_push(ctx, &key);
  xb_set_int(&zero, 0);
  xb_hash_set(hash.v.p, &key, &zero);
  xb_ref_elem(ctx, &shared[3], &hash, &key); /* @h["key"], then in a cell: a parameter passed so */
  xb_set_ptr(&shared[3], XB_T_CELL, xb_cell_move(ctx, &shared[3], 0));
  xb_gc_root_pop(ctx, 2);
  xbp_NEWBOX(ctx, &shared[4], 0, NULL);
  CHECK(shared[4].type == XB_T_OBJECT);
  for (int k = 0; k < SHARED; ++k) xb_share(&shared[k]);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
  xb_thread threads[WRITERS + READERS];
  xb_atomic_store32(&busy, WRITERS, XB_RELEASE);
  for (size_t k = 0; k < WRITERS; ++k) CHECK(xb_thread_start(&threads[k], (size_t)8 << 20, writer, (void *)k));
  for (size_t k = 0; k < READERS; ++k) CHECK(xb_thread_start(&threads[WRITERS + k], (size_t)8 << 20, reader, NULL));
  for (size_t k = 0; k < WRITERS + READERS; ++k) xb_thread_join(threads[k]);
  CHECK(bad == 0); /* references and cells */
  xb_gc_thread_attach(&base);
  ctx = xb_ctx_new();
  xb_item n;
  xb_set_int(&n, LAST);
  xbp_NUMBERS(ctx, &shared[0], 1, &n); /* the enumerator */
  xb_share(&shared[0]);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
  xb_atomic_store32(&busy, 1, XB_RELEASE);
  for (size_t k = 0; k < STEPPERS; ++k) CHECK(xb_thread_start(&threads[k], (size_t)8 << 20, stepper, NULL));
  for (size_t k = 0; k < STEPPERS; ++k) xb_thread_join(threads[k]);
  if (bad || steps != LAST || !values) fprintf(stderr, "bad %u, steps %u of %d, values %u\n", bad, steps, LAST, values);
  CHECK(bad == 0 && steps == LAST && values > 0);
  xb_gc_thread_attach(&base);
  ctx = xb_ctx_new();
  CHECK(!xb_gen_move_next(ctx, &shared[0])); /* every number given: it has ended */
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
  puts("shared_refs ok");
  return 0;
}
