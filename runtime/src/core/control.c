/* Runtime support of FOR EACH, AWAIT FOR EACH, SWITCH, USING and `...`. */
#include "xbrt_int.h"

#include "rtl/flows.h" /* xb_task_completed: AWAIT FOR EACH */

#include <string.h>

/* Whether the object answers `message`: with a method (a compiled class's, or a runtime class's), or with a variable
 * too (`orVar`). */
static int answers(const xb_item *it, const char *message, int orVar) {
  if (it->type != XB_T_OBJECT) return 0;
  const xb_object *o = it->v.p;
  if (o->cls->compiled) return xb_class_has(it->v.p, message, 1) || (orVar && xb_class_has(it->v.p, message, 0));
  if (xb_class_method_of(o->cls, message)) return 1;
  for (uint32_t k = 0; orVar && k < o->cls->nvars; ++k)
    if (xb_var_answers(&o->cls->vars[k], message)) return 1;
  return 0;
}

/* `message` sent to `obj` with no arguments, its result in `r`; 0 after an error. */
static int send0(xb_ctx *ctx, xb_item *r, const xb_item *obj, const char *message) {
  xb_item self = *obj;
  xb_gc_root_push(ctx, &self);
  xb_op_send(ctx, r, message, 1, &self);
  xb_gc_root_pop(ctx, 1);
  return !(xb_pending_get(ctx) && xb_pending(ctx));
}

/* Whether FOR EACH goes through the object: a generator's enumerator, one with GetEnumerator(), or an enumerator
 * (MoveNext() and Current). */
static int is_collection_object(const xb_item *it) {
  if (xb_is_generator(it) || answers(it, "GETENUMERATOR", 0)) return 1;
  return answers(it, "MOVENEXT", 0) && answers(it, "CURRENT", 1);
}

/* __IsCollection( xValue ): whether FOR EACH goes through xValue: an array, a string, a hash or a collection object. */
void xbf___ISCOLLECTION(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  if (argc < 1) {
    xb_set_logical(ret, 0);
    return;
  }
  xb_type t = argv[0].type;
  xb_set_logical(ret, t == XB_T_ARRAY || t == XB_T_STRING || t == XB_T_HASH || is_collection_object(&argv[0]));
}

/* An enumerator's next element: MoveNext() is .T.; 0 at the end or after an error. */
static int move_next(xb_ctx *ctx, const xb_item *e) {
  if (xb_is_generator(e)) return xb_gen_move_next(ctx, e);
  xb_item moved;
  xb_set_nil(&moved);
  xb_gc_root_push(ctx, &moved);
  int ok = send0(ctx, &moved, e, "MOVENEXT") && moved.type == XB_T_LOGICAL && moved.v.l;
  xb_gc_root_pop(ctx, 1);
  return ok;
}

/* The current element of an enumerator; 0 after an error. */
static int current(xb_ctx *ctx, xb_item *r, const xb_item *e) {
  if (xb_is_generator(e)) {
    xb_gen_current(e, r);
    return 1;
  }
  return send0(ctx, r, e, "CURRENT");
}

/* An enumerator's elements, in an array in `r` (which may be `e`'s own item: `e` is copied first). */
static void gather(xb_ctx *ctx, xb_item *r, const xb_item *e) {
  xb_item from = *e, element;
  xb_set_nil(&element);
  xb_gc_root_push(ctx, &from);
  xb_gc_root_push(ctx, &element);
  xb_set_array(r, xb_array_alloc(0));
  while (move_next(ctx, &from) && current(ctx, &element, &from)) {
    xb_array *a = r->v.p;
    xb_array_resize(a, a->len + 1);
    a->items[a->len - 1] = element;
    xb_gc_write_item(a, a->len - 1);
  }
  xb_gc_root_pop(ctx, 2);
}

/* An object goes through its enumerator: GetEnumerator(), or the object itself when it is one (MoveNext; Reset()
 * first, when it has one); for DESCEND, the enumerator's elements in an array, gone through backwards. */
void xb_each_source(xb_ctx *ctx, xb_item *r, const xb_item *coll, uint32_t descend) {
  *r = *coll;
  if (coll->type != XB_T_OBJECT) return;
  if (xb_is_generator(coll)) {
    if (descend) gather(ctx, r, coll);
    return;
  }
  if (answers(coll, "GETENUMERATOR", 0)) {
    if (!send0(ctx, r, coll, "GETENUMERATOR")) return;
  } else if (answers(coll, "MOVENEXT", 0) && answers(coll, "CURRENT", 1) && answers(coll, "RESET", 0)) {
    xb_item ignored;
    xb_set_nil(&ignored);
    xb_gc_root_push(ctx, &ignored);
    int ok = send0(ctx, &ignored, coll, "RESET");
    xb_gc_root_pop(ctx, 1);
    if (!ok) return;
  }
  if (!xb_is_generator(r) && (!answers(r, "MOVENEXT", 0) || !answers(r, "CURRENT", 1))) {
    xb_error(ctx, NULL, EG_ARG, 1068, "array access", 0, 1, coll); /* no collection, as for any other value */
    return;
  }
  if (descend) gather(ctx, r, r);
}

/* An enumerator's length so far, or the error of a value that is no collection. */
XB_NOINLINE static int64_t enum_len_other(xb_ctx *ctx, const xb_item *coll) {
  if (answers(coll, "MOVENEXT", 0)) return move_next(ctx, coll) ? INT64_MAX : 0;
  xb_error(ctx, NULL, EG_ARG, 1068, "array access", 0, 1, coll);
  return -1;
}

/* FOR EACH iterates arrays (their items), strings (one-character strings),
 * hashes (their values) and enumerators (each MoveNext() that is .T. one more
 * element: a length as large as can be, else 0); indexes are 1-based. */
int64_t xb_enum_len(xb_ctx *ctx, const xb_item *coll) {
  if (coll->type == XB_T_ARRAY) return (int64_t)xb_array_length(coll->v.p);
  if (coll->type == XB_T_STRING) return (int64_t)coll->v.s->len;
  if (coll->type == XB_T_HASH) return (int64_t)xb_hash_length(coll->v.p);
  if (xb_is_generator(coll)) return xb_gen_move_next(ctx, coll) ? INT64_MAX : 0;
  return enum_len_other(ctx, coll);
}

/* A variable that holds an element passed to it by reference is shared like
 * a variable in a cell: it moves into one, and the loop works inside it. */
uint32_t xb_each_shared(xb_ctx *ctx, xb_item *var, uint32_t is_static) {
  if (is_static && xb_storage_is_shared()) { /* shared storage: the loop works through a cell */
    xb_cell_move(ctx, var, is_static);
    return 1;
  }
  if (var->type == XB_T_REF && !xb_is_loop_ref(var)) xb_cell_move(ctx, var, is_static);
  return var->type == XB_T_CELL;
}

/* The item a FOR EACH loop works on (see xb_each_enter in xbrt.h): the variable's, or its cell's (which another
 * thread may reach, through a codeblock that captured the variable). */
static void each_base(const xb_item *var, uint32_t how, xb_item *dst) {
  if (how & XB_EACH_IN_CELL) xb_cell_get(var->v.p, dst);
  else if ((how & XB_EACH_STATIC) && xb_storage_is_shared()) xb_seq_get(var, dst, var); /* a loop begun before */
  else *dst = *var;
}

static void each_set_base(xb_item *var, uint32_t how, const xb_item *v) {
  if (how & XB_EACH_IN_CELL) {
    xb_cell_put(var->v.p, v);
  } else if ((how & XB_EACH_STATIC) && xb_storage_is_shared()) { /* a STATIC's loop begun before threads ran */
    xb_share(v);
    xb_seq_put(var, var, v);
  } else {
    *var = *v;
  }
}

void xb_each_enter(xb_ctx *ctx, xb_item *saved, xb_item *var, uint32_t how, const xb_item *coll) {
  each_base(var, how, saved); /* `coll` and `saved` are frame slots, `var` is reachable: all rooted */
  if (!how) {
    xb_ref_init(var, coll, 0, 1);
    return;
  }
  xb_item ref;
  xb_set_nil(&ref);
  xb_gc_root_push(ctx, &ref);
  xb_ref_init(&ref, coll, 0, 1); /* an allocation: the base is written after it, as it is then */
  each_set_base(var, how, &ref);
  xb_gc_root_pop(ctx, 1);
}

/* An enumerator's Current, into the loop reference's own item. Kept apart, so
 * that the paths of arrays and strings stay without a frame of their own. */
XB_NOINLINE static void move_current(xb_ctx *ctx, xb_ref *ref) {
  xb_item value;
  xb_set_nil(&value);
  xb_gc_root_push(ctx, &value);
  if (current(ctx, &value, &ref->coll)) xb_ref_own_put(ref, &value); /* the loop's: alive, pinned by this frame */
  xb_gc_root_pop(ctx, 1);
}

/* The loop's reference `r` (an item that keeps it) pointed at element `idx`. */
static void move_to(xb_ctx *ctx, const xb_item *r, int64_t idx) {
  xb_ref *ref = r->v.p;
  xb_ref_set_index(ref, idx);
  if (ref->coll.type == XB_T_ARRAY || ref->coll.type == XB_T_HASH) {
    xb_item element; /* not kept: an element gone (an array that shrank under the loop) is the bound error */
    xb_ref_get(ctx, r, &element);
  } else if (ref->coll.type == XB_T_STRING) {
    const xb_str *s = ref->coll.v.s;
    if (idx >= 1 && (uint64_t)idx <= s->len) {
      xb_item ch;
      xb_set_str(&ch, xb_str_new(s->data + idx - 1, 1)); /* one byte: static, nothing allocated */
      xb_ref_own_put(ref, &ch);
    }
  } else {
    move_current(ctx, ref);
  }
}

void xb_each_move(xb_ctx *ctx, xb_item *var, uint32_t how, int64_t idx) {
  if (!how) { /* a frame variable: its item is the loop's reference (taking the variable copies it) */
    if (var->type == XB_T_REF) move_to(ctx, var, idx);
    return;
  }
  /* In a cell or a STATIC, which another thread may reach, the reference may be replaced (two loops over one
   * variable: the program's race): a rooted copy of it, or nothing when the variable holds what the other loop left. */
  xb_item base;
  each_base(var, how, &base);
  if (base.type != XB_T_REF) return;
  xb_gc_root_push(ctx, &base);
  move_to(ctx, &base, idx);
  xb_gc_root_pop(ctx, 1);
}

void xb_each_leave(xb_item *var, uint32_t how, const xb_item *saved) { each_set_base(var, how, saved); }

void xb_each_key(xb_ctx *ctx, xb_item *r, const xb_item *coll, int64_t idx) {
  if (coll->type != XB_T_HASH) {
    xb_set_int(r, idx);
    return;
  }
  if (xb_hash_entry(coll->v.p, idx, r, NULL)) return;
  xb_item i;
  xb_set_int(&i, idx);
  xb_array_bound_error(ctx, coll, &i); /* the hash shrank under the loop */
}

/* SWITCH compares integers and strings only. */
void xb_switch_check(xb_ctx *ctx, xb_item *r, const xb_item *value) {
  if (value->type == XB_T_INT || value->type == XB_T_STRING) *r = *value;
  else xb_arg_error(ctx, r, 3104, "SWITCH", 1, value);
}

int32_t xb_switch_match(const xb_item *value, const xb_item *c) {
  if (value->type != c->type) return 0;
  if (value->type == XB_T_INT) return value->v.i == c->v.i;
  const xb_str *a = value->v.s, *b = c->v.s;
  return a->len == b->len && memcmp(a->data, b->data, a->len) == 0;
}

int32_t xb_args_rest(xb_ctx *ctx, xb_item *dst, xb_item *rest, uint32_t n, uint32_t at, uint32_t refs) {
  for (uint32_t k = 0; k < n; ++k) {
    if (xb_refs_take(refs, at + k) || (rest[k].type != XB_T_CELL && rest[k].type != XB_T_REF)) {
      dst[k] = rest[k];
      continue;
    }
    xb_ref_get(ctx, &rest[k], &dst[k]); /* `dst` is a frame's: rooted there */
    if (xb_pending_get(ctx) && xb_pending(ctx)) return 0;
  }
  return 1;
}

/* AWAIT FOR EACH (AwaitEachStmt, compiler/src/parser/AST.h): __AwaitEach( xSource ) gives what the loop awaits each
 * step of. An asynchronous enumerator is itself (an object of any class that answers MoveNextAsync); an object that
 * answers GetAsyncEnumerator gives that one; else what FOR EACH would go through (xb_each_source: an array, a hash, a
 * string, an enumerator, an object's GetEnumerator(), such as a channel's), itself when it answers MoveNextAsync (a
 * channel's enumerator), else walked by an AwaitEach object, whose MoveNextAsync takes the next element at once (a task
 * completed with .T., .F. at the end) into its Current, which the loop awaits (__AwaitEachAwaits: an element that is a
 * task gives its result). */
enum { AE_CURRENT, AE_SOURCE, AE_POSITION, AE_KEY, AE_COUNT_ };
static const xb_class_var await_each_vars[] = {
  {"CURRENT", XB_VAR_READ}, {"(SOURCE)", XB_VAR_ANY}, {"(POSITION)", XB_VAR_ANY}, {"(KEY)", XB_VAR_ANY}};
_Static_assert(XB_TABLE_SIZE(await_each_vars) == AE_COUNT_, "one AE_ index per AwaitEach variable");

/* oAwaitEach:MoveNextAsync(): the source's next element into Current, now: a task completed with whether there was
 * one (an element gone from an array that shrank meanwhile ends the walk). */
static void await_each_move_next_async(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  xb_object *o = argv[0].v.p;
  xb_item src, element, key, flag;
  xb_object_get(o, AE_SOURCE, &src);
  xb_object_get(o, AE_POSITION, &element);
  int64_t pos = element.v.i + 1;
  xb_set_nil(&element);
  xb_set_int(&key, pos); /* the position, but a hash's key */
  xb_gc_root_push(ctx, &src);
  xb_gc_root_push(ctx, &element);
  xb_gc_root_push(ctx, &key);
  int got;
  if (src.type == XB_T_ARRAY) {
    got = (uint64_t)pos <= xb_array_length(src.v.p) && xb_array_get(src.v.p, (size_t)pos - 1, &element);
  } else if (src.type == XB_T_HASH) {
    got = xb_hash_entry(src.v.p, pos, &key, &element);
  } else if (src.type == XB_T_STRING) {
    got = (uint64_t)pos <= src.v.s->len;
    if (got) xb_set_str(&element, xb_str_new(src.v.s->data + pos - 1, 1)); /* one byte: static */
  } else {
    got = move_next(ctx, &src) && current(ctx, &element, &src);
  }
  if (!(xb_pending_get(ctx) && xb_pending(ctx))) {
    xb_item at;
    xb_set_int(&at, pos);
    xb_object_put(argv[0].v.p, AE_POSITION, &at);
    if (got) xb_object_put(argv[0].v.p, AE_CURRENT, &element);
    if (got) xb_object_put(argv[0].v.p, AE_KEY, &key);
    xb_set_logical(&flag, got);
    xb_task_completed(ctx, r, &flag);
  }
  xb_gc_root_pop(ctx, 3);
}

static const xb_class_method await_each_methods[] = {{"MOVENEXTASYNC", await_each_move_next_async}};
static const xb_class await_each_class = {"AWAITEACH", AE_COUNT_, await_each_vars, NULL,
                                          XB_TABLE_SIZE(await_each_methods), await_each_methods};

void xbf___AWAITEACH(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_item src;
  xb_set_nil(&src);
  if (!argc) {
    xb_error(ctx, NULL, EG_ARG, 1068, "array access", 0, 0, NULL);
    return;
  }
  if (answers(&argv[0], "MOVENEXTASYNC", 0)) {
    *ret = argv[0];
    return;
  }
  xb_gc_root_push(ctx, &src);
  if (answers(&argv[0], "GETASYNCENUMERATOR", 0)) {
    if (send0(ctx, &src, &argv[0], "GETASYNCENUMERATOR")) *ret = src;
    xb_gc_root_pop(ctx, 1);
    return;
  }
  if (argv[0].type != XB_T_ARRAY && argv[0].type != XB_T_HASH && argv[0].type != XB_T_STRING &&
      !is_collection_object(&argv[0])) {
    xb_gc_root_pop(ctx, 1);
    xb_error(ctx, NULL, EG_ARG, 1068, "array access", 0, 1, &argv[0]); /* no collection, as for FOR EACH */
    return;
  }
  xb_each_source(ctx, &src, &argv[0], 0);
  if (!(xb_pending_get(ctx) && xb_pending(ctx))) {
    if (answers(&src, "MOVENEXTASYNC", 0)) {
      *ret = src;
    } else {
      xb_object *o = xb_object_new(&await_each_class); /* `src` is rooted */
      o->vars[AE_SOURCE] = src;
      xb_set_int(&o->vars[AE_POSITION], 0);
      xb_gc_write(o);
      xb_set_object(ret, o);
    }
  }
  xb_gc_root_pop(ctx, 1);
}

/* __AwaitEachKey( oEnumerator, nPosition ): the k of AWAIT FOR EACH k => v: the key of a hash's entry (an AwaitEach
 * object's over a hash), else the position, as FOR EACH's k (xb_each_key). */
void xbf___AWAITEACHKEY(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  if (argc > 1) *ret = argv[1];
  if (argc && argv[0].type == XB_T_OBJECT && ((const xb_object *)argv[0].v.p)->cls == &await_each_class)
    xb_object_get(argv[0].v.p, AE_KEY, ret);
}

/* __AwaitEachAwaits( oEnumerator ): whether AWAIT FOR EACH awaits its Current (an AwaitEach object's). */
void xbf___AWAITEACHAWAITS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  xb_set_logical(ret, argc && argv[0].type == XB_T_OBJECT &&
                          ((const xb_object *)argv[0].v.p)->cls == &await_each_class);
}

/* __Dispose( xValue ): the end of a USING statement: xValue:Dispose(), unless xValue is NIL. */
void xbf___DISPOSE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (!argc || argv[0].type == XB_T_NIL) return;
  xb_op_send(ctx, ret, "DISPOSE", 1, argv);
}
