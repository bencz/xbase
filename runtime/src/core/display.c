#include "xbrt_int.h"

/* Same layout as xb_str with a one-byte payload; never collected. */
static const struct {
  uint32_t flags;
  uint32_t hash;
  size_t len;
  char data[1];
} empty_string = {XB_STR_STATIC, 0, 0, {0}};

xb_str *xb_item_display(const xb_ctx *ctx, const xb_item *it) {
  switch ((xb_type)it->type) {
    case XB_T_NIL:
      return xb_str_new("NIL", 3);
    case XB_T_LOGICAL:
      return xb_str_new(it->v.l ? ".T." : ".F.", 3);
    case XB_T_INT:
      return xb_num_to_str(it, it->width ? it->width : xb_num_int_width(it->v.i), it->dec);
    case XB_T_DOUBLE:
      return xb_num_to_str(it, it->width ? it->width : xb_num_dbl_width(it->v.d), it->dec);
    case XB_T_STRING:
      return it->v.s;
    case XB_T_DATE: {
      char out[XB_DATE_TEXT_SIZE];
      size_t n = xb_date_format(it->v.i, ctx->date_format, out);
      return xb_str_new(out, n);
    }
    case XB_T_ARRAY:
    case XB_T_BLOCK: /* shown as nothing, as in Clipper */
      return (xb_str *)&empty_string;
    default:
      return xb_str_new("<?>", 3);
  }
}
