#include "xbrt_int.h"

void xbf_VALTYPE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  static const char codes[XB_T_COUNT_] = { 'U', 'L', 'N', 'N', 'C' };
  uint32_t t = argc ? argv[0].type : XB_T_NIL;
  char c = t < XB_T_COUNT_ ? codes[t] : 'U';
  xb_set_str(ret, xb_str_new(&c, 1));
}

void xbf_LEN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc && argv[0].type == XB_T_STRING) {
    xb_set_int(ret, (int64_t)argv[0].v.s->len);
    return;
  }
  xb_rt_error(ctx, "BASE", 1111, "Argument error", "LEN");
}

/* QUIT / CANCEL: unwinds every active function and ends the program. */
void xbf___QUIT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_ctx_raise(ctx, XB_PENDING_QUIT);
}
