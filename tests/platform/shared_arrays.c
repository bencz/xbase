/* The runtime's array operations on a shared array (types/array.c, rtl/arrays.c), threads at once, collections (full
 * ones, which move objects, too) running meanwhile. First appenders AAdd() strings of their own (shared as they are
 * stored, the array's cards marked) while readers index it and clone it (AClone): no reader sees a NIL or anything but
 * such a string, and in the end every string is there exactly once. Then writers assign elements (alternating integers
 * and doubles), AIns(), ADel(), ASort() and ASize() it while readers index it: a reader never sees an integer of a
 * double's bits or a tiny double (an item read torn), nor a value of another type. */
#include "check.h"
#include "core/shared.h"
#include "xbrt_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void xbf_AADD(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbf_ASIZE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbf_AINS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbf_ADEL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbf_ASORT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbf_ACLONE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

#define APPENDERS 3
#define READERS 2
#define ADDS 20000u
#define MIXERS 2
#define MIXES 20000u
#define SLOTS 64u

static xb_item shared;
static uint32_t bad, busy;

/* Each thread's context, its arguments rooted in it. */
typedef struct worker {
  xb_ctx *ctx;
  xb_item args[3], ret;
} worker;

static void begin(worker *w, void *base) {
  xb_gc_thread_attach(base);
  w->ctx = xb_ctx_new();
  for (int k = 0; k < 3; ++k) {
    xb_set_nil(&w->args[k]);
    xb_gc_root_push(w->ctx, &w->args[k]);
  }
  xb_set_nil(&w->ret);
  xb_gc_root_push(w->ctx, &w->ret);
}

static void end(worker *w) {
  xb_gc_root_pop(w->ctx, 4);
  xb_ctx_free(w->ctx);
  xb_gc_thread_detach();
}

static void poll(worker *w, uint32_t n) {
  if (xb_pending_get(w->ctx)) xb_pending_gc(w->ctx);
  if (n % 8192u == 4096u) xb_gc_collect(); /* full: objects may move */
  else if (n % 2048u == 0) xb_gc_collect_young();
}

/* The string appender `me` adds as its j-th, and back: its number me * ADDS + j, or -1 for anything else. */
static void tag(xb_item *it, uint32_t me, uint32_t j) {
  char buf[32];
  int n = snprintf(buf, sizeof buf, "n%u", me * ADDS + j);
  xb_set_str(it, xb_str_new(buf, (size_t)n));
}
static int64_t untag(const xb_item *it) {
  if (it->type != XB_T_STRING || it->v.s->len < 2 || it->v.s->data[0] != 'n') return -1;
  return strtol(it->v.s->data + 1, NULL, 10);
}

static void appender(void *arg) {
  worker w;
  begin(&w, &arg);
  uint32_t me = (uint32_t)(size_t)arg;
  for (uint32_t j = 0; j < ADDS; ++j) {
    w.args[0] = shared;
    tag(&w.args[1], me, j);
    xbf_AADD(w.ctx, &w.ret, 2, w.args);
    poll(&w, j);
  }
  xb_atomic_add32(&busy, (uint32_t)-1, XB_RELEASE);
  end(&w);
}

/* A value read from the shared array, as the phase allows it: a tagged string while it only grows, else NIL, an
 * integer below 2^32 or a double of at least 0.5. */
static int valid(const xb_item *v, int mixed) {
  if (!mixed) {
    int64_t k = untag(v);
    return k >= 0 && k < (int64_t)(APPENDERS * ADDS);
  }
  if (v->type == XB_T_NIL) return 1;
  if (v->type == XB_T_INT) return (uint64_t)v->v.i < ((uint64_t)1 << 32);
  return v->type == XB_T_DOUBLE && v->v.d >= 0.5;
}

static void reader(void *arg) {
  worker w;
  begin(&w, &arg);
  int mixed = (int)(size_t)arg;
  for (uint32_t n = 0; xb_atomic_load32(&busy, XB_ACQUIRE); ++n) {
    w.args[0] = shared;
    size_t len = xb_array_length(shared.v.p);
    if (len && !mixed) { /* the array only grows: the index operator, which raises errors, stays within it */
      xb_set_int(&w.args[1], (int64_t)(n % len) + 1);
      xb_op_index(w.ctx, &w.ret, &w.args[0], &w.args[1]);
      if (!valid(&w.ret, 0)) xb_atomic_add32(&bad, 1, XB_RELAXED);
    } else if (len && xb_array_get(shared.v.p, n % len, &w.ret) && !valid(&w.ret, 1)) { /* 0 past a shrunk end */
      xb_atomic_add32(&bad, 1, XB_RELAXED);
    }
    if (!mixed && n % 997u == 0) {
      xbf_ACLONE(w.ctx, &w.ret, 1, w.args);
      const xb_array *c = w.ret.v.p;
      for (size_t i = 0; i < c->len; ++i)
        if (!valid(&xb_array_items(c)[i], 0)) xb_atomic_add32(&bad, 1, XB_RELAXED);
    }
    poll(&w, n);
  }
  end(&w);
}

static void mixer(void *arg) {
  worker w;
  begin(&w, &arg);
  uint32_t me = (uint32_t)(size_t)arg;
  for (uint32_t j = 0; j < MIXES; ++j) {
    w.args[0] = shared;
    xb_set_int(&w.args[1], (int64_t)((j * 13u + me) % SLOTS) + 1);
    switch ((j + me) % 8u) {
      case 0: xbf_AINS(w.ctx, &w.ret, 2, w.args); break;
      case 1: xbf_ADEL(w.ctx, &w.ret, 2, w.args); break;
      case 2: xbf_ASORT(w.ctx, &w.ret, 1, w.args); break; /* mixed integers and doubles: ordered as numbers */
      case 3:
        xb_set_int(&w.args[1], (int64_t)(SLOTS + j % 8u));
        xbf_ASIZE(w.ctx, &w.ret, 2, w.args); /* the length changes; elements past it are lost */
        break;
      default: {
        if ((j / 8u + me) & 1u) xb_set_int(&w.args[2], (int64_t)j);
        else xb_set_double(&w.args[2], 0.5 + (double)j, 1);
        xb_array_put(shared.v.p, (size_t)(w.args[1].v.i - 1), &w.args[2]); /* 0 past a shrunk end */
      }
    }
    poll(&w, j);
  }
  xb_atomic_add32(&busy, (uint32_t)-1, XB_RELEASE);
  end(&w);
}

static void run(void (*fn)(void *), size_t writers, int mixed) {
  xb_thread threads[APPENDERS + READERS];
  xb_atomic_store32(&busy, (uint32_t)writers, XB_RELEASE);
  for (size_t k = 0; k < writers; ++k) CHECK(xb_thread_start(&threads[k], (size_t)8 << 20, fn, (void *)k));
  for (size_t k = 0; k < READERS; ++k)
    CHECK(xb_thread_start(&threads[writers + k], (size_t)8 << 20, reader, (void *)(size_t)mixed));
  for (size_t k = 0; k < writers + READERS; ++k) xb_thread_join(threads[k]);
}

int main(void) {
  xb_platform_init();
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_set_array(&shared, xb_array_alloc(0));
  xb_gc_add_root(&shared, 1);
  xb_share(&shared);
  xb_gc_thread_detach();
  run(appender, APPENDERS, 0);
  const xb_array *a = shared.v.p;
  CHECK(a->len == APPENDERS * ADDS);
  uint8_t *seen = calloc(APPENDERS * ADDS, 1);
  CHECK(seen);
  for (size_t i = 0; i < a->len; ++i) {
    int64_t k = untag(&xb_array_items(a)[i]);
    CHECK(k >= 0 && k < (int64_t)(APPENDERS * ADDS) && !seen[k] && xb_is_shared(xb_array_items(a)[i].v.s));
    seen[k] = 1;
  }
  free(seen);
  xb_gc_thread_attach(&base);
  xb_set_array(&shared, xb_array_alloc(SLOTS));
  xb_share(&shared);
  xb_gc_thread_detach();
  run(mixer, MIXERS, 1);
  CHECK(bad == 0);
  puts("shared_arrays ok");
  return 0;
}
