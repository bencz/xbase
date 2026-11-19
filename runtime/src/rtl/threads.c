/* Threads, mutexes, conditions, semaphores and atomic integers of the program (docs/concurrency.md#threads), and what
 * threads and tasks share (rtl/flows.h). */
#include "rtl/flows.h"

#include "core/sched.h"
#include "core/shared.h"
#include "core/sync.h"

#include <stdlib.h>
#include <string.h>

void xbf_EVAL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

/* The looks at a held lock's state before its waiter sleeps (no pause instruction: loads only). */
#define LOCK_SPINS 128u

/* The threads ThreadStart() started that have not ended, which the program's end waits for; `ending` while threads are
 * asked to QUIT (a QUIT, the start procedure returned): a thread that starts then quits at once. */
static uint32_t running;
static uint32_t ending;

/* A thread counted in `running` ended, or did not start: the program's end, which waits for the count to change
 * (xb_threads_end), is woken. */
static void thread_gone(void) {
  xb_atomic_add32(&running, (uint32_t)-1, XB_ACQ_REL);
  xb_park_wake(&running, 1);
}

/* The wait's word is published in the context (waitingOn) before the QUIT is tested: xb_gc_raise_all, which raises
 * the QUIT and then wakes the word it reads there, finds the waiter (both sequentially consistent), and the park tests
 * the QUIT again under its bucket's lock: no wait outlives a QUIT, and none looks at the clock meanwhile. While the
 * program is ending (`ending`, stored before the QUIT reaches the contexts) a context whose QUIT is set aside, or that
 * ends the program itself, waits no more; any other waits for its QUIT, so that it leaves with the request pending
 * (else it would go on as if its wait had ended normally). The object stays where it is while its waiter sleeps (its
 * stack, which points into it, pins it). */
static int wait_until(xb_ctx *ctx, const xb_item *object, uint32_t k, uint32_t value, int64_t deadline) {
  for (;;) {
    const uint32_t *word = xb_object_word(object, k);
    xb_atomic_storeptr((void **)&ctx->waitingOn, (void *)(uintptr_t)word, XB_SEQ_CST);
    if (xb_atomic_load32(word, XB_ACQUIRE) != value) return XB_WAIT_CHANGED;
    if (xb_pending_get(ctx) & XB_PENDING_QUIT) return XB_WAIT_QUIT;
    if (ctx->quitAside && xb_atomic_load32(&ending, XB_ACQUIRE)) return XB_WAIT_QUIT;
    int64_t left = -1;
    if (deadline >= 0) {
      left = deadline - xb_platform_monotonic_ns();
      if (left <= 0) return XB_WAIT_TIMED_OUT;
    }
    xb_park_wait_blocking_stop(word, value, &ctx->pending, XB_PENDING_QUIT, left);
  }
}

int xb_wait_until(xb_ctx *ctx, const xb_item *object, uint32_t k, uint32_t value, int64_t deadline) {
  int r = wait_until(ctx, object, k, value, deadline);
  xb_atomic_storeptr((void **)&ctx->waitingOn, NULL, XB_RELEASE);
  return r;
}

int xb_wait_while(xb_ctx *ctx, const xb_item *object, uint32_t k, uint32_t value) {
  return xb_wait_until(ctx, object, k, value, -1) == XB_WAIT_CHANGED;
}

void xb_flow_arg_error(xb_ctx *ctx, int32_t code, const char *what, uint32_t argc, xb_item *argv) {
  xb_error_described(ctx, NULL, EG_ARG, code, "Argument error", what, 0, argc, argv);
}

int xb_deadline_arg(xb_ctx *ctx, const char *what, int32_t code, uint32_t k, uint32_t argc, xb_item *argv,
                    int64_t *deadline) {
  *deadline = -1;
  if (k >= argc || argv[k].type == XB_T_NIL) return 1;
  if (!xb_is_number(&argv[k])) {
    xb_flow_arg_error(ctx, code, what, argc, argv);
    return 0;
  }
  int64_t ms = xb_num_to_i64(&argv[k]);
  if (ms >= 0) *deadline = xb_deadline_after_ms(ms);
  return 1;
}

int xb_integer_arg(xb_ctx *ctx, const char *what, int32_t code, uint32_t k, uint32_t argc, xb_item *argv, int64_t *n) {
  if (k < argc && xb_is_number(&argv[k])) {
    *n = xb_num_to_i64(&argv[k]);
    return 1;
  }
  xb_flow_arg_error(ctx, code, what, argc, argv);
  return 0;
}

void xb_quit_all(const xb_ctx *ctx) {
  xb_atomic_store32(&ending, 1, XB_SEQ_CST);
  xb_gc_raise_all(ctx, XB_PENDING_QUIT);
  xb_sched_stop();
}

void xb_threads_end(xb_ctx *ctx) {
  ++ctx->quitAside; /* its own waits meanwhile end at once: no QUIT comes to it */
  if (xb_atomic_load32(&running, XB_ACQUIRE)) xb_quit_all(ctx);
  /* each thread's end wakes this wait (program_thread_main) */
  for (uint32_t seen; (seen = xb_atomic_load32(&running, XB_ACQUIRE)) != 0;) xb_park_wait_blocking(&running, seen, -1);
  xb_sched_end(ctx); /* the tasks that did not start are canceled */
  xb_atomic_store32(&ending, 0, XB_SEQ_CST); /* none runs (a QUIT ended them too): threads may start again */
  --ctx->quitAside;
}

/* Threads: the result, whether it ended, the block and its arguments (an array, NIL once it ended), the thread's
 * xb_flow_self, and the snapshot of its creator's environment (xb_env_capture, NIL once it ended). */

enum { T_RESULT, T_DONE, T_WORK, T_ID, T_ENV, T_COUNT_ };
static const xb_class_var thread_vars[] = {
  {"(RESULT)", XB_VAR_ANY}, {"(DONE)", XB_VAR_ANY}, {"(WORK)", XB_VAR_ANY}, {"(ID)", XB_VAR_ANY},
  {"(ENV)", XB_VAR_ANY}};
_Static_assert(XB_TABLE_SIZE(thread_vars) == T_COUNT_, "one T_ index per Thread variable");

/* oThread:Join(): waits for the thread to end, and returns what its block returned. */
static void thread_join(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  if (xb_atomic_load64(xb_object_word64(&argv[0], T_ID), XB_RELAXED) == xb_thread_self()) {
    xb_error_described(ctx, r, EG_ARG, XB_CODE_THREAD_JOIN, "Thread joins itself", "THREAD:JOIN", XB_ERR_SUBSTITUTE,
                       argc, argv);
    return;
  }
  if (!xb_wait_while(ctx, &argv[0], T_DONE, 0)) return;
  xb_object_get(argv[0].v.p, T_RESULT, r);
}

/* oThread:IsAlive(): whether the thread still runs. */
static void thread_is_alive(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  xb_set_logical(r, xb_atomic_load32(xb_object_word(&argv[0], T_DONE), XB_ACQUIRE) == 0);
}

static const xb_class_method thread_methods[] = {{"JOIN", thread_join}, {"ISALIVE", thread_is_alive}};
static const xb_class thread_class = {
  "THREAD", T_COUNT_, thread_vars, NULL, XB_TABLE_SIZE(thread_methods), thread_methods};

/* Program threads: each runs `body` attached to the collector, with a context of its own, counted in `running`. */
typedef struct program_thread {
  void (*body)(xb_ctx *ctx, void *arg);
  void *arg;
} program_thread;

/* The thread's first function: its frame lies above every frame the program's code has (gc.c scans from it). */
static void program_thread_main(void *p) {
  xb_gc_thread_attach((const void *)&p); /* the second thread makes the program's storage shared */
  program_thread t = *(program_thread *)p;
  free(p);
  xb_ctx *ctx = xb_ctx_new();
  const char *low, *high;
  if (xb_platform_stack_bounds(&low, &high)) xb_ctx_set_stack(ctx, low, high);
  if (xb_atomic_load32(&ending, XB_SEQ_CST)) xb_pending_set(ctx, XB_PENDING_QUIT); /* registered before the look */
  t.body(ctx, t.arg);
  xb_sched_thread_end();
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
  thread_gone();
}

int xb_program_thread_start(void (*body)(xb_ctx *ctx, void *arg), void *arg, size_t stack) {
  program_thread *t = malloc(sizeof *t);
  if (!t) xb_gc_fatal("out of memory");
  *t = (program_thread){body, arg};
  xb_atomic_add32(&running, 1, XB_ACQ_REL);
  xb_thread h;
  if (!xb_thread_start(&h, stack, program_thread_main, t)) {
    thread_gone();
    free(t);
    return 0;
  }
  xb_thread_detach(h);
  return 1;
}

void xb_eval_work(xb_ctx *ctx, xb_item *r, const xb_item *work) {
  uint32_t n = (uint32_t)xb_array_length(work->v.p);
  xb_item *args = malloc((n ? n : 1) * sizeof *args); /* the block, then its arguments: copies, which Eval may change */
  if (!args) xb_gc_fatal("out of memory");
  for (uint32_t k = 0; k < n; ++k) {
    xb_array_get(work->v.p, k, &args[k]);
    xb_gc_root_push(ctx, &args[k]);
  }
  xbf_EVAL(ctx, r, n, args);
  xb_gc_root_pop(ctx, n);
  free(args);
}

/* What a new thread takes from its creator, which waits until it has (`ready`): the creator's stack keeps the thread
 * object in place meanwhile. */
typedef struct start {
  xb_item thread;
  uint32_t ready;
} start;

/* The thread's end: its result (NIL when a BREAK or a QUIT ended it; a QUIT, or an error nobody handled, ends the
 * program), then the joiners woken. */
static void finish(xb_ctx *ctx, xb_item *thread, xb_item *result) {
  uint32_t ended = xb_pending_get(ctx) & XB_PENDING_UNWIND;
  if (ended & XB_PENDING_QUIT) xb_quit_all(ctx);
  if (ended) {
    xb_pending_clear(ctx, XB_PENDING_UNWIND);
    xb_set_nil(result);
  }
  xb_item nil;
  xb_set_nil(&nil);
  xb_object_put(thread->v.p, T_WORK, &nil);
  xb_object_put(thread->v.p, T_ENV, &nil);
  xb_object_put(thread->v.p, T_RESULT, result);
  xb_atomic_store32(xb_object_word(thread, T_DONE), 1, XB_RELEASE);
  xb_park_wake(xb_object_word(thread, T_DONE), 1);
}

static void thread_body(xb_ctx *ctx, void *p) {
  start *s = p;
  xb_item thread = s->thread, work, env, result;
  xb_set_nil(&result);
  xb_gc_root_push(ctx, &thread);
  xb_gc_root_push(ctx, &result);
  xb_atomic_store64(xb_object_word64(&thread, T_ID), xb_thread_self(), XB_RELAXED);
  xb_atomic_store32(&s->ready, 1, XB_RELEASE);
  xb_park_wake(&s->ready, 1); /* by address alone: the creator may be gone, with `s` */
  xb_object_get(thread.v.p, T_ENV, &env);
  xb_env_restore(ctx, &env);
  xb_object_get(thread.v.p, T_WORK, &work);
  xb_gc_root_push(ctx, &work);
  if (!(xb_pending_get(ctx) & XB_PENDING_QUIT)) xb_eval_work(ctx, &result, &work);
  xb_env_close_areas(ctx);
  finish(ctx, &thread, &result);
  xb_gc_root_pop(ctx, 3);
}

/* ThreadStart( bWork, [xArgs...] ): a Thread running Eval( bWork, xArgs... ). */
void xbf_THREADSTART(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (!argc || !xb_is_work(&argv[0])) {
    xb_flow_arg_error(ctx, XB_CODE_THREADSTART, "THREADSTART", argc, argv);
    return;
  }
  xb_set_object(ret, xb_object_new(&thread_class)); /* rooted there from now on */
  xb_array *work = xb_array_alloc(argc);
  memcpy(xb_array_items(work), argv, argc * sizeof *argv);
  xb_gc_write_items(work, 0, argc);
  xb_object *t = ret->v.p;
  xb_set_array(&t->vars[T_WORK], work);
  xb_set_logical(&t->vars[T_DONE], 0);
  xb_set_logical(&t->vars[T_ID], 0);
  xb_gc_write(t);
  xb_item env;
  xb_set_nil(&env);
  xb_gc_root_push(ctx, &env);
  xb_env_capture(ctx, &env);
  t->vars[T_ENV] = env; /* `t`, on the stack, stays where it is */
  xb_gc_write(t);
  xb_gc_root_pop(ctx, 1);
  xb_share(ret); /* the thread, the block, its arguments and the environment: the new thread reaches them */
  start s = {*ret, 0};
  if (!xb_program_thread_start(thread_body, &s, XB_PROGRAM_THREAD_STACK)) {
    xb_set_nil(ret); /* no thread: nothing to join */
    xb_error_described(ctx, NULL, EG_CREATE, XB_CODE_THREAD_CREATE, "Cannot start a thread", "THREADSTART", 0, 0, NULL);
    return;
  }
  while (!xb_atomic_load32(&s.ready, XB_ACQUIRE)) /* blocked: its attach stops every other thread */
    xb_park_wait_blocking(&s.ready, 0, -1);
}

/* Mutexes: the words of a recursive lock (xb_rwords, core/sync.h) in the object's variables; the flows that wait for it
 * asynchronously (LockAsync, HoldAsync: `waits` below), and how many waiters each flow has in line. */

enum {
  M_STATE, M_OWNER, M_DEPTH, M_QUEUE, M_WAITING, M_WAITERS, M_WAITERS_HEAD, M_WAITERS_COUNT, M_FLOWS, M_COUNT_
};
static const xb_class_var mutex_vars[] = {
  {"(STATE)", XB_VAR_ANY},   {"(OWNER)", XB_VAR_ANY},        {"(DEPTH)", XB_VAR_ANY},
  {"(QUEUE)", XB_VAR_ANY},   {"(WAITING)", XB_VAR_ANY},      {"(WAITERS)", XB_VAR_ANY},
  {"(WAITERSHEAD)", XB_VAR_ANY}, {"(WAITERSCOUNT)", XB_VAR_ANY}, {"(FLOWS)", XB_VAR_ANY}};
_Static_assert(XB_TABLE_SIZE(mutex_vars) == M_COUNT_, "one M_ index per Mutex variable");

/* Asynchronous waits for what a Mutex or a Semaphore gives (a lock, a place): the object's variables of its waiters
 * (the short lock of their ring; how many are in line, a word kept equal to the ring's count under that lock and read
 * without it, so that a release that finds none takes no lock; the ring), how the thing is taken for a flow without
 * waiting and given back, and, for a lock (which belongs to a flow), the flow that holds it (0: none) and the hash of
 * how many waiters each flow has in line (NULL and NO_FLOWS for a place, which belongs to none). A waiter is {task,
 * flow, value}: the flow it is for, the task's value once granted; NIL in the ring once it was granted out of its turn.
 */
#define NO_FLOWS UINT32_MAX
typedef struct waits {
  uint32_t queue, waiting;
  xb_ring ring;
  uint32_t flows;
  int (*take)(const xb_item *o, uint64_t flow);
  void (*give_back)(const xb_item *o, uint64_t flow);
  uint64_t (*owner)(const xb_item *o);
} waits;

enum { W_TASK, W_FLOW, W_VALUE, W_COUNT_ };
_Static_assert(W_TASK == 0, "a waiter's task first: xb_ring_pair_gone");

/* A flow's count of waiters in line changed by one (under the ring's lock): the count now. */
static int64_t flow_waiters(xb_ctx *ctx, const xb_item *o, const waits *k, uint64_t flow, int more) {
  xb_hash *h = ((xb_object *)o->v.p)->vars[k->flows].v.p;
  xb_item key, n;
  xb_set_int(&key, (int64_t)flow);
  int64_t now = (xb_hash_get(h, &key, &n) ? n.v.i : 0) + (more ? 1 : -1);
  xb_set_int(&n, now);
  if (more) xb_hash_put(ctx, h, &key, &n);
  else if (now > 0) xb_hash_replace(h, &key, &n);
  else xb_hash_remove(h, &key);
  return now;
}

/* The waits of an object, for the ring's `gone` test: a waiter granted out of its turn (NIL) or ended (canceled, timed
 * out) needs its place no more, and leaves its flow's count. */
typedef struct waits_of {
  const xb_item *o;
  const waits *k;
} waits_of;

static int waiter_gone(void *arg, const xb_item *w) {
  if (w->type != XB_T_ARRAY) return 1;
  if (!xb_ring_pair_gone(NULL, w)) return 0;
  const waits_of *of = arg;
  if (of->k->flows != NO_FLOWS) flow_waiters(NULL, of->o, of->k, (uint64_t)xb_array_items(w->v.p)[W_FLOW].v.i, 0);
  return 1;
}

/* The waiters of the flow `owner`, which holds the lock, granted out of their turn (they would wait behind others for
 * what their flow has): each takes it again and leaves NIL in its place. */
static void grant_owner(const xb_item *o, const waits *k, uint64_t owner, xb_claims *c) {
  xb_item nil;
  xb_set_nil(&nil);
  for (uint32_t i = 0; i < xb_ring_count(o, k->ring); ++i) {
    const xb_item *e = xb_ring_at(o, k->ring, i);
    if (e->type != XB_T_ARRAY) continue;
    xb_item *w = xb_array_items(e->v.p);
    if ((uint64_t)w[W_FLOW].v.i != owner || xb_task_is_final(&w[W_TASK]) || !k->take(o, owner)) continue;
    if (xb_task_claim(&w[W_TASK])) {
      c->held = *e;
      xb_ring_set(o, k->ring, i, &nil);
      flow_waiters(NULL, o, k, owner, 0);
      w = xb_array_items(c->held.v.p);
      xb_claims_add(c, &w[W_TASK], &w[W_VALUE]);
    } else {
      k->give_back(o, owner); /* it ended as it was granted */
    }
  }
}

/* The waiters of `o` given what is free in turn (under their ring's lock), each completed once that lock is
 * released: the first waiter takes it for its flow, and the others wait for more. A waiter that ended meanwhile
 * (canceled, timed out) is passed over; one that ends as it is granted gives back what it took. Then the waiters of
 * the flow that holds the lock (granted now, or taken without the line) take it too. */
static void grant(xb_ctx *ctx, const xb_item *o, const waits *k) {
  xb_claims c;
  xb_claims_root(ctx, &c);
  xb_object_lock_short(o, k->queue);
  while (xb_ring_count(o, k->ring)) {
    const xb_item *e = xb_ring_at(o, k->ring, 0);
    if (e->type != XB_T_ARRAY) { /* the place of a waiter granted out of its turn */
      xb_ring_pop(o, k->ring, &c.held);
      continue;
    }
    xb_item *w = xb_array_items(e->v.p);
    uint64_t flow = (uint64_t)w[W_FLOW].v.i;
    int gone = xb_task_is_final(&w[W_TASK]);
    if (!gone && !k->take(o, flow)) break; /* none free: the next release grants again */
    xb_ring_pop(o, k->ring, &c.held);
    if (k->flows != NO_FLOWS) flow_waiters(NULL, o, k, flow, 0);
    w = xb_array_items(c.held.v.p);
    if (gone) continue;
    if (xb_task_claim(&w[W_TASK])) xb_claims_add(&c, &w[W_TASK], &w[W_VALUE]);
    else k->give_back(o, flow); /* it ended as it was granted */
  }
  if (k->flows != NO_FLOWS && xb_ring_count(o, k->ring)) {
    uint64_t owner = k->owner(o);
    xb_item key, n;
    xb_set_int(&key, (int64_t)owner);
    if (owner && xb_hash_get(((xb_object *)o->v.p)->vars[k->flows].v.p, &key, &n)) grant_owner(o, k, owner, &c);
  }
  xb_atomic_store32(xb_object_word(o, k->waiting), xb_ring_count(o, k->ring), XB_SEQ_CST);
  xb_object_unlock_short(o, k->queue);
  xb_claims_settle(ctx, &c);
  xb_gc_root_pop(ctx, 2);
}

/* After a release of what `o` gives, or a lock taken without the line: its waiters granted when there are any. The
 * change before and the count of waiters are ordered against a waiter's count and its take (a sequentially
 * consistent fence on both sides): one of the two sees the other. */
static void released(xb_ctx *ctx, const xb_item *o, const waits *k) {
  xb_atomic_fence(XB_SEQ_CST);
  if (xb_atomic_load32(xb_object_word(o, k->waiting), XB_RELAXED)) grant(ctx, o, k);
}

/* A waiter of `o` for this flow until `deadline` (< 0: none) unless `*token` cancels it first: its task in `*r`,
 * completed with `*granted` once taken, else with `*timed_out` at the deadline; at once when it can be taken now and
 * no other waiter is in line before it (or the flow holds it already). 0, with the argument error of `what`, for a
 * token that is no token. */
static int wait_async(xb_ctx *ctx, xb_item *r, const xb_item *o, const waits *k, const xb_item *token,
                      int64_t deadline, const xb_item *granted, const xb_item *timed_out, const char *what,
                      uint32_t argc, xb_item *argv) {
  uint64_t self = xb_flow_self();
  int first = !xb_atomic_load32(xb_object_word(o, k->waiting), XB_SEQ_CST) || (k->owner && k->owner(o) == self);
  if (first && k->take(o, self)) {
    xb_task_completed(ctx, r, granted);
    return 1;
  }
  xb_item waiter;
  xb_set_nil(&waiter);
  xb_gc_root_push(ctx, &waiter);
  int ok = xb_task_waiter(ctx, r, token, deadline, timed_out, what, argc, argv);
  if (ok) {
    xb_array *w = xb_array_alloc(W_COUNT_); /* `w`, on the stack, stays where it is */
    xb_set_array(&waiter, w);
    xb_item *items = xb_array_items(w);
    items[W_TASK] = *r;
    xb_set_int(&items[W_FLOW], (int64_t)self);
    items[W_VALUE] = *granted;
    xb_gc_write(w);
    xb_share(&waiter);
    waits_of of = {o, k};
    xb_object_lock_short(o, k->queue);
    xb_ring_push(o, k->ring, &waiter, waiter_gone, &of);
    if (k->flows != NO_FLOWS) flow_waiters(ctx, o, k, self, 1);
    xb_atomic_store32(xb_object_word(o, k->waiting), xb_ring_count(o, k->ring), XB_SEQ_CST);
    xb_object_unlock_short(o, k->queue);
    xb_atomic_fence(XB_SEQ_CST); /* the count before the take (released's other side) */
    grant(ctx, o, k); /* freed meanwhile: the first waiter takes it */
  }
  xb_gc_root_pop(ctx, 1);
  return ok;
}

/* The waits of an asynchronous call ( [nMs], [oToken] ) after the object, checked whether it waits or not: 0 after
 * an argument error. */
static int async_args(xb_ctx *ctx, const char *what, uint32_t argc, xb_item *argv, int64_t *deadline,
                      xb_item *token) {
  if (!xb_deadline_arg(ctx, what, XB_CODE_TASK, 1, argc, argv, deadline)) return 0;
  if (argc > 2) *token = argv[2];
  else xb_set_nil(token);
  if (xb_task_token_ok(token)) return 1;
  xb_flow_arg_error(ctx, XB_CODE_TASK, what, argc, argv);
  return 0;
}

xb_rwords xb_object_rwords(const xb_item *object, uint32_t k) {
  return (xb_rwords){xb_object_word(object, k), xb_object_word64(object, k + 1), xb_object_word(object, k + 2)};
}

int xb_object_lock(xb_ctx *ctx, const xb_item *object, uint32_t k) {
  uint64_t self = xb_flow_self();
  for (uint32_t spin = 0; spin < LOCK_SPINS; ++spin) /* a holder's work is short: worth a look before sleeping */
    if ((spin == 0 || !xb_atomic_load32(xb_object_word(object, k), XB_RELAXED)) &&
        xb_rwords_try(xb_object_rwords(object, k), self))
      return 1;
  while (!xb_rwords_claim(xb_object_rwords(object, k), self)) {
    if (xb_flow_outer(xb_atomic_load64(xb_object_word64(object, k + 1), XB_RELAXED))) { /* waits for itself */
      xb_error_described(ctx, NULL, EG_ARG, XB_CODE_MUTEX_OUTER, "Mutex held by a flow this one runs in",
                         "MUTEX:LOCK", 0, 0, NULL);
      return 0;
    }
    if (!xb_wait_while(ctx, object, k, 2)) return 0;
  }
  return 1;
}

/* The word: 0 free, 1 held, 2 held and maybe waited for. A holder never takes it again, so it needs no owner. */
void xb_object_lock_short(const xb_item *object, uint32_t k) {
  uint32_t free_state = 0;
  if (xb_atomic_cas32(xb_object_word(object, k), &free_state, 1, XB_ACQUIRE)) return;
  for (uint32_t spin = 0; spin < LOCK_SPINS; ++spin) { /* loads until it looks free: the holder's work is short */
    if (xb_atomic_load32(xb_object_word(object, k), XB_RELAXED)) continue;
    free_state = 0;
    if (xb_atomic_cas32(xb_object_word(object, k), &free_state, 1, XB_ACQUIRE)) return;
  }
  while (xb_atomic_xchg32(xb_object_word(object, k), 2, XB_ACQUIRE) != 0) /* the address again after each wait */
    xb_park_wait_short(xb_object_word(object, k), 2, -1);
}

void xb_object_unlock_short(const xb_item *object, uint32_t k) {
  if (xb_atomic_xchg32(xb_object_word(object, k), 0, XB_RELEASE) == 2) xb_park_wake(xb_object_word(object, k), 0);
}

static xb_rwords mutex_words(const xb_item *m) { return xb_object_rwords(m, M_STATE); }

static int mutex_take_for(const xb_item *m, uint64_t flow) { return xb_rwords_try(mutex_words(m), flow); }
static void mutex_give_back(const xb_item *m, uint64_t flow) { xb_rwords_release(mutex_words(m), flow); }
static uint64_t mutex_owner(const xb_item *m) { return xb_atomic_load64(xb_object_word64(m, M_OWNER), XB_RELAXED); }
static const waits mutex_waits = {M_QUEUE,        M_WAITING,       {M_WAITERS, M_WAITERS_HEAD, M_WAITERS_COUNT},
                                  M_FLOWS,        mutex_take_for,  mutex_give_back,
                                  mutex_owner};

/* The mutex released once by `self`: 0 when `self` does not hold it. Released whole, it goes to an asynchronous
 * waiter. */
static int mutex_release(xb_ctx *ctx, const xb_item *m, uint64_t self) {
  if (!xb_rwords_release(mutex_words(m), self)) return 0;
  if (mutex_owner(m) != self) released(ctx, m, &mutex_waits);
  return 1;
}

/* The mutex taken by this flow, as Lock() does: its waiters of this flow already in line take it too (released). */
static int mutex_take(xb_ctx *ctx, const xb_item *m) {
  if (!xb_object_lock(ctx, m, M_STATE)) return 0;
  released(ctx, m, &mutex_waits);
  return 1;
}

static int is_mutex(const xb_item *it);

/* oMutex:Lock(): takes the lock, waiting while another flow holds it (or until a QUIT ends the wait). */
static void mutex_lock(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  xb_set_nil(r);
  mutex_take(ctx, &argv[0]);
}

/* oMutex:TryLock(): takes the lock when no other flow holds it: whether it did. */
static void mutex_try_lock(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  int taken = xb_rwords_try(mutex_words(&argv[0]), xb_flow_self());
  if (taken) released(ctx, &argv[0], &mutex_waits);
  xb_set_logical(r, taken);
}

/* oMutex:Unlock(): releases the lock once; an error when this flow does not hold it. */
static void mutex_unlock(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  xb_set_nil(r);
  if (!mutex_release(ctx, &argv[0], xb_flow_self()))
    xb_error_described(ctx, NULL, EG_ARG, XB_CODE_MUTEX_UNLOCK, "Mutex not held", "MUTEX:UNLOCK", 0, argc, argv);
}

/* Holders: what oMutex:Hold() returns, its Dispose() releasing the mutex once (USING, SYNCHRONIZED). */

enum { H_MUTEX, H_COUNT_ };
static const xb_class_var holder_vars[] = {{"(MUTEX)", XB_VAR_ANY}};
_Static_assert(XB_TABLE_SIZE(holder_vars) == H_COUNT_, "one H_ index per holder variable");

/* oHolder:Dispose(): the mutex released once, by the flow holding it; nothing after, nor from another flow. */
static void holder_dispose(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  xb_set_nil(r);
  xb_item m, nil;
  xb_object_get(argv[0].v.p, H_MUTEX, &m);
  xb_gc_root_push(ctx, &m);
  int released = is_mutex(&m) && mutex_release(ctx, &m, xb_flow_self());
  xb_gc_root_pop(ctx, 1);
  if (!released) return; /* released, or another's */
  xb_set_nil(&nil);
  xb_object_put(argv[0].v.p, H_MUTEX, &nil);
}

static const xb_class_method holder_methods[] = {{"DISPOSE", holder_dispose}};
static const xb_class holder_class = {
  "MUTEXHOLDER", H_COUNT_, holder_vars, NULL, XB_TABLE_SIZE(holder_methods), holder_methods};

/* oMutex:Hold(): takes the lock as Lock() does and returns a holder whose Dispose() releases it (NIL when a QUIT ended
 * the wait). */
static void mutex_hold(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  xb_set_nil(r);
  xb_object *h = xb_object_new(&holder_class); /* before the lock: an allocation may wait for a collection */
  xb_set_object(r, h);
  h->vars[H_MUTEX] = argv[0];
  xb_gc_write(h);
  if (!mutex_take(ctx, &argv[0])) {
    xb_set_nil(r);
  } else if (xb_pending_get(ctx) & XB_PENDING_UNWIND) { /* a QUIT came meanwhile: no USING will store the holder */
    mutex_release(ctx, &argv[0], xb_flow_self());
    xb_set_nil(r);
  }
}

/* oMutex:LockAsync( [nMs], [oToken] ): a task that completes with .T. once this flow holds the lock (at once when it
 * is free or the flow's), with .F. when nMs milliseconds passed first; the token cancels the wait. An asynchronous
 * function that awaits it holds the lock across its steps, as Lock() would, without a worker waiting. */
static void mutex_lock_async(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  int64_t deadline;
  xb_item token, granted, timed_out;
  xb_set_nil(r);
  if (!async_args(ctx, "MUTEX:LOCKASYNC", argc, argv, &deadline, &token)) return;
  xb_set_logical(&granted, 1);
  xb_set_logical(&timed_out, 0);
  xb_gc_root_push(ctx, &token);
  wait_async(ctx, r, &argv[0], &mutex_waits, &token, deadline, &granted, &timed_out, "MUTEX:LOCKASYNC", argc, argv);
  xb_gc_root_pop(ctx, 1);
}

/* oMutex:HoldAsync( [nMs], [oToken] ): LockAsync's wait, completed with a holder (Hold()'s: its Dispose() releases the
 * lock), or with NIL when nMs milliseconds passed first. */
static void mutex_hold_async(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  int64_t deadline;
  xb_item token, holder, nil;
  xb_set_nil(r);
  xb_set_nil(&holder);
  xb_set_nil(&nil);
  if (!async_args(ctx, "MUTEX:HOLDASYNC", argc, argv, &deadline, &token)) return;
  xb_gc_root_push(ctx, &token);
  xb_gc_root_push(ctx, &holder);
  xb_object *h = xb_object_new(&holder_class);
  xb_set_object(&holder, h);
  h->vars[H_MUTEX] = argv[0];
  xb_gc_write(h);
  xb_share(&holder);
  wait_async(ctx, r, &argv[0], &mutex_waits, &token, deadline, &holder, &nil, "MUTEX:HOLDASYNC", argc, argv);
  xb_gc_root_pop(ctx, 2);
}

static const xb_class_method mutex_methods[] = {
  {"LOCK", mutex_lock},           {"TRYLOCK", mutex_try_lock}, {"UNLOCK", mutex_unlock}, {"HOLD", mutex_hold},
  {"LOCKASYNC", mutex_lock_async}, {"HOLDASYNC", mutex_hold_async}};
static const xb_class mutex_class = {"MUTEX", M_COUNT_, mutex_vars, NULL, XB_TABLE_SIZE(mutex_methods), mutex_methods};

static int is_mutex(const xb_item *it) {
  return it->type == XB_T_OBJECT && ((const xb_object *)it->v.p)->cls == &mutex_class;
}

/* Mutex(): a mutex, free. */
void xbf_MUTEX(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_object *m = xb_object_new(&mutex_class);
  for (uint32_t k = 0; k < M_WAITERS; ++k) xb_set_logical(&m->vars[k], 0);
  xb_ring_init(m, mutex_waits.ring);
  xb_set_hash(&m->vars[M_FLOWS], xb_hash_alloc()); /* `m` is new: no barrier */
  xb_set_object(ret, m);
}

/* Conditions: the waiters and the signals given to them in one word (16 bits each: a waiter takes its signal and
 * leaves in one step, and a signal never outlives the waiters it was for), and a generation that each signal changes,
 * which waiters park on. */

enum { C_WAITERS, C_GENERATION, C_COUNT_ };
static const xb_class_var condition_vars[] = {{"(WAITERS)", XB_VAR_ANY}, {"(GENERATION)", XB_VAR_ANY}};
_Static_assert(XB_TABLE_SIZE(condition_vars) == C_COUNT_, "one C_ index per Condition variable");

#define C_WAITER 1u
#define C_SIGNAL (1u << 16)
#define C_FIELD 0xFFFFu
#define C_SIGNALS(w) ((w) >> 16)
#define C_WAITING(w) ((w) & C_FIELD)

/* A waiter left, the word `before` it did: a thread waiting for a place (the word was full) is woken. */
static void condition_left(const xb_item *c, uint32_t before) {
  if (C_WAITING(before) == C_FIELD) xb_park_wake(xb_object_word(c, C_WAITERS), 0);
}

/* A waiter leaves: with its signal when one is there (1), else without (0). */
static int condition_leave(const xb_item *c) {
  uint32_t *word = xb_object_word(c, C_WAITERS), w = xb_atomic_load32(word, XB_ACQUIRE);
  for (;;) {
    uint32_t signalled = C_SIGNALS(w) > 0;
    if (xb_atomic_cas32(word, &w, w - C_WAITER - (signalled ? C_SIGNAL : 0), XB_ACQ_REL)) {
      condition_left(c, w);
      return (int)signalled;
    }
  }
}

/* The calling thread counted as a waiter: 0 when the word counts as many as it can. */
static int condition_join(const xb_item *c) {
  uint32_t *word = xb_object_word(c, C_WAITERS), w = xb_atomic_load32(word, XB_RELAXED);
  while (C_WAITING(w) != C_FIELD)
    if (xb_atomic_cas32(word, &w, w + C_WAITER, XB_ACQ_REL)) return 1;
  return 0;
}

/* oCondition:Wait( oMutex, [nMs] ): releases oMutex, which this flow holds (however many times it took it), waits
 * for a Notify() (or NotifyAll()) or nMs milliseconds, takes oMutex back as it was: whether it was notified. A QUIT
 * ends the wait without the mutex. A thread that finds no place among the waiters waits for one with the mutex
 * released, then returns .F. as if it had timed out: its caller tests its condition again. */
static void condition_wait(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  int64_t deadline;
  xb_set_nil(r);
  if (argc < 2 || !is_mutex(&argv[1]) ||
      xb_atomic_load64(xb_object_word64(&argv[1], M_OWNER), XB_RELAXED) != xb_flow_self()) {
    xb_error_described(ctx, NULL, EG_ARG, XB_CODE_CONDITION, "Mutex not held", "CONDITION:WAIT", 0, argc, argv);
    return;
  }
  if (!xb_deadline_arg(ctx, "CONDITION:WAIT", XB_CODE_CONDITION, 2, argc, argv, &deadline)) return;
  int joined = condition_join(&argv[0]); /* before the mutex goes: a Notify() after it finds this waiter */
  uint32_t depth = *xb_object_word(&argv[1], M_DEPTH);
  *xb_object_word(&argv[1], M_DEPTH) = 1;
  mutex_release(ctx, &argv[1], xb_flow_self());
  int notified = 0, ended = XB_WAIT_CHANGED;
  if (!joined) {
    while (ended == XB_WAIT_CHANGED) {
      uint32_t w = xb_atomic_load32(xb_object_word(&argv[0], C_WAITERS), XB_ACQUIRE);
      if (C_WAITING(w) != C_FIELD) break;
      ended = xb_wait_until(ctx, &argv[0], C_WAITERS, w, deadline);
    }
  } else {
    while (ended == XB_WAIT_CHANGED) {
      uint32_t generation = xb_atomic_load32(xb_object_word(&argv[0], C_GENERATION), XB_ACQUIRE);
      uint32_t *word = xb_object_word(&argv[0], C_WAITERS), w = xb_atomic_load32(word, XB_ACQUIRE);
      if (C_SIGNALS(w) && xb_atomic_cas32(word, &w, w - C_WAITER - C_SIGNAL, XB_ACQ_REL)) {
        condition_left(&argv[0], w);
        notified = 1;
        break;
      }
      if (C_SIGNALS(w)) continue; /* another waiter took one meanwhile: look again */
      ended = xb_wait_until(ctx, &argv[0], C_GENERATION, generation, deadline);
    }
    if (!notified) notified = condition_leave(&argv[0]); /* a signal given as it gave up is its own */
  }
  if (ended == XB_WAIT_QUIT || !mutex_take(ctx, &argv[1])) return;
  *xb_object_word(&argv[1], M_DEPTH) = depth;
  xb_set_logical(r, notified);
}

/* Signals for `wanted` more of the waiters that have none (all of them: UINT32_MAX), then the waiters woken. */
static void condition_signal(const xb_item *c, uint32_t wanted, int all) {
  uint32_t *word = xb_object_word(c, C_WAITERS), w = xb_atomic_load32(word, XB_ACQUIRE), given;
  do {
    uint32_t idle = C_WAITING(w) - C_SIGNALS(w);
    given = idle < wanted ? idle : wanted;
    if (!given) return;
  } while (!xb_atomic_cas32(word, &w, w + given * C_SIGNAL, XB_ACQ_REL));
  xb_atomic_add32(xb_object_word(c, C_GENERATION), 1, XB_RELEASE);
  xb_park_wake(xb_object_word(c, C_GENERATION), all);
}

/* oCondition:Notify(): one waiting thread goes on (none waits: nothing happens). */
static void condition_notify(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  xb_set_nil(r);
  condition_signal(&argv[0], 1, 0);
}

/* oCondition:NotifyAll(): every waiting thread goes on. */
static void condition_notify_all(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  xb_set_nil(r);
  condition_signal(&argv[0], UINT32_MAX, 1);
}

static const xb_class_method condition_methods[] = {
  {"WAIT", condition_wait}, {"NOTIFY", condition_notify}, {"NOTIFYALL", condition_notify_all}};
static const xb_class condition_class = {
  "CONDITION", C_COUNT_, condition_vars, NULL, XB_TABLE_SIZE(condition_methods), condition_methods};

/* Condition(): a condition variable, which threads wait on holding a Mutex. */
void xbf_CONDITION(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_object *c = xb_object_new(&condition_class);
  for (uint32_t k = 0; k < C_COUNT_; ++k) xb_set_logical(&c->vars[k], 0);
  xb_set_object(ret, c);
}

/* Semaphores: the count of free places and the most it may reach; the tasks that wait for a place asynchronously
 * (WaitAsync, waits as a Mutex's are). */

enum { S_COUNT, S_MAXIMUM, S_QUEUE, S_WAITING, S_WAITERS, S_WAITERS_HEAD, S_WAITERS_COUNT, S_COUNT_ };
static const xb_class_var semaphore_vars[] = {
  {"(COUNT)", XB_VAR_ANY},   {"(MAXIMUM)", XB_VAR_ANY}, {"(QUEUE)", XB_VAR_ANY},
  {"(WAITING)", XB_VAR_ANY}, {"(WAITERS)", XB_VAR_ANY}, {"(WAITERSHEAD)", XB_VAR_ANY}, {"(WAITERSCOUNT)", XB_VAR_ANY}};
_Static_assert(XB_TABLE_SIZE(semaphore_vars) == S_COUNT_, "one S_ index per Semaphore variable");

/* A place taken, when one is free (for any flow: places belong to none). */
static int semaphore_take(const xb_item *s, uint64_t flow) {
  XB_UNUSED(flow);
  uint32_t *word = xb_object_word(s, S_COUNT), n = xb_atomic_load32(word, XB_ACQUIRE);
  while (n)
    if (xb_atomic_cas32(word, &n, n - 1, XB_ACQ_REL)) return 1;
  return 0;
}

/* A place given back by a waiter that ended as it was granted: a thread waiting for one is woken. */
static void semaphore_give_back(const xb_item *s, uint64_t flow) {
  XB_UNUSED(flow);
  uint32_t *word = xb_object_word(s, S_COUNT);
  if (!xb_atomic_add32(word, 1, XB_ACQ_REL)) xb_park_wake(word, 0);
}

static const waits semaphore_waits = {S_QUEUE,        S_WAITING,           {S_WAITERS, S_WAITERS_HEAD, S_WAITERS_COUNT},
                                      NO_FLOWS,       semaphore_take,      semaphore_give_back,
                                      NULL};

#define SEMAPHORE_MAX INT32_MAX

/* oSemaphore:Wait( [nMs] ): takes a place, waiting up to nMs milliseconds (without: as long as it takes) while none
 * is free: whether it took one. */
static void semaphore_wait(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  int64_t deadline;
  xb_set_nil(r);
  if (!xb_deadline_arg(ctx, "SEMAPHORE:WAIT", XB_CODE_SEMAPHORE, 1, argc, argv, &deadline)) return;
  for (;;) {
    uint32_t *word = xb_object_word(&argv[0], S_COUNT), n = xb_atomic_load32(word, XB_ACQUIRE);
    if (n) {
      if (!xb_atomic_cas32(word, &n, n - 1, XB_ACQ_REL)) continue;
      if (n > 1) xb_park_wake(word, 0); /* places left: the next waiter takes one (Release() wakes only from 0) */
      break;
    }
    int ended = xb_wait_until(ctx, &argv[0], S_COUNT, 0, deadline);
    if (ended == XB_WAIT_QUIT) return;
    if (ended == XB_WAIT_TIMED_OUT) {
      xb_set_logical(r, 0);
      return;
    }
  }
  xb_set_logical(r, 1);
}

/* oSemaphore:Release( [nCount] ): gives back nCount places (1 without it): the count before; past the maximum, an
 * error that changes nothing. */
static void semaphore_release(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  int64_t n = 1;
  xb_set_nil(r);
  if (argc > 1 && argv[1].type != XB_T_NIL) {
    if (!xb_integer_arg(ctx, "SEMAPHORE:RELEASE", XB_CODE_SEMAPHORE, 1, argc, argv, &n)) return;
    if (n < 1) {
      xb_flow_arg_error(ctx, XB_CODE_SEMAPHORE, "SEMAPHORE:RELEASE", argc, argv);
      return;
    }
  }
  uint32_t *word = xb_object_word(&argv[0], S_COUNT), before = xb_atomic_load32(word, XB_ACQUIRE);
  uint32_t maximum = xb_atomic_load32(xb_object_word(&argv[0], S_MAXIMUM), XB_RELAXED);
  do {
    if (n > (int64_t)maximum - (int64_t)before) {
      xb_error_described(ctx, NULL, EG_LIMIT, XB_CODE_SEMAPHORE_FULL, "Semaphore full", "SEMAPHORE:RELEASE", 0, argc,
                         argv);
      return;
    }
  } while (!xb_atomic_cas32(word, &before, before + (uint32_t)n, XB_ACQ_REL));
  if (!before) xb_park_wake(word, 0); /* one: each waiter that takes a place with more left wakes the next */
  released(ctx, &argv[0], &semaphore_waits);
  xb_set_int(r, before);
}

/* oSemaphore:WaitAsync( [nMs], [oToken] ): a task that completes with .T. once it took a place (at once when one is
 * free), with .F. when nMs milliseconds passed first; the token cancels the wait. Places a Release() gives go to these
 * waiters in their order. */
static void semaphore_wait_async(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  int64_t deadline;
  xb_item token, granted, timed_out;
  xb_set_nil(r);
  if (!async_args(ctx, "SEMAPHORE:WAITASYNC", argc, argv, &deadline, &token)) return;
  xb_set_logical(&granted, 1);
  xb_set_logical(&timed_out, 0);
  xb_gc_root_push(ctx, &token);
  wait_async(ctx, r, &argv[0], &semaphore_waits, &token, deadline, &granted, &timed_out, "SEMAPHORE:WAITASYNC", argc,
             argv);
  xb_gc_root_pop(ctx, 1);
}

/* oSemaphore:Count(): the places free now. */
static void semaphore_count(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  xb_set_int(r, xb_atomic_load32(xb_object_word(&argv[0], S_COUNT), XB_ACQUIRE));
}

static const xb_class_method semaphore_methods[] = {{"WAIT", semaphore_wait},
                                                    {"RELEASE", semaphore_release},
                                                    {"COUNT", semaphore_count},
                                                    {"WAITASYNC", semaphore_wait_async}};
static const xb_class semaphore_class = {
  "SEMAPHORE", S_COUNT_, semaphore_vars, NULL, XB_TABLE_SIZE(semaphore_methods), semaphore_methods};

/* Semaphore( nInitial, [nMaximum] ): a semaphore with nInitial places free of nMaximum (without it: 2^31 - 1); a count
 * outside 0 .. 2^31 - 1, a maximum below 1 or below nInitial is an argument error. */
void xbf_SEMAPHORE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  int64_t initial, maximum = SEMAPHORE_MAX;
  if (!xb_integer_arg(ctx, "SEMAPHORE", XB_CODE_SEMAPHORE, 0, argc, argv, &initial)) return;
  if (argc > 1 && argv[1].type != XB_T_NIL &&
      !xb_integer_arg(ctx, "SEMAPHORE", XB_CODE_SEMAPHORE, 1, argc, argv, &maximum))
    return;
  if (initial < 0 || maximum < 1 || maximum > SEMAPHORE_MAX || initial > maximum) {
    xb_flow_arg_error(ctx, XB_CODE_SEMAPHORE, "SEMAPHORE", argc, argv);
    return;
  }
  xb_object *s = xb_object_new(&semaphore_class);
  for (uint32_t k = 0; k < S_WAITERS; ++k) xb_set_logical(&s->vars[k], 0);
  xb_ring_init(s, semaphore_waits.ring);
  s->vars[S_COUNT].v.l = (int32_t)initial;
  s->vars[S_MAXIMUM].v.l = (int32_t)maximum;
  xb_set_object(ret, s);
}

/* Atomic integers: one integer, read whole and changed under the object's lock once it is shared (in place while it
 * is local). */

enum { A_VALUE, A_COUNT_ };
static const xb_class_var atomic_vars[] = {{"(VALUE)", XB_VAR_ANY}};
_Static_assert(XB_TABLE_SIZE(atomic_vars) == A_COUNT_, "one A_ index per AtomicInt variable");


/* The value replaced by next( value, args ) unless that returns 0, in one step: the value it had. */
static int64_t atomic_step(const xb_item *a, int (*next)(int64_t value, const int64_t *args, int64_t *out),
                           const int64_t *args) {
  xb_object *o = a->v.p;
  int shared = xb_is_shared(o);
  if (shared) xb_seq_lock(o);
  int64_t value = o->vars[A_VALUE].v.i, changed;
  if (next(value, args, &changed)) {
    xb_item v;
    xb_set_int(&v, changed);
    if (shared) xb_seq_store(&o->vars[A_VALUE], &v);
    else o->vars[A_VALUE] = v; /* an integer: no barrier */
  }
  if (shared) xb_seq_unlock(o);
  return value;
}

static int add(int64_t value, const int64_t *args, int64_t *out) {
  *out = (int64_t)((uint64_t)value + (uint64_t)args[0]); /* wraps, as two's complement does */
  return 1;
}

static int exchange_if(int64_t value, const int64_t *args, int64_t *out) {
  *out = args[1];
  return value == args[0];
}

static int store(int64_t value, const int64_t *args, int64_t *out) {
  XB_UNUSED(value);
  *out = args[0];
  return 1;
}

/* oAtomic:Value: the integer. */
static void atomic_value(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  xb_object_get(argv[0].v.p, A_VALUE, r);
}

/* oAtomic:Value := n: stores n (an integer). */
static void atomic_set(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  int64_t n[1];
  xb_set_nil(r);
  if (!xb_integer_arg(ctx, "ATOMICINT:VALUE", XB_CODE_ATOMICINT, 1, argc, argv, &n[0])) return;
  atomic_step(&argv[0], store, n);
  xb_set_int(r, n[0]);
}

/* oAtomic:Add( n ): adds n (an integer) and returns the new value. */
static void atomic_add(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  int64_t n[1];
  xb_set_nil(r);
  if (!xb_integer_arg(ctx, "ATOMICINT:ADD", XB_CODE_ATOMICINT, 1, argc, argv, &n[0])) return;
  xb_set_int(r, (int64_t)((uint64_t)atomic_step(&argv[0], add, n) + (uint64_t)n[0]));
}

/* oAtomic:CompareExchange( nExpected, nNew ): nNew stored when the value is nExpected: whether it was. */
static void atomic_compare_exchange(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  int64_t n[2];
  xb_set_nil(r);
  if (!xb_integer_arg(ctx, "ATOMICINT:COMPAREEXCHANGE", XB_CODE_ATOMICINT, 1, argc, argv, &n[0]) ||
      !xb_integer_arg(ctx, "ATOMICINT:COMPAREEXCHANGE", XB_CODE_ATOMICINT, 2, argc, argv, &n[1]))
    return;
  xb_set_logical(r, atomic_step(&argv[0], exchange_if, n) == n[0]);
}

static const xb_class_method atomic_methods[] = {
  {"VALUE", atomic_value}, {"_VALUE", atomic_set}, {"ADD", atomic_add}, {"COMPAREEXCHANGE", atomic_compare_exchange}};
static const xb_class atomic_class = {
  "ATOMICINT", A_COUNT_, atomic_vars, NULL, XB_TABLE_SIZE(atomic_methods), atomic_methods};

/* AtomicInt( [nValue] ): an atomic integer holding nValue (0 without it). */
void xbf_ATOMICINT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  int64_t n = 0;
  if (argc && argv[0].type != XB_T_NIL && !xb_integer_arg(ctx, "ATOMICINT", XB_CODE_ATOMICINT, 0, argc, argv, &n))
    return;
  xb_object *a = xb_object_new(&atomic_class);
  xb_set_int(&a->vars[A_VALUE], n);
  xb_set_object(ret, a);
}
