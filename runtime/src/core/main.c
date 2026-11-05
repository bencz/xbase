#include "xbrt_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The program's stack: compiled functions keep their item frames on it, so
 * deep recursion needs more than the default of a process's first thread
 * (1 MB on Windows). Reserved, not committed: unused pages cost nothing. */
#define XB_PROGRAM_STACK_SIZE ((size_t)256 << 20)

typedef struct program {
  int argc;
  char **argv;
  int exitcode;
} program;

/* The command-line arguments after the program's name become strings in a
 * frame of their own, linked first into the context's chain (so the collector
 * sees them), which the start function receives as its parameters. */
static void run_program(void *arg) {
  program *p = arg;
  xb_ctx *ctx = xb_ctx_new();
  uint32_t n = p->argc > 1 ? (uint32_t)(p->argc - 1) : 0;
  xb_frame *args = calloc(1, sizeof(xb_frame) + n * sizeof(xb_item)); /* NIL slots */
  if (!args) xb_gc_fatal("out of memory");
  args->nslots = n;
  args->prev = ctx->frames;
  ctx->frames = args;
  for (uint32_t i = 0; i < n; ++i) {
    const char *a = p->argv[i + 1];
    xb_set_str(&args->slots[i], xb_str_new(a, strlen(a)));
  }
  xb_app_main(ctx, n, args->slots);
  fflush(stdout);
  ctx->frames = args->prev;
  free(args);
  p->exitcode = xb_ctx_exitcode(ctx);
  xb_ctx_free(ctx);
}

int main(int argc, char **argv) {
  xb_platform_init();
  program p = {0, NULL, 0};
  p.argv = xb_platform_args(argc, argv, &p.argc);
  if (!xb_platform_run_on_stack(XB_PROGRAM_STACK_SIZE, run_program, &p)) run_program(&p);
  return p.exitcode;
}
