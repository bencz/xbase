/* Generators: the enumerator a function with YIELD returns (compiler/src/xir/Gen.cpp, genGenerator). It holds the
 * function's resume function, the state to resume in, the resume function's frame between steps, and the element
 * the last step yielded (its Current). MoveNext runs one step: the resume function takes its frame back
 * (xb_gen_restore), goes on after the YIELD of the state, and either yields again (xb_gen_yield: the frame and the
 * element saved, .T.) or ends (xb_gen_done, .F.). Its other messages are the Enumerable class's
 * (runtime/src/rtl/collections.prg). */
#include "xbrt_int.h"

#include "core/shared.h"

#include <string.h>

/* The variables (XB_GEN_*, xbrt.h): CURRENT answers the message; the others, whose names are no message, are the
 * generator's own. */
static const xb_class_var gen_vars[] = {
  {"CURRENT", XB_VAR_READ}, {"(STATE)", XB_VAR_ANY}, {"(FRAME)", XB_VAR_ANY}, {"(RESUME)", XB_VAR_ANY},
};
_Static_assert(XB_TABLE_SIZE(gen_vars) == XB_GEN_VARS, "one XB_GEN_ index per generator variable");

/* The state of an enumerator that has ended (or is running a step: a step that fails ends it). A step of a shared
 * enumerator is claimed first, under its lock: its state `s` becomes -(s + 2), which only that step's xb_gen_restore
 * reads back, so a MoveNext from another thread meanwhile finds no state to claim and answers .F. A local enumerator
 * is reached by this thread alone: its step's xb_gen_restore makes it ended until the step yields (a MoveNext inside
 * the step, or from a thread it was shared with meanwhile, answers .F. too). One thread at a time touches the
 * frame. */
#define GEN_CLAIMED(s) (-(s) - 2)

void xb_gen_new(xb_ctx *ctx, xb_item *r, xb_func resume, uint32_t argc, const xb_item *argv) {
  XB_UNUSED(ctx);
  xb_object *g = xb_object_new(&xb_generator_class);
  xb_set_object(r, g); /* rooted there from now on */
  xb_set_int(&g->vars[XB_GEN_STATE], 0);
  xb_set_int(&g->vars[XB_GEN_RESUME], (int64_t)(uintptr_t)resume);
  xb_array *frame = xb_array_alloc((uint64_t)argc + 1); /* slot 0, the enumerator itself, is the resume's argument */
  xb_set_array(&g->vars[XB_GEN_FRAME], frame);
  xb_gc_write(g);
  if (argc) memcpy(frame->items + 1, argv, argc * sizeof *argv);
  xb_gc_write_items(frame, 1, (size_t)argc + 1);
}

void xb_frame_store_slow(xb_ctx *ctx, const xb_item *holder, uint32_t k, const xb_item *slots, uint32_t nslots) {
  xb_object *o = holder->v.p;
  if (o->vars[k].type != XB_T_ARRAY) { /* the first save: an allocation, rooted in the holder (a safepoint) */
    xb_array *made = xb_array_alloc(nslots);
    o = holder->v.p;
    xb_item frame;
    xb_set_array(&frame, made);
    if (xb_is_shared(o)) xb_object_put(o, k, &frame);
    else {
      o->vars[k] = frame;
      xb_gc_write(o);
    }
  }
  xb_array *frame = o->vars[k].v.p;
  /* Grown first: an allocation, a safepoint where the program's storage may become shared (publication,
   * docs/concurrency.md), and the holder with it. */
  if (frame->len < nslots) xb_array_resize_any(ctx, frame, nslots);
  if (!xb_is_shared(o)) { /* its frame too (only it reaches the frame): in place, as items built in locals and then
                            * copied would stall the copies' loads */
    xb_frame_copy(frame->items, slots, nslots);
    xb_gc_write_items(frame, 1, nslots);
    return;
  }
  for (uint32_t s = 1; s < nslots; ++s) xb_share(&slots[s]); /* before another thread can reach them */
  xb_seq_lock(frame);
  for (uint32_t s = 1; s < nslots; ++s) xb_seq_store(&frame->items[s], &slots[s]);
  xb_gc_write_items(frame, 1, nslots);
  xb_seq_unlock(frame);
}

int32_t xb_gen_restore(xb_ctx *ctx, const xb_item *gen, xb_item *slots, uint32_t nslots) {
  XB_UNUSED(ctx);
  xb_object *g = gen->v.p;
  xb_frame_load(g->vars[XB_GEN_FRAME].v.p, slots, nslots);
  if (!xb_is_shared(g)) { /* ended until the step yields; one that fails ends it (xb_gen_move_next) */
    int32_t state = (int32_t)g->vars[XB_GEN_STATE].v.i; /* never claimed: only shared ones are, and they stay shared */
    xb_set_int(&g->vars[XB_GEN_STATE], XB_GEN_ENDED);
    return state;
  }
  xb_item s; /* claimed until the step yields */
  xb_object_get(g, XB_GEN_STATE, &s);
  return s.v.i <= -2 ? (int32_t)GEN_CLAIMED(s.v.i) : (int32_t)s.v.i;
}

/* Whether a step may run: a local enumerator's state is not ended; a shared one's step is claimed (0 when it has ended
 * or another step runs). */
static int claim(xb_object *g) {
  if (!xb_is_shared(g)) return g->vars[XB_GEN_STATE].v.i >= 0;
  xb_seq_lock(g);
  int64_t s = g->vars[XB_GEN_STATE].v.i;
  if (s >= 0) {
    xb_item claimed;
    xb_set_int(&claimed, GEN_CLAIMED(s));
    xb_seq_store(&g->vars[XB_GEN_STATE], &claimed);
  }
  xb_seq_unlock(g);
  return s >= 0;
}

void xb_gen_yield(xb_ctx *ctx, const xb_item *gen, const xb_item *slots, uint32_t nslots, uint32_t state,
                  const xb_item *value) {
  xb_frame_store(ctx, gen, XB_GEN_FRAME, slots, nslots);
  xb_object *g = gen->v.p;
  if (!xb_is_shared(g)) {
    g->vars[XB_GEN_CURRENT] = *value;
    xb_set_int(&g->vars[XB_GEN_STATE], state);
    xb_gc_write(g);
    return;
  }
  xb_object_put(g, XB_GEN_CURRENT, value);
  xb_item s;
  xb_set_int(&s, state);
  xb_object_put(g, XB_GEN_STATE, &s);
}

void xb_gen_done(xb_ctx *ctx, const xb_item *gen) {
  xb_object *g = gen->v.p;
  if (!xb_is_shared(g)) {
    xb_set_nil(&g->vars[XB_GEN_CURRENT]);
    xb_set_int(&g->vars[XB_GEN_STATE], XB_GEN_ENDED);
  } else {
    xb_item v;
    xb_set_nil(&v);
    xb_object_put(g, XB_GEN_CURRENT, &v);
    xb_set_int(&v, XB_GEN_ENDED);
    xb_object_put(g, XB_GEN_STATE, &v);
  }
  xb_array_resize_any(ctx, g->vars[XB_GEN_FRAME].v.p, 0); /* the frame's values can go */
}

int xb_gen_move_next(xb_ctx *ctx, const xb_item *gen) {
  xb_object *g = gen->v.p;
  if (!claim(g)) return 0;
  xb_func resume = (xb_func)(uintptr_t)g->vars[XB_GEN_RESUME].v.i;
  xb_item self = *gen, ret; /* the enumerator is the caller's (and slot 0 of the step's frame); the result a logical */
  xb_set_nil(&ret);
  resume(ctx, &ret, 1, &self);
  if (xb_pending_get(ctx) & XB_PENDING_UNWIND) { /* a step that fails ends it, as the end of its body does */
    xb_gen_done(ctx, gen);
    return 0;
  }
  return ret.type == XB_T_LOGICAL && ret.v.l;
}

static void gen_move_next(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  xb_set_logical(r, xb_gen_move_next(ctx, &argv[0]));
}

static const xb_class_method gen_methods[] = {{"MOVENEXT", gen_move_next}};
const xb_class xb_generator_class = {"GENERATOR", XB_GEN_VARS, gen_vars, NULL, XB_TABLE_SIZE(gen_methods), gen_methods};

/* The runtime's Enumerable() (runtime/src/rtl/collections.prg): a program's class of that name is another. */
void xbf_ENUMERABLE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
static xb_class_def *enumerable;

int xb_gen_message(xb_ctx *ctx, xb_item *r, const char *message, uint32_t argc, xb_item *argv) {
  if (strcmp(message, "CLASSNAME") == 0) return 0; /* its own: GENERATOR */
  return xb_class_send_as(ctx, r, xbf_ENUMERABLE, &enumerable, message, argc, argv); /* Select, Where, ToArray ... */
}
