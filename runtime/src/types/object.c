#include "xbrt_int.h"

#include <string.h>

xb_object *xb_object_new(const xb_class *cls) {
  xb_object *o = xb_gc_alloc(XB_GC_OBJECT, offsetof(xb_object, vars) + cls->nvars * sizeof(xb_item));
  o->cls = cls;
  o->nvars = cls->nvars;
  for (uint32_t i = 0; i < cls->nvars; ++i) {
    switch (cls->vars[i].rule) {
      case XB_VAR_ANY:
      case XB_VAR_ARRAY: xb_set_nil(&o->vars[i]); break;
      case XB_VAR_LOGICAL: xb_set_logical(&o->vars[i], 0); break;
      case XB_VAR_STRING: xb_set_str(&o->vars[i], xb_str_empty()); break;
      case XB_VAR_INT32:
      case XB_VAR_UINT16: xb_set_int(&o->vars[i], 0); break;
    }
  }
  return o;
}

void xb_set_object(xb_item *it, xb_object *o) { xb_set_ptr(it, XB_T_OBJECT, o); }

/* What ClassName() returns for a value that is not an object. */
static const char *const type_class_names[XB_T_COUNT_] = {
  [XB_T_NIL] = "NIL",      [XB_T_LOGICAL] = "LOGICAL", [XB_T_INT] = "NUMERIC", [XB_T_DOUBLE] = "NUMERIC",
  [XB_T_STRING] = "CHARACTER", [XB_T_ARRAY] = "ARRAY", [XB_T_BLOCK] = "BLOCK",  [XB_T_DATE] = "DATE",
};

/* A number truncated toward zero and wrapped to 32 bits signed (0 for a
 * double beyond 64 bits). */
static int64_t wrap_int32(const xb_item *n) {
  int64_t i = n->type == XB_T_INT ? n->v.i : xb_dbl_fits_i64(n->v.d) ? (int64_t)n->v.d : 0;
  uint32_t low = (uint32_t)i;
  return low < UINT32_C(0x80000000) ? (int64_t)low : (int64_t)low - (INT64_C(1) << 32);
}

/* Assigns variable `k` of `o`. The result (the value of the assignment
 * expression) is the value kept, with these exceptions: a string variable
 * returns its argument even when it ignores it, an ignored array or logical
 * gives NIL, and a 16-bit variable returns the number wrapped to 32 bits only. */
static void assign_var(xb_ctx *ctx, xb_item *r, xb_object *o, uint32_t k, const xb_item *value) {
  const xb_class_var *var = &o->cls->vars[k];
  xb_item kept = *value;
  switch (var->rule) {
    case XB_VAR_ANY: break;
    case XB_VAR_ARRAY: if (value->type != XB_T_ARRAY) return; break;
    case XB_VAR_LOGICAL: if (value->type != XB_T_LOGICAL) return; break;
    case XB_VAR_STRING:
      *r = *value;
      if (value->type != XB_T_STRING) return;
      break;
    case XB_VAR_INT32:
    case XB_VAR_UINT16: {
      if (!xb_is_number(value)) {
        xb_arg_error(ctx, r, 0, var->name, 1, value);
        return;
      }
      int64_t i = wrap_int32(value);
      xb_set_int(r, i);
      xb_set_int(&o->vars[k], var->rule == XB_VAR_UINT16 ? (uint16_t)i : i); /* no barrier: not a heap value */
      return;
    }
  }
  o->vars[k] = kept;
  xb_gc_write(o);
  *r = kept;
}

void xb_op_send(xb_ctx *ctx, xb_item *r, const char *message, uint32_t argc, xb_item *argv) {
  int assign = message[0] == '_';
  const char *name = message + assign;
  if (argv[0].type == XB_T_OBJECT) {
    xb_object *o = argv[0].v.p;
    const xb_class *cls = o->cls;
    for (uint32_t k = 0; k < cls->nvars; ++k)
      if (strcmp(cls->vars[k].name, name) == 0) {
        if (!assign) {
          *r = o->vars[k]; /* arguments are ignored */
        } else {
          xb_item nil;
          xb_set_nil(&nil);
          assign_var(ctx, r, o, k, argc > 1 ? &argv[1] : &nil);
        }
        return;
      }
  }
  if (!assign && strcmp(message, "CLASSNAME") == 0) {
    const char *cn = argv[0].type == XB_T_OBJECT ? ((const xb_object *)argv[0].v.p)->cls->name
                     : argv[0].type < XB_T_COUNT_ && type_class_names[argv[0].type] ? type_class_names[argv[0].type]
                                                                                    : "NIL";
    xb_set_str(r, xb_str_new(cn, strlen(cn)));
    return;
  }
  if (assign) xb_error(ctx, r, EG_NOVARMETHOD, 1005, name, XB_ERR_SUBSTITUTE, argc, argv);
  else xb_error(ctx, r, EG_NOMETHOD, 1004, name, XB_ERR_SUBSTITUTE, argc, argv);
}
