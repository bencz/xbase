#include "xbrt_int.h"

#include <stdio.h>

/* __ConsoleWrite( xValue ): writes the display form of xValue (a string as it
 * is, numbers, dates and logicals as QOut() shows them) to the console. The
 * console is the standard output until the terminal layer exists. */
void xbf___CONSOLEWRITE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  if (!argc) return;
  const xb_str *s = xb_item_display(ctx, &argv[0]);
  if (s->len == 1) fputc((unsigned char)s->data[0], stdout); /* a separator: much cheaper than fwrite */
  else fwrite(s->data, 1, s->len, stdout);
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
  if (out == stderr) fflush(stdout);
  const xb_str *s = xb_item_display(ctx, &argv[1]);
  fwrite(s->data, 1, s->len, out);
}
