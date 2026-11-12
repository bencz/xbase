/* Blocking operations beside collections: two threads open and close one file through the registry of shared files
 * (whose lock is held across file operations) and read it into a string they keep rooted, while another thread
 * collects, young and full, all along. Nothing waits forever, and every read fills its string whole. */
#include "check.h"
#include "core/shared_file.h"
#include "xbrt_int.h"

#include <stdio.h>
#include <string.h>

#define ROUNDS 300u
#define SIZE 4000u
#define FILE_NAME "gc_blocking.tmp"

static uint32_t bad, readers = 2;

static void reader(void *arg) {
  xb_gc_thread_attach(&arg);
  xb_ctx *ctx = xb_ctx_new();
  xb_item held;
  xb_set_nil(&held);
  xb_gc_root_push(ctx, &held);
  for (uint32_t round = 0; round < ROUNDS; ++round) {
    xb_shared *s;
    int32_t err;
    if (!xb_shared_open(FILE_NAME, 1, 0, 0, &s, &err)) {
      xb_atomic_add32(&bad, 1, XB_RELAXED);
      continue;
    }
    xb_str *buf = xb_str_alloc(SIZE); /* rooted while the read fills it: another thread collects meanwhile */
    xb_set_str(&held, buf);
    int64_t got = xb_platform_file_read_at(xb_shared_handle(s), 0, buf->data, SIZE, &err);
    if (got != SIZE) xb_atomic_add32(&bad, 1, XB_RELAXED);
    for (uint32_t k = 0; k < SIZE; ++k)
      if (buf->data[k] != (char)('a' + k % 26)) {
        xb_atomic_add32(&bad, 1, XB_RELAXED);
        break;
      }
    xb_shared_close(s, &err);
    if (xb_pending_get(ctx)) xb_pending_gc(ctx);
  }
  xb_gc_root_pop(ctx, 1);
  xb_ctx_free(ctx);
  xb_atomic_add32(&readers, (uint32_t)-1, XB_RELEASE);
  xb_gc_thread_detach();
}

static void collector(void *arg) {
  xb_gc_thread_attach(&arg);
  xb_ctx *ctx = xb_ctx_new();
  xb_item junk;
  xb_set_nil(&junk);
  xb_gc_root_push(ctx, &junk);
  for (uint32_t n = 0; xb_atomic_load32(&readers, XB_ACQUIRE); ++n) {
    for (uint32_t k = 0; k < 200; ++k) xb_set_str(&junk, xb_str_new("garbage of the collector's own", 30));
    if (n % 7 == 0) xb_gc_collect();
    else xb_gc_collect_young();
  }
  xb_gc_root_pop(ctx, 1);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
}

int main(void) {
  xb_platform_init();
  char data[SIZE];
  for (uint32_t k = 0; k < SIZE; ++k) data[k] = (char)('a' + k % 26);
  xb_file f;
  int32_t err;
  CHECK(xb_platform_file_create(FILE_NAME, 0, &f, &err) && xb_platform_file_write(f, data, SIZE, &err) &&
        xb_platform_file_close(f, &err));
  xb_thread threads[3];
  CHECK(xb_thread_start(&threads[0], 0, collector, NULL));
  CHECK(xb_thread_start(&threads[1], 0, reader, NULL));
  CHECK(xb_thread_start(&threads[2], 0, reader, NULL));
  for (size_t k = 0; k < 3; ++k) xb_thread_join(threads[k]);
  xb_platform_file_erase(FILE_NAME, &err);
  CHECK(bad == 0);
  puts("gc_blocking ok");
  return 0;
}
