/* Asynchronous generators: an ASYNC function with YIELD. Calling it runs nothing and returns its enumerator, an object
 * of the runtime class ASYNCGENERATOR (xb_agen_new). Its MoveNextAsync( [oToken] ) gives a task of whether an element
 * came, which is then in Current: the first one starts the body on the caller's thread (xb_async_gen_start, as an
 * asynchronous function's first step), each later one resumes it (xb_task_resume: at once on the caller's thread when
 * the stack has room); a YIELD takes the element, parks the body (its task waits for nothing) and makes the task .T.;
 * an AWAIT suspends it as in any asynchronous function, the task then waiting; the body's end makes it .F., an error
 * fails it with the error, and the enumeration is over (.F. from then on). The token cancels the wait and ends the
 * enumeration: what the body yields after it is dropped. One MoveNextAsync() at a time: another while one waits raises
 * BASE/9035, as a channel's enumerator does. MoveNext() waits for the element (FOR EACH). */
#include "xbrt_int.h"

#include "rtl/flows.h"

#include <stdlib.h>

/* The variables: the element (read only), the state word (AG_*, with AG_TOKEN while the running MoveNextAsync() has a
 * token), the resume function (its address), the arguments until the body starts, the task of the MoveNextAsync()
 * that waits (NIL while its step runs on its caller's thread without a token: made only if the step suspends), the
 * body's task once it parked at a YIELD (the same task to the end), and the element a YIELD gave when a token may
 * cancel its MoveNextAsync(), until the step's end makes it Current (dropped when it was canceled; with no token the
 * YIELD makes it Current itself). Each is written only when it changes: once shared, a write takes a stripe's lock. A
 * MoveNextAsync() takes the state from AG_IDLE or AG_YIELDED to AG_RUNNING (one compare-and-swap) and then writes the
 * waiting task; the body reads it at the end of its step, which that MoveNextAsync() started or resumed: published to
 * it either way. */
enum { G_CURRENT, G_STATE, G_RESUME, G_ARGS, G_PENDING, G_TASK, G_YIELDED, G_COUNT_ };
enum { AG_IDLE, AG_RUNNING, AG_YIELDED, AG_DONE, AG_STATES = 0xff, AG_TOKEN = 0x100 };
static const xb_class_var agen_vars[] = {{"CURRENT", XB_VAR_READ}, {"(STATE)", XB_VAR_ANY},   {"(RESUME)", XB_VAR_ANY},
                                         {"(ARGS)", XB_VAR_ANY},   {"(PENDING)", XB_VAR_ANY}, {"(TASK)", XB_VAR_ANY},
                                         {"(YIELDED)", XB_VAR_ANY}};
_Static_assert(XB_TABLE_SIZE(agen_vars) == G_COUNT_, "one G_ index per AsyncGenerator variable");

static const xb_class agen_class;

/* The operation of the tasks MoveNextAsync() makes and of its errors. */
static const char move_next_async[] = "ASYNCGENERATOR:MOVENEXTASYNC";

/* The MoveNextAsync() whose step runs on this thread without a waiting task: its enumerator (its caller's item, a root:
 * the object may move meanwhile) and where its task goes, once the step tells how it ended (made then: done already,
 * unless the step suspended). Nested: a body's step may call another generator's MoveNextAsync(). */
typedef struct here_request {
  const xb_item *gen;
  xb_item *task;
} here_request;
static XB_THREAD_LOCAL here_request *here;

void xb_agen_new(xb_ctx *ctx, xb_item *r, xb_func resume, uint32_t argc, const xb_item *argv) {
  XB_UNUSED(ctx);
  xb_object *g = xb_object_new(&agen_class);
  xb_set_object(r, g);
  xb_set_logical(&g->vars[G_STATE], AG_IDLE); /* the word read and changed atomically (its v.l) */
  xb_set_int(&g->vars[G_RESUME], (int64_t)(uintptr_t)resume);
  xb_array *a = xb_array_alloc(argc); /* a safepoint: `*r` (the caller's) keeps the enumerator, its frame `argv` */
  for (uint32_t i = 0; i < argc; ++i) xb_array_items(a)[i] = argv[i];
  g = r->v.p;
  xb_set_array(&g->vars[G_ARGS], a);
  xb_gc_write(g);
}

/* The body's first step, with the arguments the enumerator kept, copied where nothing moves them while it runs. */
static void start(xb_ctx *ctx, const xb_item *gen) {
  xb_item args, resume, nil;
  xb_set_nil(&nil);
  xb_object_get(gen->v.p, G_ARGS, &args);
  xb_object_get(gen->v.p, G_RESUME, &resume);
  xb_object_put(gen->v.p, G_ARGS, &nil);
  uint32_t argc = args.type == XB_T_ARRAY ? (uint32_t)xb_array_length(args.v.p) : 0;
  xb_item *argv = argc ? malloc(argc * sizeof *argv) : NULL;
  if (argc && !argv) xb_gc_fatal("out of memory");
  for (uint32_t i = 0; i < argc; ++i) {
    xb_array_get(args.v.p, i, &argv[i]);
    xb_gc_root_push(ctx, &argv[i]);
  }
  xb_async_gen_start(ctx, gen, (xb_func)(uintptr_t)resume.v.i, argc, argv);
  xb_gc_root_pop(ctx, argc);
  free(argv);
}

/* oGenerator:MoveNextAsync( [oToken] ) (see above). */
static void agen_move_next_async(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  xb_item token, task, nil, no;
  xb_set_nil(r);
  xb_set_nil(&nil);
  xb_set_logical(&no, 0);
  if (argc > 1) token = argv[1];
  else xb_set_nil(&token);
  if (!xb_task_token_ok(&token)) {
    xb_flow_arg_error(ctx, XB_CODE_TASK, move_next_async, argc, argv);
    return;
  }
  xb_set_nil(&task);
  xb_gc_root_push(ctx, &token);
  xb_gc_root_push(ctx, &task);
  uint32_t s = xb_atomic_load32(xb_object_word(&argv[0], G_STATE), XB_ACQUIRE);
  for (;;) {
    if ((s & AG_STATES) == AG_RUNNING) {
      xb_object_get(argv[0].v.p, G_PENDING, &task); /* its token canceled it: the enumeration is over */
      if (task.type != XB_T_NIL && xb_task_is_final(&task)) {
        xb_task_completed(ctx, r, &no);
        break;
      }
      uint32_t now = xb_atomic_load32(xb_object_word(&argv[0], G_STATE), XB_ACQUIRE);
      if ((now & AG_STATES) != AG_RUNNING) { /* the step ended meanwhile (its task taken, then the state stored) */
        s = now;
        continue;
      }
      xb_error_described(ctx, NULL, EG_ARG, XB_CODE_ENUMERATOR_WAITING, "Enumerator waiting already", move_next_async,
                         0, argc, argv);
      break;
    }
    if (s == AG_DONE) {
      xb_task_completed(ctx, r, &no);
      break;
    }
    /* A task made before the step: one a token cancels, or one the step may not run here to tell (a stack too short
     * to resume it on); the address of the state again after that allocation. */
    int room = xb_stack_room(ctx); /* decided once: the step runs here, or on a worker with its task made first */
    int ahead = token.type != XB_T_NIL || (s == AG_YIELDED && !room);
    if (ahead && r->type == XB_T_NIL && !xb_task_promise(ctx, r, &token, move_next_async, argc, argv)) break;
    uint32_t running = AG_RUNNING | (token.type != XB_T_NIL ? AG_TOKEN : 0u);
    if (!xb_atomic_cas32(xb_object_word(&argv[0], G_STATE), &s, running, XB_ACQ_REL)) continue;
    if (r->type != XB_T_NIL) xb_object_put(argv[0].v.p, G_PENDING, r); /* NIL there since the last step's end */
    here_request request = {&argv[0], r}, *outer = here;
    if (!ahead) here = &request;
    xb_await_fused(ctx); /* an AWAIT of this MoveNextAsync() gets its task: the body's calls are no AWAIT's */
    if (s == AG_IDLE) {
      start(ctx, &argv[0]);
    } else {
      xb_object_get(argv[0].v.p, G_TASK, &task);
      xb_task_resume(ctx, &task, room);
    }
    here = outer;
    break;
  }
  xb_gc_root_pop(ctx, 2);
}

/* oGenerator:MoveNext(): waits for MoveNextAsync()'s outcome (a failure raised as Result raises it). */
static void agen_move_next(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  xb_item task;
  xb_set_nil(&task);
  xb_gc_root_push(ctx, &task);
  agen_move_next_async(ctx, &task, 1, argv);
  if (!(xb_pending_get(ctx) & XB_PENDING_UNWIND) && task.type != XB_T_NIL) xb_await_wait(ctx, r, &task);
  xb_gc_root_pop(ctx, 1);
}

void xb_agen_yield(xb_ctx *ctx, const xb_item *gen, const xb_item *value, const xb_item *task) {
  XB_UNUSED(ctx);
  xb_item kept;
  int tokened = (xb_atomic_load32(xb_object_word(gen, G_STATE), XB_ACQUIRE) & AG_TOKEN) != 0;
  xb_object_put(gen->v.p, tokened ? G_YIELDED : G_CURRENT, value);
  xb_object_get(gen->v.p, G_TASK, &kept);
  if (kept.type == XB_T_NIL) xb_object_put(gen->v.p, G_TASK, task);
}

/* The request of the MoveNextAsync() running `gen`'s step on this thread without a task, or NULL. */
static here_request *request_of(const xb_item *gen) { return here && here->gen->v.p == gen->v.p ? here : NULL; }

void xb_agen_step_awaits(xb_ctx *ctx, const xb_item *gen) {
  xb_item pending, nil;
  xb_set_nil(&nil);
  xb_object_get(gen->v.p, G_PENDING, &pending);
  here_request *request = request_of(gen);
  if (pending.type != XB_T_NIL || !request) return;
  xb_task_promise(ctx, request->task, &nil, move_next_async, 0, NULL); /* no token: it cannot fail */
  xb_object_put(gen->v.p, G_PENDING, request->task);
}

void xb_agen_step_ended(xb_ctx *ctx, const xb_item *gen, int outcome, const xb_item *failure) {
  if (outcome == XB_AGEN_AWAITING) return; /* the task of MoveNextAsync() still waits */
  xb_item pending, nil, flag;
  xb_set_nil(&nil);
  xb_object_get(gen->v.p, G_PENDING, &pending);
  xb_gc_root_push(ctx, &pending);
  if (pending.type != XB_T_NIL) xb_object_put(gen->v.p, G_PENDING, &nil);
  here_request *request = pending.type == XB_T_NIL ? request_of(gen) : NULL; /* its step ran here, without a task */
  int mine = request || (pending.type != XB_T_NIL && xb_task_claim(&pending)); /* else canceled by its token */
  int tokened = (xb_atomic_load32(xb_object_word(gen, G_STATE), XB_ACQUIRE) & AG_TOKEN) != 0;
  uint32_t state = outcome == XB_AGEN_YIELDED && mine ? AG_YIELDED : AG_DONE;
  if (state == AG_YIELDED && tokened) { /* the element taken: Current */
    xb_item element;
    xb_object_get(gen->v.p, G_YIELDED, &element);
    xb_object_put(gen->v.p, G_CURRENT, &element);
  } else if (state == AG_DONE) { /* the body parked for good, or ended: let it go */
    xb_object_put(gen->v.p, G_TASK, &nil);
  }
  if (tokened) xb_object_put(gen->v.p, G_YIELDED, &nil);
  xb_atomic_store32(xb_object_word(gen, G_STATE), state, XB_RELEASE); /* before the task: its waiter may go on */
  if (request) { /* the task made done, for the MoveNextAsync() that returns it */
    if (outcome == XB_AGEN_YIELDED || outcome == XB_AGEN_ENDED) {
      xb_set_logical(&flag, outcome == XB_AGEN_YIELDED);
      xb_task_completed(ctx, request->task, &flag);
    } else {
      xb_task_promise(ctx, request->task, &nil, move_next_async, 0, NULL);
      if (outcome == XB_AGEN_FAILED) {
        xb_task_settle(ctx, request->task, 0, failure);
      } else if (xb_task_claim(request->task)) {
        xb_task_cancel_claimed(ctx, request->task);
      }
    }
  } else if (mine) {
    switch (outcome) {
    case XB_AGEN_YIELDED:
    case XB_AGEN_ENDED:
      xb_set_logical(&flag, outcome == XB_AGEN_YIELDED);
      xb_task_settle_claimed(ctx, &pending, 1, &flag);
      break;
    case XB_AGEN_FAILED: xb_task_settle_claimed(ctx, &pending, 0, failure); break;
    default: xb_task_cancel_claimed(ctx, &pending); break;
    }
  }
  xb_gc_root_pop(ctx, 1);
}

static const xb_class_method agen_methods[] = {{"MOVENEXT", agen_move_next}, {"MOVENEXTASYNC", agen_move_next_async}};
static const xb_class agen_class = {"ASYNCGENERATOR", G_COUNT_, agen_vars, NULL, XB_TABLE_SIZE(agen_methods),
                                    agen_methods, NULL};
