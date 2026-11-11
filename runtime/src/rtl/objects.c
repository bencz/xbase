/* Reflection on objects, with xbase names: ObjIsKindOf, ObjHasVar,
 * ObjHasMethod, ObjVars, ObjMethods and ObjSend. Names are compared in any
 * letter case. They see objects of compiled classes (types/class.c) and of
 * the runtime's own classes (their variables; they have no methods). */
#include "xbrt_int.h"

#include <stdlib.h>
#include <string.h>

/* A name argument upper-cased in `buf` (`size` bytes), or in memory the caller
 * frees when it is longer; NULL when the argument is not a string. */
#define NAME_SIZE 128
static char *upper_name(const xb_item *it, char buf[NAME_SIZE]) {
  if (it->type != XB_T_STRING) return NULL;
  const xb_str *s = it->v.s;
  char *out = s->len < NAME_SIZE ? buf : malloc(s->len + 1);
  if (!out) xb_gc_fatal("out of memory");
  for (size_t k = 0; k < s->len; ++k) out[k] = xb_ascii_upper(s->data[k]);
  out[s->len] = '\0';
  return out;
}

static void free_name(char *name, const char buf[NAME_SIZE]) {
  if (name != buf) free(name);
}

static const xb_object *object_arg(uint32_t argc, const xb_item *argv) {
  return argc && argv[0].type == XB_T_OBJECT ? argv[0].v.p : NULL;
}

/* ObjIsKindOf( oObject, cClass ): whether the object's class is cClass or
 * inherits from it. */
void xbf_OBJISKINDOF(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  const xb_object *o = object_arg(argc, argv);
  char buf[NAME_SIZE];
  char *name = argc > 1 ? upper_name(&argv[1], buf) : NULL;
  int yes = 0;
  if (o && name) yes = o->cls->compiled ? xb_class_kind_of(o, name) : strcmp(o->cls->name, name) == 0;
  if (name) free_name(name, buf);
  xb_set_logical(ret, yes);
}

static void has(xb_item *ret, uint32_t argc, xb_item *argv, int method) {
  const xb_object *o = object_arg(argc, argv);
  char buf[NAME_SIZE];
  char *name = argc > 1 ? upper_name(&argv[1], buf) : NULL;
  int yes = 0;
  if (o && name) {
    if (o->cls->compiled) {
      yes = xb_class_has(o, name, method);
    } else if (!method) {
      for (uint32_t k = 0; k < o->cls->nvars && !yes; ++k) yes = strcmp(o->cls->vars[k].name, name) == 0;
    }
  }
  if (name) free_name(name, buf);
  xb_set_logical(ret, yes);
}

/* ObjHasVar( oObject, cName ), ObjHasMethod( oObject, cName ): whether the
 * object has a variable (or a method, ACCESS and ASSIGN included) answering
 * cName, whatever its scope. */
void xbf_OBJHASVAR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  has(ret, argc, argv, 0);
}
void xbf_OBJHASMETHOD(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  has(ret, argc, argv, 1);
}

static void names(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, int methods, const char *fn) {
  const xb_object *o = object_arg(argc, argv);
  if (!o) {
    xb_fn_arg_error(ctx, ret, 0, fn, argc, argv);
    return;
  }
  uint32_t n = o->cls->compiled ? xb_class_names(o, methods, NULL) : methods ? 0 : o->cls->nvars;
  const char **list = malloc((n ? n : 1) * sizeof *list);
  if (!list) xb_gc_fatal("out of memory");
  if (o->cls->compiled) xb_class_names(o, methods, list);
  else
    for (uint32_t k = 0; k < n; ++k) list[k] = o->cls->vars[k].name;
  xb_array *a = xb_array_alloc(n);
  xb_set_array(ret, a); /* rooted there while the names are made */
  for (uint32_t k = 0; k < n; ++k) {
    xb_set_str(&a->items[k], xb_str_new(list[k], strlen(list[k])));
    xb_gc_write(a);
  }
  free(list);
}

/* ObjVars( oObject ), ObjMethods( oObject ): the names of the EXPORTED
 * variables (or methods, ACCESS included) the object answers, upper-cased, in
 * the order its class tables them; anything but an object is BASE/0. */
void xbf_OBJVARS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  names(ctx, ret, argc, argv, 0, "OBJVARS");
}
void xbf_OBJMETHODS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  names(ctx, ret, argc, argv, 1, "OBJMETHODS");
}

/* ObjSend( xValue, cMessage, [xArgs...] ): the message cMessage ("_NAME"
 * assigns) sent to xValue with the arguments, as xValue:cMessage( xArgs )
 * from outside its class; a cMessage that is not a string is BASE/0. */
void xbf_OBJSEND(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  char buf[NAME_SIZE];
  char *message = argc > 1 ? upper_name(&argv[1], buf) : NULL;
  if (!message) {
    xb_fn_arg_error(ctx, ret, 0, "OBJSEND", argc, argv);
    return;
  }
  argv[1] = argv[0]; /* the receiver, then the arguments */
  xb_op_send(ctx, ret, message, argc - 1, argv + 1);
  free_name(message, buf);
}
