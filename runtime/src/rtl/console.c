#include "xbrt_int.h"

#include <stdio.h>

/* A write to the standard output or error, which blocks while the system takes the bytes (a pipe that is full): run
 * as a blocking operation, the text rooted meanwhile (the display of a number is a new string). */
typedef struct text_write {
  const xb_str *s;
  FILE *out;
} text_write;

static void run_write(void *p) {
  const text_write *w = p;
  if (w->s->len == 1) fputc((unsigned char)w->s->data[0], w->out); /* a separator: much cheaper than fwrite */
  else fwrite(w->s->data, 1, w->s->len, w->out);
}

static void write_text(xb_ctx *ctx, const xb_str *s, FILE *out) {
  xb_item held;
  xb_set_str(&held, (xb_str *)(uintptr_t)s);
  xb_gc_root_push(ctx, &held);
  text_write w = {s, out};
  xb_blocking_run(run_write, &w);
  xb_gc_root_pop(ctx, 1);
}

/* __ConsoleWrite( xValue ): writes the display form of xValue (a string as it
 * is, numbers, dates and logicals as QOut() shows them) to the console. The
 * console is the standard output until the terminal layer exists. */
void xbf___CONSOLEWRITE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  if (!argc) return;
  write_text(ctx, xb_item_display(ctx, &argv[0]), stdout);
}

/* __StdWrite( nHandle, xValue ): writes the display form of xValue to the
 * standard output (handle 1) or the standard error (handle 2), never to the
 * console; anything else is ignored. The standard output is flushed before
 * the standard error is written, so the two keep their order on a terminal. */
void xbf___STDWRITE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  if (argc < 2 || !xb_is_number(&argv[0])) return;
  int64_t handle = xb_num_to_i64(&argv[0]);
  if (handle != 1 && handle != 2) return;
  FILE *out = handle == 1 ? stdout : stderr;
  if (out == stderr) xb_platform_flush(stdout);
  write_text(ctx, xb_item_display(ctx, &argv[1]), out);
}
