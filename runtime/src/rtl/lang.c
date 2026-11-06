#include "xbrt_int.h"

void xbf_VALTYPE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  static const char codes[XB_T_COUNT_] = {
    [XB_T_NIL] = 'U',    [XB_T_LOGICAL] = 'L', [XB_T_INT] = 'N',  [XB_T_DOUBLE] = 'N', [XB_T_STRING] = 'C',
    [XB_T_ARRAY] = 'A',  [XB_T_BLOCK] = 'B',   [XB_T_CELL] = 'U', [XB_T_REF] = 'U',    [XB_T_OBJECT] = 'O',
    [XB_T_DATE] = 'D',
  };
  uint32_t t = argc ? argv[0].type : XB_T_NIL;
  char c = t < XB_T_COUNT_ ? codes[t] : 'U';
  xb_set_str(ret, xb_str_new(&c, 1));
}

void xbf_LEN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc && argv[0].type == XB_T_STRING) {
    xb_set_int(ret, (int64_t)argv[0].v.s->len);
    return;
  }
  if (argc && argv[0].type == XB_T_ARRAY) {
    xb_set_int(ret, (int64_t)((const xb_array *)argv[0].v.p)->len);
    return;
  }
  xb_arg_error(ctx, ret, 1111, "LEN", argc, argv);
}

/* QUIT / CANCEL: unwinds every active function and ends the program. */
void xbf___QUIT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_ctx_raise(ctx, XB_PENDING_QUIT);
}

/* Eval( bBlock [, xArgs...] ) */
void xbf_EVAL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1) {
    xb_error(ctx, ret, EG_NOMETHOD, 1004, "EVAL", XB_ERR_SUBSTITUTE, 0, argv);
    return;
  }
  xb_block_call(ctx, ret, &argv[0], argc - 1, argv + 1);
}

/* __GcCollect( [lYoung] ): a full collection (or a young one with .T.);
 * returns the live objects found by the last full collection. */
void xbf___GCCOLLECT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  if (argc > 0 && argv[0].type == XB_T_LOGICAL && argv[0].v.l) xb_gc_collect_young();
  else xb_gc_collect();
  xb_set_int(ret, (int64_t)xb_gc_live_objects());
}
