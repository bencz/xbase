/* Collections while other threads run compiled loops without calls (gc_loops.prg). Each loop reaches a safepoint
 * through its polls (a counted loop's at its stops, another's when its count of passes runs out), so every collection
 * ends: three threads spin in loops that never end (the test exits while they run; a loop that did not poll would hold
 * the first collection up until the test's time limit). A fourth thread runs Sums() over and over, its polls serving
 * the collections from the middle of its loops: its result is the same each time. */
#include "check.h"
#include "xbrt_int.h"

void xbp_SPINCOUNTED(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbp_SPINWHILE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbp_SPINPAST(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbp_SUMS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

#define SPINNERS 3
#define COLLECTIONS 2000

static const xb_func spinners[SPINNERS] = {xbp_SPINCOUNTED, xbp_SPINWHILE, xbp_SPINPAST};
static uint32_t started, stop, sums, wrong;

static void spin(void *which) {
  xb_gc_thread_attach(&which);
  xb_ctx *ctx = xb_ctx_new();
  xb_item ret;
  xb_set_nil(&ret);
  xb_atomic_add32(&started, 1, XB_RELEASE);
  spinners[(size_t)which](ctx, &ret, 0, NULL);
}

static int64_t sum(xb_ctx *ctx) {
  xb_item ret;
  xb_set_nil(&ret);
  xbp_SUMS(ctx, &ret, 0, NULL);
  return ret.type == XB_T_INT ? ret.v.i : -1;
}

static void sum_over(void *unused) {
  (void)unused;
  xb_gc_thread_attach(&unused);
  xb_ctx *ctx = xb_ctx_new();
  int64_t want = sum(ctx);
  xb_atomic_add32(&started, 1, XB_RELEASE);
  while (!xb_atomic_load32(&stop, XB_ACQUIRE)) {
    if (sum(ctx) != want) xb_atomic_add32(&wrong, 1, XB_RELAXED);
    xb_atomic_add32(&sums, 1, XB_RELAXED);
  }
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
}

/* Waiting for a thread is a blocking operation: a collection never waits for the waiter. */
static void join_summer(void *t) { xb_thread_join(*(xb_thread *)t); }

int main(void) {
  xb_platform_init();
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_ctx *ctx = xb_ctx_new();
  xb_thread threads[SPINNERS], summer;
  for (size_t k = 0; k < SPINNERS; ++k) CHECK(xb_thread_start(&threads[k], (size_t)8 << 20, spin, (void *)k));
  CHECK(xb_thread_start(&summer, (size_t)8 << 20, sum_over, NULL));
  while (xb_atomic_load32(&started, XB_ACQUIRE) != SPINNERS + 1) xb_platform_sleep_ms(1);
  for (int k = 0; k < COLLECTIONS; ++k) xb_gc_collect();
  xb_atomic_store32(&stop, 1, XB_RELEASE);
  xb_blocking_run(join_summer, &summer);
  CHECK(xb_atomic_load32(&wrong, XB_RELAXED) == 0 && xb_atomic_load32(&sums, XB_RELAXED) > 0);
  CHECK(!(xb_pending_get(ctx) & XB_PENDING_SAFEPOINT));
  puts("gc_loops ok");
  fflush(stdout);
  exit(0); /* the spinning loops still run */
}
