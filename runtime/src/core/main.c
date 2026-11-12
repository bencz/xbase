#include "xbrt_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The program's stack: compiled functions keep their item frames on it, so
 * deep recursion needs more than the default of a process's first thread
 * (1 MB on Windows). Reserved, not committed: unused pages cost nothing, so a
 * 64-bit target, whose calls may take twice the stack of others (the ppc64
 * ELF ABI's), reserves 1 GB; a 32-bit one, whose address space is small,
 * 256 MB. */
#define XB_PROGRAM_STACK_SIZE (sizeof(void *) >= 8 ? (size_t)1 << 30 : (size_t)256 << 20)

typedef struct program {
  int argc;
  char **argv;
} program;

/* The command-line arguments after the program's name become strings in a
 * frame of their own, linked first into the context's chain (so the collector
 * sees them), which the start function receives as its parameters. */
XB_NOINLINE static void run(program *p) {
  xb_ctx *ctx = xb_ctx_new();
  const char *low, *high;
  xb_platform_stack_bounds(&low, &high);
  xb_ctx_set_stack(ctx, low);
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
  xb_platform_flush(stdout);
  ctx->frames = args->prev;
  free(args);
  xb_ctx_free(ctx);
}

/* The thread's first function: its frame lies above every frame the program's code has, so collections scan the
 * stack from it (gc.c). */
static void run_program(void *arg) {
  volatile int returned = 0;
  xb_gc_thread_attach((const void *)&arg);
  run(arg);
  xb_gc_thread_detach();
  returned = 1; /* a store after the call: run() is no tail call, which would put its frame over this one */
  (void)returned;
}

int main(int argc, char **argv) {
  xb_platform_init();
  program p = {0, NULL};
  p.argv = xb_platform_args(argc, argv, &p.argc);
  xb_thread thread;
  if (xb_thread_start(&thread, XB_PROGRAM_STACK_SIZE, run_program, &p)) xb_thread_join(thread);
  else run_program(&p); /* on this thread, with the stack the system gave it */
  return xb_exit_code();
}
