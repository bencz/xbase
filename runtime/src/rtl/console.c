#include "xbrt_int.h"

#include <stdio.h>

/* Console output. Will move behind the terminal (GT) layer. */
static void out_args(const xb_ctx *ctx, uint32_t argc, const xb_item *argv) {
  for (uint32_t i = 0; i < argc; ++i) {
    if (i) fputc(' ', stdout);
    const xb_str *s = xb_item_display(ctx, &argv[i]);
    fwrite(s->data, 1, s->len, stdout);
  }
}

void xbf_QOUT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  fputc('\n', stdout);
  out_args(ctx, argc, argv);
}

void xbf_QQOUT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  out_args(ctx, argc, argv);
}
