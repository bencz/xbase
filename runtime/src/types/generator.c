/* Generators: the enumerator a function with YIELD returns (compiler/src/xir/Gen.cpp, genGenerator). It holds the
 * function's resume function, the state to resume in, the resume function's frame between steps, and the element
 * the last step yielded (its Current). MoveNext runs one step: the resume function takes its frame back
 * (xb_gen_restore), goes on after the YIELD of the state, and either yields again (xb_gen_yield: the frame and the
 * element saved, .T.) or ends (xb_gen_done, .F.). Its other messages are the Enumerable class's
 * (runtime/src/rtl/collections.prg). */
#include "xbrt_int.h"

#include "core/shared.h"

#include <string.h>

/* The variables: CURRENT answers the message; the others, whose names are no message, are the generator's own. */
enum { GEN_CURRENT = XB_GEN_CURRENT, GEN_STATE, GEN_FRAME, GEN_RESUME, GEN_COUNT_ };
static const xb_class_var gen_vars[] = {
  {"CURRENT", XB_VAR_READ}, {"(STATE)", XB_VAR_ANY}, {"(FRAME)", XB_VAR_ANY}, {"(RESUME)", XB_VAR_ANY},
};
const xb_class xb_generator_class = {"GENERATOR", GEN_COUNT_, gen_vars, NULL};

/* The state of an enumerator that has ended (or is running a step: a step that fails ends it). A step of a shared
 * enumerator is claimed first, under its lock: its state `s` becomes -(s + 2), which only that step's xb_gen_restore
 * reads back, so a MoveNext from another thread meanwhile finds no state to claim and answers .F. A local enumerator
 * is reached by this thread alone: its step's xb_gen_restore makes it ended until the step yields (a MoveNext inside
 * the step, or from a thread it was shared with meanwhile, answers .F. too). One thread at a time touches the
 * frame. */
#define GEN_ENDED (-1)
#define GEN_CLAIMED(s) (-(s) - 2)

void xb_gen_new(xb_ctx *ctx, xb_item *r, xb_func resume, uint32_t argc, const xb_item *argv) {
  XB_UNUSED(ctx);
  xb_object *g = xb_object_new(&xb_generator_class);
  xb_set_object(r, g); /* rooted there from now on */
  xb_set_int(&g->vars[GEN_STATE], 0);
  xb_set_int(&g->vars[GEN_RESUME], (int64_t)(uintptr_t)resume);
  xb_array *frame = xb_array_alloc((uint64_t)argc + 1); /* slot 0, the enumerator itself, is the resume's argument */
  xb_set_array(&g->vars[GEN_FRAME], frame);
  xb_gc_write(g);
  if (argc) memcpy(frame->items + 1, argv, argc * sizeof *argv);
  xb_gc_write_items(frame, 1, (size_t)argc + 1);
}

int32_t xb_gen_restore(xb_ctx *ctx, const xb_item *gen, xb_item *slots, uint32_t nslots) {
  XB_UNUSED(ctx);
  xb_object *g = gen->v.p;
  const xb_array *frame = g->vars[GEN_FRAME].v.p;
  uint32_t n = nslots < frame->len ? nslots : (uint32_t)frame->len;
  if (n > 1) memcpy(slots + 1, frame->items + 1, (n - 1) * sizeof *slots);
  if (!xb_is_shared(g)) { /* ended until the step yields; one that fails ends it (xb_gen_move_next) */
    int32_t state = (int32_t)g->vars[GEN_STATE].v.i; /* never claimed: only shared ones are, and they stay shared */
    xb_set_int(&g->vars[GEN_STATE], GEN_ENDED);
    return state;
  }
  xb_item s; /* claimed until the step yields */
  xb_object_get(g, GEN_STATE, &s);
  return s.v.i <= -2 ? (int32_t)GEN_CLAIMED(s.v.i) : (int32_t)s.v.i;
}

/* Whether a step may run: a local enumerator's state is not ended; a shared one's step is claimed (0 when it has ended
 * or another step runs). */
static int claim(xb_object *g) {
  if (!xb_is_shared(g)) return g->vars[GEN_STATE].v.i >= 0;
  xb_seq_lock(g);
  int64_t s = g->vars[GEN_STATE].v.i;
  if (s >= 0) {
    xb_item claimed;
    xb_set_int(&claimed, GEN_CLAIMED(s));
    xb_seq_store(&g->vars[GEN_STATE], &claimed);
  }
  xb_seq_unlock(g);
  return s >= 0;
}

void xb_gen_yield(xb_ctx *ctx, const xb_item *gen, const xb_item *slots, uint32_t nslots, uint32_t state,
                  const xb_item *value) {
  xb_object *g = gen->v.p;
  xb_array *frame = g->vars[GEN_FRAME].v.p;
  if (!xb_is_shared(g)) { /* its frame too (only it reaches the frame): in place, as items built in locals and then
                            * copied would stall the copies' loads */
    if (frame->len < nslots) xb_array_resize(frame, nslots); /* the enumerator holds it: reachable */
    memcpy(frame->items + 1, slots + 1, (nslots - 1) * sizeof *slots);
    xb_gc_write_items(frame, 1, nslots);
    g->vars[GEN_CURRENT] = *value;
    xb_set_int(&g->vars[GEN_STATE], state);
    xb_gc_write(g);
    return;
  }
  if (frame->len < nslots) xb_array_resize_any(ctx, frame, nslots);
  for (uint32_t k = 1; k < nslots; ++k) xb_array_put(frame, k, &slots[k]); /* the slots' values shared first */
  xb_object_put(g, GEN_CURRENT, value);
  xb_item s;
  xb_set_int(&s, state);
  xb_object_put(g, GEN_STATE, &s);
}

void xb_gen_done(xb_ctx *ctx, const xb_item *gen) {
  xb_object *g = gen->v.p;
  if (!xb_is_shared(g)) {
    xb_set_nil(&g->vars[GEN_CURRENT]);
    xb_set_int(&g->vars[GEN_STATE], GEN_ENDED);
  } else {
    xb_item v;
    xb_set_nil(&v);
    xb_object_put(g, GEN_CURRENT, &v);
    xb_set_int(&v, GEN_ENDED);
    xb_object_put(g, GEN_STATE, &v);
  }
  xb_array_resize_any(ctx, g->vars[GEN_FRAME].v.p, 0); /* the frame's values can go */
}

int xb_gen_move_next(xb_ctx *ctx, const xb_item *gen) {
  xb_object *g = gen->v.p;
  if (!claim(g)) return 0;
  xb_func resume = (xb_func)(uintptr_t)g->vars[GEN_RESUME].v.i;
  xb_item self = *gen, ret; /* the enumerator is the caller's (and slot 0 of the step's frame); the result a logical */
  xb_set_nil(&ret);
  resume(ctx, &ret, 1, &self);
  if (xb_pending_get(ctx) & XB_PENDING_UNWIND) { /* a step that fails ends it, as the end of its body does */
    xb_gen_done(ctx, gen);
    return 0;
  }
  return ret.type == XB_T_LOGICAL && ret.v.l;
}

/* The runtime's Enumerable() (runtime/src/rtl/collections.prg): a program's class of that name is another. */
void xbf_ENUMERABLE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
static xb_class_def *enumerable;

int xb_gen_message(xb_ctx *ctx, xb_item *r, const char *message, uint32_t argc, xb_item *argv) {
  if (strcmp(message, "MOVENEXT") == 0) {
    xb_set_logical(r, xb_gen_move_next(ctx, &argv[0]));
    return 1;
  }
  if (strcmp(message, "CLASSNAME") == 0) return 0; /* its own: GENERATOR */
  return xb_class_send_as(ctx, r, xbf_ENUMERABLE, &enumerable, message, argc, argv); /* Select, Where, ToArray ... */
}
