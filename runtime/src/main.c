#include "xbrt_int.h"

#include <stdio.h>

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  xb_platform_init();
  xb_ctx *ctx = xb_ctx_new();
  xb_app_main(ctx);
  fflush(stdout);
  int rc = xb_ctx_exitcode(ctx);
  xb_ctx_free(ctx);
  return rc;
}
