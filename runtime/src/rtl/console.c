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
