#include "xbrt_int.h"

#include <stdio.h>

/* The program's stack: compiled functions keep their item frames on it, so
 * deep recursion needs more than the default of a process's first thread
 * (1 MB on Windows). Reserved, not committed: unused pages cost nothing. */
#define XB_PROGRAM_STACK_SIZE ((size_t)256 << 20)

static void run_program(void *exitcode) {
  xb_ctx *ctx = xb_ctx_new();
  xb_app_main(ctx);
  fflush(stdout);
  *(int *)exitcode = xb_ctx_exitcode(ctx);
  xb_ctx_free(ctx);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  xb_platform_init();
  int rc = 0;
  if (!xb_platform_run_on_stack(XB_PROGRAM_STACK_SIZE, run_program, &rc)) run_program(&rc);
  return rc;
}
