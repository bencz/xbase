/* When the system gives no thread (RLIMIT_NPROC at 1 for the user; skipped for root, whom it does not bind, and on
 * Windows): the timers' thread cannot start, which is said once on standard error, and a timer then never fires, so
 * TaskDelay():Wait( nMs ) gives .F. at its limit instead of waiting for ever (no_threads.prg). */
#define _POSIX_C_SOURCE 200809L /* fileno, dup, dup2 */
#include "check.h"
#include "xbrt_int.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
int main(void) {
  puts("no_threads skipped (Windows)");
  return 0;
}
#else
#include <sys/resource.h>
#include <unistd.h>

void xbp_TWODELAYS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

int main(void) {
  if (geteuid() == 0) {
    puts("no_threads skipped (root)");
    return 0;
  }
  xb_platform_init();
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_ctx *ctx = xb_ctx_new();
  struct rlimit one = {1, 1};
  if (setrlimit(RLIMIT_NPROC, &one) != 0) {
    puts("no_threads skipped (no limit)");
    return 0;
  }
  FILE *err = tmpfile();
  CHECK(err);
  fflush(stderr);
  int saved = dup(fileno(stderr));
  CHECK(saved >= 0 && dup2(fileno(err), fileno(stderr)) >= 0);
  xb_item r;
  xb_set_nil(&r);
  xb_gc_root_push(ctx, &r);
  xbp_TWODELAYS(ctx, &r, 0, NULL);
  fflush(stderr);
  CHECK(dup2(saved, fileno(stderr)) >= 0);
  CHECK(r.type == XB_T_ARRAY && xb_array_length(r.v.p) == 2);
  for (uint32_t k = 0; k < 2; ++k) {
    xb_item v;
    CHECK(xb_array_get(r.v.p, k, &v) && v.type == XB_T_LOGICAL && !v.v.l);
  }
  char text[512] = {0};
  rewind(err);
  size_t got = fread(text, 1, sizeof text - 1, err);
  text[got] = 0;
  const char *said = "the timers' thread cannot start";
  const char *first = strstr(text, said);
  CHECK(first && !strstr(first + 1, said)); /* said once */
  puts("no_threads ok");
  return 0;
}
#endif
