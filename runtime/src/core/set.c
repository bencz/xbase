/* Set( nSetting [, xNewValue] ): returns the setting's value before the call. */
#include "xbrt_int.h"

#include "set.ch"

/* Logical settings also accept "ON"/"OFF" (any case), the form SET commands
 * pass; any other value leaves the setting unchanged. */
static int set_logical(const xb_item *v, int old) {
  if (v->type == XB_T_LOGICAL) return v->v.l != 0;
  if (v->type == XB_T_STRING) {
    const xb_str *s = v->v.s;
    char c0 = s->len > 0 ? (char)(s->data[0] | 0x20) : 0;
    char c1 = s->len > 1 ? (char)(s->data[1] | 0x20) : 0;
    char c2 = s->len > 2 ? (char)(s->data[2] | 0x20) : 0;
    if (c0 == 'o' && c1 == 'n') return 1;
    if (c0 == 'o' && c1 == 'f' && c2 == 'f') return 0;
  }
  return old;
}

void xbf_SET(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || !xb_is_number(&argv[0])) return;
  int64_t which = xb_num_to_i64(&argv[0]);
  const xb_item *value = argc > 1 ? &argv[1] : NULL;

  switch (which) {
    case _SET_EXACT:
      xb_set_logical(ret, ctx->set_exact);
      if (value) ctx->set_exact = set_logical(value, ctx->set_exact);
      break;
    case _SET_DECIMALS:
      xb_set_int(ret, ctx->set_decimals);
      if (value && xb_is_number(value)) {
        int64_t n = xb_num_to_i64(value);
        if (n < 0) xb_rt_error(ctx, "BASE", 2020, "Argument error", "SET");
        else ctx->set_decimals = n > INT32_MAX ? INT32_MAX : (int32_t)n;
      }
      break;
    default:
      break;
  }
}
