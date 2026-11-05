#include "xbrt_int.h"

xb_str *xb_item_display(const xb_item *it) {
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
    default:
      return xb_str_new("<?>", 3);
  }
}
