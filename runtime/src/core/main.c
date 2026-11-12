#include "xbrt_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The program's stack: compiled functions keep their item frames on it, so
 * deep recursion needs more than the default of a process's first thread
 * (1 MB on Windows). Reserved, not committed: unused pages cost nothing. */
#define XB_PROGRAM_STACK_SIZE ((size_t)256 << 20)

static int64_t started; /* the monotonic clock when main began */

int64_t xb_program_elapsed_ms(void) { return (xb_platform_monotonic_ns() - started) / XB_NS_PER_MS; }

typedef struct program {
  int argc;
  char **argv;
  int exitcode;
} program;

/* The command-line arguments after the program's name become strings in a
 * frame of their own, linked first into the context's chain (so the collector
 * sees them), which the start function receives as its parameters. */
XB_NOINLINE static void run(program *p) {
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

/* The thread's first function: its frame lies above every frame the program's code has, so collections that move
 * objects scan the stack from it (gc.c). */
static void run_program(void *arg) {
  volatile int returned = 0;
  xb_gc_thread_stack((const void *)&arg);
  run(arg);
  returned = 1; /* a store after the call: run() is no tail call, which would put its frame over this one */
  (void)returned;
}

int main(int argc, char **argv) {
  xb_platform_init();
  started = xb_platform_monotonic_ns();
  program p = {0, NULL, 0};
  p.argv = xb_platform_args(argc, argv, &p.argc);
  xb_thread thread;
  if (xb_thread_start(&thread, XB_PROGRAM_STACK_SIZE, run_program, &p)) xb_thread_join(thread);
  else run_program(&p);
  return p.exitcode;
}
