#include "xbrt_int.h"

#include "core/sched.h"
#include "term/screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The program's stack: compiled functions keep their item frames on it, so deep recursion needs more than the default
 * of a process's first thread (1 MB on Windows). Reserved, not committed: unused pages cost nothing, so a 64-bit
 * target, whose calls may take twice the stack of others (the ppc64 ELF ABI's), reserves 1 GB; a 32-bit one, whose
 * address space is small, 384 MB (200000 levels of a function of a dozen statements, as the recursion_deep test
 * makes, take about 270 MB there: an item is four registers of the few a 32-bit x86 has). */
#define XB_PROGRAM_STACK_SIZE (sizeof(void *) >= 8 ? (size_t)1 << 30 : (size_t)384 << 20)

typedef struct program {
  int argc;
  char **argv;
} program;

/* What follows the option's name (`name`, upper case) in `arg`, any letter case, or NULL. */
static const char *option_value(const char *arg, const char *name) {
  size_t k = 0;
  while (name[k] && xb_ascii_upper(arg[k]) == name[k]) ++k;
  return name[k] ? NULL : arg + k;
}

/* A runtime option, an argument that starts with //, which the program does not receive: //WORKERS:n sets the
 * workers that run tasks at once (core/sched.c), //TERM:name the terminal driver (term/drivers.c); any other is
 * ignored. Letters in any case. */
static void runtime_option(const char *arg) {
  const char *value;
  if ((value = option_value(arg, "WORKERS:")) != NULL) {
    unsigned long n = strtoul(value, NULL, 10);
    if (n) xb_sched_set_workers(n > UINT32_MAX ? UINT32_MAX : (unsigned)n);
  } else if ((value = option_value(arg, "TERM:")) != NULL) {
    xb_term_start(value);
  }
}

/* The command-line arguments after the program's name, but the runtime options, become strings in a frame of their
 * own, linked first into the context's chain (so the collector sees them), which the start function receives as its
 * parameters. */
XB_NOINLINE static void run(program *p) {
  int kept = 1;
  for (int i = 1; i < p->argc; ++i) {
    if (strncmp(p->argv[i], "//", 2) == 0) runtime_option(p->argv[i] + 2);
    else p->argv[kept++] = p->argv[i];
  }
  p->argc = kept;
  xb_ctx *ctx = xb_ctx_new();
  const char *low, *high;
  xb_platform_stack_bounds(&low, &high);
  xb_ctx_set_stack(ctx, low, high);
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
  xb_term_end();
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
  xb_gc_trim_end();
  xb_platform_threads_wait(); /* no thread of the runtime's ends with the process: it keeps the program's exit code */
  return xb_exit_code();
}
