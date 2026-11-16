/* Threads, mutexes, conditions, semaphores and atomic integers of the program (docs/concurrency.md#threads).
 *
 * The words threads wait on are 32-bit values (v.l) of the objects' own variables, changed atomically: an object
 * moves only while every thread is stopped, and one a waiter waits on is pinned by its stack. A wait is a blocking
 * operation (collections go on without the waiter) of a while at a time, so that a QUIT finds the waiter. */
#include "xbrt_int.h"

#include "core/shared.h"
#include "core/sync.h"

#include <stdlib.h>
#include <string.h>

void xbf_EVAL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

/* The stack a thread of the program may grow to (reserved, committed as it is used). */
#define THREAD_STACK ((size_t)16 << 20)

/* How long a wait sleeps before it looks for a QUIT again. */
#define WAIT_LOOK_NS ((int64_t)50 * XB_NS_PER_MS)

/* xbase's own error codes of these functions (9001 to 9015 are the bit and character functions'). */
enum {
  CODE_THREADSTART = 9016,
  CODE_THREAD_CREATE,
  CODE_THREAD_JOIN,
  CODE_MUTEX_UNLOCK,
  CODE_ATOMICINT,
  CODE_CONDITION,
  CODE_SEMAPHORE,
  CODE_SEMAPHORE_FULL
};

/* The threads ThreadStart() started that have not ended, which the program's end waits for; `ending` while threads are
 * asked to QUIT (a QUIT, the start procedure returned): a thread that starts then quits at once. */
static uint32_t running;
static uint32_t ending;

/* The word of an object's own variable `k`. */
static uint32_t *word_of(const xb_item *object, uint32_t k) {
  return (uint32_t *)&((xb_object *)object->v.p)->vars[k].v.l;
}

/* How a wait ended. */
enum { WAIT_QUIT = -1, WAIT_TIMED_OUT, WAIT_CHANGED };

/* Waits while the word of `object`'s variable `k` is `value`, until the monotonic clock reaches `deadline` (< 0: no
 * limit). `object` is an item that keeps the object (it may move between the waits). */
static int wait_until(xb_ctx *ctx, const xb_item *object, uint32_t k, uint32_t value, int64_t deadline) {
  for (;;) {
    const uint32_t *word = word_of(object, k);
    if (xb_atomic_load32(word, XB_ACQUIRE) != value) return WAIT_CHANGED;
    if (xb_pending_get(ctx) & XB_PENDING_QUIT) return WAIT_QUIT;
    int64_t slice = WAIT_LOOK_NS;
    if (deadline >= 0) {
      int64_t left = deadline - xb_platform_monotonic_ns();
      if (left <= 0) return WAIT_TIMED_OUT;
      if (left < slice) slice = left;
    }
    xb_park_wait_blocking(word, value, slice);
  }
}

/* Waits while the word is `value`, with no limit: 1 once it changed, 0 when a QUIT asks this thread to end. */
static int wait_while(xb_ctx *ctx, const xb_item *object, uint32_t k, uint32_t value) {
  return wait_until(ctx, object, k, value, -1) == WAIT_CHANGED;
}

/* The argument error `code` of `what` (a function or "CLASS:METHOD"), not substitutable. */
static void arg_error(xb_ctx *ctx, int32_t code, const char *what, uint32_t argc, xb_item *argv) {
  xb_error_described(ctx, NULL, EG_ARG, code, "Argument error", what, 0, argc, argv);
}

/* The deadline of a wait of argument `k` milliseconds (NIL, missing or negative: none, -1) into `*deadline`; anything
 * but a number or NIL is the argument error `code` of `what`: 0. */
static int deadline_arg(xb_ctx *ctx, const char *what, int32_t code, uint32_t k, uint32_t argc, xb_item *argv,
                        int64_t *deadline) {
  *deadline = -1;
  if (k >= argc || argv[k].type == XB_T_NIL) return 1;
  if (!xb_is_number(&argv[k])) {
    arg_error(ctx, code, what, argc, argv);
    return 0;
  }
  int64_t ms = xb_num_to_i64(&argv[k]), limit = INT64_MAX / 2 / XB_NS_PER_MS; /* far enough, and no overflow */
  if (ms >= 0) *deadline = xb_platform_monotonic_ns() + (ms < limit ? ms : limit) * XB_NS_PER_MS;
  return 1;
}

/* The integer of argument `k` (a number, truncated toward zero, saturated), or the argument error `code` of `what`:
 * 0. */
static int integer_arg(xb_ctx *ctx, const char *what, int32_t code, uint32_t k, uint32_t argc, xb_item *argv,
                       int64_t *n) {
  if (k < argc && xb_is_number(&argv[k])) {
    *n = xb_num_to_i64(&argv[k]);
    return 1;
  }
  arg_error(ctx, code, what, argc, argv);
  return 0;
}

/* The program's end from any thread: every other context is asked to QUIT. */
static void quit_all(const xb_ctx *ctx) {
  xb_atomic_store32(&ending, 1, XB_SEQ_CST);
  xb_gc_raise_all(ctx, XB_PENDING_QUIT);
}

void xb_threads_end(xb_ctx *ctx) {
  if (xb_atomic_load32(&running, XB_ACQUIRE)) quit_all(ctx);
  for (uint32_t seen; (seen = xb_atomic_load32(&running, XB_ACQUIRE)) != 0;)
    xb_park_wait_blocking(&running, seen, WAIT_LOOK_NS);
  xb_atomic_store32(&ending, 0, XB_SEQ_CST); /* none runs (a QUIT ended them too): threads may start again */
}

/* Threads: the result, whether it ended, the block and its arguments (an array, NIL once it ended), and the thread's
 * xb_thread_self. */

enum { T_RESULT, T_DONE, T_WORK, T_ID, T_COUNT_ };
static const xb_class_var thread_vars[] = {
  {"(RESULT)", XB_VAR_ANY}, {"(DONE)", XB_VAR_ANY}, {"(WORK)", XB_VAR_ANY}, {"(ID)", XB_VAR_ANY}};
_Static_assert(XB_TABLE_SIZE(thread_vars) == T_COUNT_, "one T_ index per Thread variable");

/* oThread:Join(): waits for the thread to end, and returns what its block returned. */
static void thread_join(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  if (xb_atomic_load32(word_of(&argv[0], T_ID), XB_RELAXED) == xb_thread_self()) {
    xb_error_described(ctx, r, EG_ARG, CODE_THREAD_JOIN, "Thread joins itself", "THREAD:JOIN", XB_ERR_SUBSTITUTE,
                       argc, argv);
    return;
  }
  if (!wait_while(ctx, &argv[0], T_DONE, 0)) return;
  xb_object_get(argv[0].v.p, T_RESULT, r);
}

/* oThread:IsAlive(): whether the thread still runs. */
static void thread_is_alive(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  xb_set_logical(r, xb_atomic_load32(word_of(&argv[0], T_DONE), XB_ACQUIRE) == 0);
}

static const xb_class_method thread_methods[] = {{"JOIN", thread_join}, {"ISALIVE", thread_is_alive}};
static const xb_class thread_class = {
  "THREAD", T_COUNT_, thread_vars, NULL, XB_TABLE_SIZE(thread_methods), thread_methods};

/* What a new thread takes from its creator, which waits until it has (`ready`): the creator's stack keeps the thread
 * object in place meanwhile. */
typedef struct start {
  xb_item thread;
  xb_ctx *creator;
  uint32_t ready;
} start;

/* The thread's end: its result (NIL when a BREAK or a QUIT ended it; a QUIT, or an error nobody handled, ends the
 * program), then the joiners woken. */
static void finish(xb_ctx *ctx, xb_item *thread, xb_item *result) {
  uint32_t ended = xb_pending_get(ctx) & XB_PENDING_UNWIND;
  if (ended & XB_PENDING_QUIT) quit_all(ctx);
  if (ended) {
    xb_pending_clear(ctx, XB_PENDING_UNWIND);
    xb_set_nil(result);
  }
  xb_item nil;
  xb_set_nil(&nil);
  xb_object_put(thread->v.p, T_WORK, &nil);
  xb_object_put(thread->v.p, T_RESULT, result);
  xb_atomic_store32(word_of(thread, T_DONE), 1, XB_RELEASE);
  xb_park_wake(word_of(thread, T_DONE), 1);
}

static void run(start *s) {
  xb_ctx *ctx = xb_ctx_new();
  const char *low, *high;
  if (xb_platform_stack_bounds(&low, &high)) xb_ctx_set_stack(ctx, low);
  xb_env_inherit(ctx, s->creator);
  xb_item thread = s->thread, work, result;
  xb_set_nil(&result);
  xb_gc_root_push(ctx, &thread);
  xb_gc_root_push(ctx, &result);
  xb_atomic_store32(word_of(&thread, T_ID), xb_thread_self(), XB_RELAXED);
  if (xb_atomic_load32(&ending, XB_SEQ_CST)) xb_pending_set(ctx, XB_PENDING_QUIT); /* registered before the look */
  xb_atomic_store32(&s->ready, 1, XB_RELEASE);
  xb_park_wake(&s->ready, 1); /* by address alone: the creator may be gone, with `s` */

  xb_object_get(thread.v.p, T_WORK, &work);
  uint32_t n = (uint32_t)xb_array_length(work.v.p);
  xb_item *args = malloc(n * sizeof *args); /* the block, then its arguments: copies, which Eval may change */
  if (!args) xb_gc_fatal("out of memory");
  for (uint32_t k = 0; k < n; ++k) {
    xb_array_get(work.v.p, k, &args[k]);
    xb_gc_root_push(ctx, &args[k]);
  }
  if (!(xb_pending_get(ctx) & XB_PENDING_QUIT)) xbf_EVAL(ctx, &result, n, args);
  xb_gc_root_pop(ctx, n);
  free(args);
  finish(ctx, &thread, &result);
  xb_gc_root_pop(ctx, 2);
  xb_ctx_free(ctx);
}

/* The thread's first function: its frame lies above every frame the program's code has (gc.c scans from it). */
static void thread_main(void *p) {
  xb_gc_thread_attach((const void *)&p); /* the second thread makes the program's storage shared */
  run(p);
  xb_gc_thread_detach();
  if (xb_atomic_add32(&running, (uint32_t)-1, XB_ACQ_REL) == 1) xb_park_wake(&running, 1);
}

/* ThreadStart( bWork, [xArgs...] ): a Thread running Eval( bWork, xArgs... ). */
void xbf_THREADSTART(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (!argc || (argv[0].type != XB_T_BLOCK && argv[0].type != XB_T_SYMBOL)) {
    xb_fn_arg_error(ctx, ret, CODE_THREADSTART, "THREADSTART", argc, argv);
    return;
  }
  xb_set_object(ret, xb_object_new(&thread_class)); /* rooted there from now on */
  xb_array *work = xb_array_alloc(argc);
  memcpy(work->items, argv, argc * sizeof *argv);
  xb_gc_write_items(work, 0, argc);
  xb_object *t = ret->v.p;
  xb_set_array(&t->vars[T_WORK], work);
  xb_set_logical(&t->vars[T_DONE], 0);
  xb_set_logical(&t->vars[T_ID], 0);
  xb_gc_write(t);
  xb_share(ret); /* the thread, the block and its arguments: the new thread reaches them */
  xb_env_share(ctx);
  start s = {*ret, ctx, 0};
  xb_atomic_add32(&running, 1, XB_ACQ_REL);
  xb_thread h;
  if (!xb_thread_start(&h, THREAD_STACK, thread_main, &s)) {
    xb_atomic_add32(&running, (uint32_t)-1, XB_ACQ_REL);
    xb_set_nil(ret); /* no thread: nothing to join */
    xb_error_described(ctx, NULL, EG_CREATE, CODE_THREAD_CREATE, "Cannot start a thread", "THREADSTART", 0, 0, NULL);
    return;
  }
  while (!xb_atomic_load32(&s.ready, XB_ACQUIRE)) /* blocked: its attach stops every other thread */
    xb_park_wait_blocking(&s.ready, 0, -1);
  xb_thread_detach(h);
}

/* Mutexes: the words of a recursive lock (xb_rwords, core/sync.h) in the object's variables. */

enum { M_STATE, M_OWNER, M_DEPTH, M_COUNT_ };
static const xb_class_var mutex_vars[] = {{"(STATE)", XB_VAR_ANY}, {"(OWNER)", XB_VAR_ANY}, {"(DEPTH)", XB_VAR_ANY}};
_Static_assert(XB_TABLE_SIZE(mutex_vars) == M_COUNT_, "one M_ index per Mutex variable");

static xb_rwords mutex_words(const xb_item *m) {
  return (xb_rwords){word_of(m, M_STATE), word_of(m, M_OWNER), word_of(m, M_DEPTH)};
}

/* The lock taken by this thread, waiting while another thread holds it: 1; 0 when a QUIT ended the wait. */
static int mutex_take(xb_ctx *ctx, const xb_item *m) {
  if (xb_rwords_try(mutex_words(m))) return 1;
  while (!xb_rwords_claim(mutex_words(m)))
    if (!wait_while(ctx, m, M_STATE, 2)) return 0;
  return 1;
}

static int is_mutex(const xb_item *it);

/* oMutex:Lock(): takes the lock, waiting while another thread holds it (or until a QUIT ends the wait). */
static void mutex_lock(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  xb_set_nil(r);
  mutex_take(ctx, &argv[0]);
}

/* oMutex:TryLock(): takes the lock when no other thread holds it: whether it did. */
static void mutex_try_lock(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  xb_set_logical(r, xb_rwords_try(mutex_words(&argv[0])));
}

/* oMutex:Unlock(): releases the lock once; an error when this thread does not hold it. */
static void mutex_unlock(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  xb_set_nil(r);
  if (!xb_rwords_release(mutex_words(&argv[0])))
    xb_error_described(ctx, NULL, EG_ARG, CODE_MUTEX_UNLOCK, "Mutex not held", "MUTEX:UNLOCK", 0, argc, argv);
}

/* Holders: what oMutex:Hold() returns, its Dispose() releasing the mutex once (USING, SYNCHRONIZED). */

enum { H_MUTEX, H_COUNT_ };
static const xb_class_var holder_vars[] = {{"(MUTEX)", XB_VAR_ANY}};
_Static_assert(XB_TABLE_SIZE(holder_vars) == H_COUNT_, "one H_ index per holder variable");

/* oHolder:Dispose(): the mutex released once, by the thread holding it; nothing after, nor from another thread. */
static void holder_dispose(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  xb_set_nil(r);
  xb_item m, nil;
  xb_object_get(argv[0].v.p, H_MUTEX, &m);
  if (!is_mutex(&m) || !xb_rwords_release(mutex_words(&m))) return; /* released already, or not this thread's */
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
    xb_rwords_release(mutex_words(&argv[0]));
    xb_set_nil(r);
  }
}

static const xb_class_method mutex_methods[] = {
  {"LOCK", mutex_lock}, {"TRYLOCK", mutex_try_lock}, {"UNLOCK", mutex_unlock}, {"HOLD", mutex_hold}};
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
  for (uint32_t k = 0; k < M_COUNT_; ++k) xb_set_logical(&m->vars[k], 0);
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
  if (C_WAITING(before) == C_FIELD) xb_park_wake(word_of(c, C_WAITERS), 0);
}

/* A waiter leaves: with its signal when one is there (1), else without (0). */
static int condition_leave(const xb_item *c) {
  uint32_t *word = word_of(c, C_WAITERS), w = xb_atomic_load32(word, XB_ACQUIRE);
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
  uint32_t *word = word_of(c, C_WAITERS), w = xb_atomic_load32(word, XB_RELAXED);
  while (C_WAITING(w) != C_FIELD)
    if (xb_atomic_cas32(word, &w, w + C_WAITER, XB_ACQ_REL)) return 1;
  return 0;
}

/* oCondition:Wait( oMutex, [nMs] ): releases oMutex, which this thread holds (however many times it took it), waits
 * for a Notify() (or NotifyAll()) or nMs milliseconds, takes oMutex back as it was: whether it was notified. A QUIT
 * ends the wait without the mutex. A thread that finds no place among the waiters waits for one with the mutex
 * released, then returns .F. as if it had timed out: its caller tests its condition again. */
static void condition_wait(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  int64_t deadline;
  xb_set_nil(r);
  if (argc < 2 || !is_mutex(&argv[1]) ||
      xb_atomic_load32(word_of(&argv[1], M_OWNER), XB_RELAXED) != xb_thread_self()) {
    xb_error_described(ctx, NULL, EG_ARG, CODE_CONDITION, "Mutex not held", "CONDITION:WAIT", 0, argc, argv);
    return;
  }
  if (!deadline_arg(ctx, "CONDITION:WAIT", CODE_CONDITION, 2, argc, argv, &deadline)) return;
  int joined = condition_join(&argv[0]); /* before the mutex goes: a Notify() after it finds this waiter */
  uint32_t depth = *word_of(&argv[1], M_DEPTH);
  *word_of(&argv[1], M_DEPTH) = 1;
  xb_rwords_release(mutex_words(&argv[1]));
  int notified = 0, ended = WAIT_CHANGED;
  if (!joined) {
    while (ended == WAIT_CHANGED) {
      uint32_t w = xb_atomic_load32(word_of(&argv[0], C_WAITERS), XB_ACQUIRE);
      if (C_WAITING(w) != C_FIELD) break;
      ended = wait_until(ctx, &argv[0], C_WAITERS, w, deadline);
    }
  } else {
    while (ended == WAIT_CHANGED) {
      uint32_t generation = xb_atomic_load32(word_of(&argv[0], C_GENERATION), XB_ACQUIRE);
      uint32_t *word = word_of(&argv[0], C_WAITERS), w = xb_atomic_load32(word, XB_ACQUIRE);
      if (C_SIGNALS(w) && xb_atomic_cas32(word, &w, w - C_WAITER - C_SIGNAL, XB_ACQ_REL)) {
        condition_left(&argv[0], w);
        notified = 1;
        break;
      }
      if (C_SIGNALS(w)) continue; /* another waiter took one meanwhile: look again */
      ended = wait_until(ctx, &argv[0], C_GENERATION, generation, deadline);
    }
    if (!notified) notified = condition_leave(&argv[0]); /* a signal given as it gave up is its own */
  }
  if (ended == WAIT_QUIT || !mutex_take(ctx, &argv[1])) return;
  *word_of(&argv[1], M_DEPTH) = depth;
  xb_set_logical(r, notified);
}

/* Signals for `wanted` more of the waiters that have none (all of them: UINT32_MAX), then the waiters woken. */
static void condition_signal(const xb_item *c, uint32_t wanted, int all) {
  uint32_t *word = word_of(c, C_WAITERS), w = xb_atomic_load32(word, XB_ACQUIRE), given;
  do {
    uint32_t idle = C_WAITING(w) - C_SIGNALS(w);
    given = idle < wanted ? idle : wanted;
    if (!given) return;
  } while (!xb_atomic_cas32(word, &w, w + given * C_SIGNAL, XB_ACQ_REL));
  xb_atomic_add32(word_of(c, C_GENERATION), 1, XB_RELEASE);
  xb_park_wake(word_of(c, C_GENERATION), all);
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

/* Semaphores: the count of free places and the most it may reach. */

enum { S_COUNT, S_MAXIMUM, S_COUNT_ };
static const xb_class_var semaphore_vars[] = {{"(COUNT)", XB_VAR_ANY}, {"(MAXIMUM)", XB_VAR_ANY}};
_Static_assert(XB_TABLE_SIZE(semaphore_vars) == S_COUNT_, "one S_ index per Semaphore variable");

#define SEMAPHORE_MAX INT32_MAX

/* oSemaphore:Wait( [nMs] ): takes a place, waiting up to nMs milliseconds (without: as long as it takes) while none
 * is free: whether it took one. */
static void semaphore_wait(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  int64_t deadline;
  xb_set_nil(r);
  if (!deadline_arg(ctx, "SEMAPHORE:WAIT", CODE_SEMAPHORE, 1, argc, argv, &deadline)) return;
  for (;;) {
    uint32_t *word = word_of(&argv[0], S_COUNT), n = xb_atomic_load32(word, XB_ACQUIRE);
    if (n) {
      if (!xb_atomic_cas32(word, &n, n - 1, XB_ACQ_REL)) continue;
      if (n > 1) xb_park_wake(word, 0); /* places left: the next waiter takes one (Release() wakes only from 0) */
      break;
    }
    int ended = wait_until(ctx, &argv[0], S_COUNT, 0, deadline);
    if (ended == WAIT_QUIT) return;
    if (ended == WAIT_TIMED_OUT) {
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
    if (!integer_arg(ctx, "SEMAPHORE:RELEASE", CODE_SEMAPHORE, 1, argc, argv, &n)) return;
    if (n < 1) {
      arg_error(ctx, CODE_SEMAPHORE, "SEMAPHORE:RELEASE", argc, argv);
      return;
    }
  }
  uint32_t *word = word_of(&argv[0], S_COUNT), before = xb_atomic_load32(word, XB_ACQUIRE);
  uint32_t maximum = xb_atomic_load32(word_of(&argv[0], S_MAXIMUM), XB_RELAXED);
  do {
    if (n > (int64_t)maximum - (int64_t)before) {
      xb_error_described(ctx, NULL, EG_LIMIT, CODE_SEMAPHORE_FULL, "Semaphore full", "SEMAPHORE:RELEASE", 0, argc,
                         argv);
      return;
    }
  } while (!xb_atomic_cas32(word, &before, before + (uint32_t)n, XB_ACQ_REL));
  if (!before) xb_park_wake(word, n > 1);
  xb_set_int(r, before);
}

/* oSemaphore:Count(): the places free now. */
static void semaphore_count(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  xb_set_int(r, xb_atomic_load32(word_of(&argv[0], S_COUNT), XB_ACQUIRE));
}

static const xb_class_method semaphore_methods[] = {
  {"WAIT", semaphore_wait}, {"RELEASE", semaphore_release}, {"COUNT", semaphore_count}};
static const xb_class semaphore_class = {
  "SEMAPHORE", S_COUNT_, semaphore_vars, NULL, XB_TABLE_SIZE(semaphore_methods), semaphore_methods};

/* Semaphore( nInitial, [nMaximum] ): a semaphore with nInitial places free of nMaximum (without it: 2^31 - 1); a count
 * outside 0 .. 2^31 - 1, a maximum below 1 or below nInitial is an argument error. */
void xbf_SEMAPHORE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  int64_t initial, maximum = SEMAPHORE_MAX;
  if (!integer_arg(ctx, "SEMAPHORE", CODE_SEMAPHORE, 0, argc, argv, &initial)) return;
  if (argc > 1 && argv[1].type != XB_T_NIL && !integer_arg(ctx, "SEMAPHORE", CODE_SEMAPHORE, 1, argc, argv, &maximum))
    return;
  if (initial < 0 || maximum < 1 || maximum > SEMAPHORE_MAX || initial > maximum) {
    arg_error(ctx, CODE_SEMAPHORE, "SEMAPHORE", argc, argv);
    return;
  }
  xb_object *s = xb_object_new(&semaphore_class);
  xb_set_logical(&s->vars[S_COUNT], 0);
  xb_set_logical(&s->vars[S_MAXIMUM], 0);
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
  if (!integer_arg(ctx, "ATOMICINT:VALUE", CODE_ATOMICINT, 1, argc, argv, &n[0])) return;
  atomic_step(&argv[0], store, n);
  xb_set_int(r, n[0]);
}

/* oAtomic:Add( n ): adds n (an integer) and returns the new value. */
static void atomic_add(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  int64_t n[1];
  xb_set_nil(r);
  if (!integer_arg(ctx, "ATOMICINT:ADD", CODE_ATOMICINT, 1, argc, argv, &n[0])) return;
  xb_set_int(r, (int64_t)((uint64_t)atomic_step(&argv[0], add, n) + (uint64_t)n[0]));
}

/* oAtomic:CompareExchange( nExpected, nNew ): nNew stored when the value is nExpected: whether it was. */
static void atomic_compare_exchange(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  int64_t n[2];
  xb_set_nil(r);
  if (!integer_arg(ctx, "ATOMICINT:COMPAREEXCHANGE", CODE_ATOMICINT, 1, argc, argv, &n[0]) ||
      !integer_arg(ctx, "ATOMICINT:COMPAREEXCHANGE", CODE_ATOMICINT, 2, argc, argv, &n[1]))
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
  if (argc && argv[0].type != XB_T_NIL && !integer_arg(ctx, "ATOMICINT", CODE_ATOMICINT, 0, argc, argv, &n)) return;
  xb_object *a = xb_object_new(&atomic_class);
  xb_set_int(&a->vars[A_VALUE], n);
  xb_set_object(ret, a);
}
