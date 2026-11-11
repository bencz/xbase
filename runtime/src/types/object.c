#include "xbrt_int.h"

#include <stdio.h>
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
  [XB_T_HASH] = "HASH",
};

const char *xb_message_text(char buf[XB_MESSAGE_TEXT_SIZE], const char *cls, const char *message) {
  size_t n = 0;
  for (const char *s = cls; *s && n + 1 < XB_MESSAGE_TEXT_SIZE; ++s) buf[n++] = *s;
  if (n + 1 < XB_MESSAGE_TEXT_SIZE) buf[n++] = ':';
  for (const char *s = message; *s && n + 1 < XB_MESSAGE_TEXT_SIZE; ++s) buf[n++] = *s;
  buf[n] = '\0';
  return buf;
}

/* What the call stack shows of a message while its error block runs:
 * "CLASS:MESSAGE" for an object (MESSAGE as sent: "_NAME" to assign), else
 * the message. */
static const char *message_frame(char buf[XB_MESSAGE_TEXT_SIZE], const xb_item *self, const char *message) {
  if (self->type != XB_T_OBJECT) return message;
  return xb_message_text(buf, ((const xb_object *)self->v.p)->cls->name, message);
}

/* Assigns variable `k` of `o`. The result (the value of the assignment
 * expression) is the value kept, with these exceptions: a string variable
 * returns its argument even when it ignores it, an ignored array or logical
 * gives NIL, and a 16-bit variable returns the number wrapped to 32 bits only. */
static void assign_var(xb_ctx *ctx, xb_item *r, const xb_item *self, uint32_t k, const xb_item *value) {
  xb_object *o = self->v.p;
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
        char frame[XB_MESSAGE_TEXT_SIZE], message[XB_MESSAGE_TEXT_SIZE];
        snprintf(message, sizeof message, "_%s", var->name);
        xb_error_in(ctx, message_frame(frame, self, message), r, EG_ARG, 0, var->name, XB_ERR_SUBSTITUTE, 1, value);
        return;
      }
      int64_t i = xb_num_wrap_int32(value);
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
    if (cls->compiled) {
      xb_class_send(ctx, r, message, NULL, 0, argc, argv);
      return;
    }
    if (xb_is_generator(&argv[0]) && xb_gen_message(ctx, r, message, &argv[0])) return;
    for (uint32_t k = 0; k < cls->nvars; ++k)
      if (strcmp(cls->vars[k].name, name) == 0) {
        if (!assign) {
          *r = o->vars[k]; /* arguments are ignored */
        } else {
          xb_item nil;
          xb_set_nil(&nil);
          assign_var(ctx, r, &argv[0], k, argc > 1 ? &argv[1] : &nil);
        }
        return;
      }
  }
  if (!xb_class_scalar_send(ctx, r, message, NULL, 0, argc, argv)) xb_object_message(ctx, r, message, argc, argv);
}

const char *xb_type_class_name(xb_type type) { return type < XB_T_COUNT_ ? type_class_names[type] : NULL; }

void xb_object_message(xb_ctx *ctx, xb_item *r, const char *message, uint32_t argc, xb_item *argv) {
  int assign = message[0] == '_';
  const char *name = message + assign;
  if (!assign && strcmp(message, "CLASSNAME") == 0) {
    const char *cn = argv[0].type == XB_T_OBJECT ? ((const xb_object *)argv[0].v.p)->cls->name
                                                 : xb_type_class_name((xb_type)argv[0].type);
    if (!cn) cn = "NIL";
    xb_set_str(r, xb_str_new(cn, strlen(cn)));
    return;
  }
  char frame[XB_MESSAGE_TEXT_SIZE];
  const char *shown = message_frame(frame, &argv[0], message);
  if (assign) xb_error_in(ctx, shown, r, EG_NOVARMETHOD, 1005, name, XB_ERR_SUBSTITUTE, argc, argv);
  else xb_error_in(ctx, shown, r, EG_NOMETHOD, 1004, name, XB_ERR_SUBSTITUTE, argc, argv);
}
