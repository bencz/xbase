/* What the runtime's threads and tasks share (rtl/threads.c, rtl/tasks.c): their error codes, the words of their
 * objects that threads wait on, waits with a limit, and the checks of their arguments. */
#ifndef XB_FLOWS_H
#define XB_FLOWS_H

#include "xbrt_int.h"

#include "core/sync.h"

/* xbase's own error codes of these functions (9001 to 9015 are the bit and character functions'). */
enum {
  XB_CODE_THREADSTART = 9016,
  XB_CODE_THREAD_CREATE,
  XB_CODE_THREAD_JOIN,
  XB_CODE_MUTEX_UNLOCK,
  XB_CODE_ATOMICINT,
  XB_CODE_CONDITION,
  XB_CODE_SEMAPHORE,
  XB_CODE_SEMAPHORE_FULL,
  XB_CODE_TASK,
  XB_CODE_TASK_COMPLETED,
  XB_CODE_TASK_CANCELED,
  XB_CODE_TASK_TIMEOUT,
  XB_CODE_OPERATION_CANCELED,
  XB_CODE_CHANNEL,
  XB_CODE_CHANNEL_CLOSED,
  XB_CODE_MUTEX_OUTER,
  XB_CODE_ENUMERATOR_WAITING = 9035 /* after 9032 (Memory low) and the CHARACTER methods' (types.h) */
};

/* The word of an object's own variable `k`: its 32-bit value (v.l), changed atomically. An object moves only while
 * every thread is stopped, and one a waiter waits on is pinned by its stack: the address is taken again after any
 * wait or allocation. */
static inline uint32_t *xb_object_word(const xb_item *object, uint32_t k) {
  return (uint32_t *)&((xb_object *)object->v.p)->vars[k].v.l;
}

/* The same for a 64-bit value (v.i, 8-aligned in every item): a lock's holder (xb_rwords), a thread's number. */
static inline uint64_t *xb_object_word64(const xb_item *object, uint32_t k) {
  return (uint64_t *)&((xb_object *)object->v.p)->vars[k].v.i;
}

/* A recursive lock (xb_rwords, core/sync.h) over the words of an object's variables k, k + 1 and k + 2 (a Mutex's, a
 * Channel's). xb_object_lock takes it for the flow (xb_flow_self), waiting as Lock() does: 0 when a QUIT or the
 * program's end ended the wait, or when a flow the running one runs inside holds it (a wait that could never end:
 * BASE/9031 raised). */
xb_rwords xb_object_rwords(const xb_item *object, uint32_t k);
int xb_object_lock(xb_ctx *ctx, const xb_item *object, uint32_t k);
/* A lock held a moment (a Channel's) in the word of variable k: never taken again by its holder, so the word alone
 * (0 free, 1 held, 2 maybe waited for): one compare-and-swap to take it, one exchange to release it, loads before its
 * waiter sleeps on the word (as a short wait, which keeps the worker's place); no QUIT ends the wait. */
void xb_object_lock_short(const xb_item *object, uint32_t k);
void xb_object_unlock_short(const xb_item *object, uint32_t k);

/* An object's variable `k` that holds a 32-bit count (written with xb_set_int), read without its lock only where the
 * caller allows a stale value. */
static inline uint32_t xb_object_u32(const xb_item *o, uint32_t k) {
  return (uint32_t)((const xb_object *)o->v.p)->vars[k].v.i;
}

/* Rings (rtl/waiters.c): a queue in three variables of a runtime object (its array, its head, its count), read and
 * written only under the object's short lock. The arrays are the collector's and shared with the object: only the
 * lock's holder reads or writes them, in place. A ring grows when full (twice as large; first the elements its `gone`
 * test finds needless are dropped, the test told so through `arg` and called once for each, and when that freed less
 * than half of it, it grows all the same, so that waiters canceled again and again cost what they are), and shrinks to
 * half once a quarter of it is used. xb_ring_at is the element k places from the head (k below the count),
 * xb_ring_set replaces it; xb_ring_task_gone is the test of a ring of waiting tasks (an ended one, canceled or timed
 * out, needs its place no more), xb_ring_pair_gone of a ring of arrays whose first item is such a task. */
typedef struct xb_ring {
  uint32_t items, head, count;
} xb_ring;
typedef int (*xb_ring_gone)(void *arg, const xb_item *element);
void xb_ring_init(xb_object *o, xb_ring g);
static inline uint32_t xb_ring_count(const xb_item *o, xb_ring g) { return xb_object_u32(o, g.count); }
static inline const xb_item *xb_ring_at(const xb_item *o, xb_ring g, uint32_t k) {
  const xb_array *a = ((const xb_object *)o->v.p)->vars[g.items].v.p;
  return &xb_array_items(a)[(xb_object_u32(o, g.head) + k) % a->len];
}
void xb_ring_set(const xb_item *o, xb_ring g, uint32_t k, const xb_item *v);
void xb_ring_push(const xb_item *o, xb_ring g, const xb_item *v, xb_ring_gone gone, void *arg);
void xb_ring_pop(const xb_item *o, xb_ring g, xb_item *out);
int xb_ring_task_gone(void *arg, const xb_item *task);
int xb_ring_pair_gone(void *arg, const xb_item *pair);

/* Waiting tasks claimed under an object's lock (xb_task_claim: a token cancels them no more) and completed once it is
 * released (xb_claims_settle: wakes and continuations run there, never under the lock): pairs {task, value} in an
 * array (`list`, NIL until a claim), completed with their values (failed with them, `fail`). `held` keeps what a
 * ring gave up until it is in the list: popped, a waiter is reachable from nowhere else, and the allocations that
 * follow may collect. xb_claims_root roots both (two roots, the caller pops them with its own). */
typedef struct xb_claims {
  xb_item list;
  xb_item held;
  uint32_t n;
  int fail;
} xb_claims;
void xb_claims_root(xb_ctx *ctx, xb_claims *c);
void xb_claims_add(xb_claims *c, const xb_item *task, const xb_item *value);
void xb_claims_settle(xb_ctx *ctx, xb_claims *c);

/* How a wait ended. */
enum { XB_WAIT_QUIT = -1, XB_WAIT_TIMED_OUT, XB_WAIT_CHANGED };

/* Waits while the word of `object`'s variable `k` is `value`, until the monotonic clock reaches `deadline` (< 0: no
 * limit), in a blocking operation (collections go on without the waiter); a QUIT wakes it (the context's waitingOn),
 * and so does the program's end for code an ALWAYS runs while a QUIT is set aside (others wait for the QUIT that comes
 * to them: they leave with it pending). `object` is an item that keeps the object. xb_wait_while: no limit, 1 once the
 * word changed, 0 for a QUIT or the program's end. */
int xb_wait_until(xb_ctx *ctx, const xb_item *object, uint32_t k, uint32_t value, int64_t deadline);
int xb_wait_while(xb_ctx *ctx, const xb_item *object, uint32_t k, uint32_t value);

/* Whether the item is work a thread or a task runs: a codeblock or a function reference. */
static inline int xb_is_work(const xb_item *it) { return it->type == XB_T_BLOCK || it->type == XB_T_SYMBOL; }

/* The monotonic time `ms` milliseconds from now (clamped far enough that nothing overflows). */
static inline int64_t xb_deadline_after_ms(int64_t ms) {
  int64_t limit = INT64_MAX / 2 / XB_NS_PER_MS;
  return xb_platform_monotonic_ns() + (ms < 0 ? 0 : ms < limit ? ms : limit) * XB_NS_PER_MS;
}

/* The argument error `code` of `what` (a function or "CLASS:METHOD"), not substitutable. */
void xb_flow_arg_error(xb_ctx *ctx, int32_t code, const char *what, uint32_t argc, xb_item *argv);

/* The deadline of a wait of argument `k` milliseconds (NIL, missing or negative: none, -1) into `*deadline`; anything
 * but a number or NIL is the argument error `code` of `what`: 0. */
int xb_deadline_arg(xb_ctx *ctx, const char *what, int32_t code, uint32_t k, uint32_t argc, xb_item *argv,
                    int64_t *deadline);

/* The integer of argument `k` (a number, truncated toward zero, saturated), or the argument error `code` of `what`:
 * 0. */
int xb_integer_arg(xb_ctx *ctx, const char *what, int32_t code, uint32_t k, uint32_t argc, xb_item *argv, int64_t *n);

/* Tasks for the runtime's other classes (rtl/tasks.c). xb_task_promise: a new task in `*r`, shared, that code
 * completes (xb_task_settle) and that `*token` cancels first (NIL: none): 0, with the argument error of `what`, when
 * the token is no token. xb_task_settle: completes it with `value` (`ok`), or fails it with that value: 0 when it had
 * ended already (canceled). xb_task_completed: a task completed with `value` (with .T., one task shared by all). */
int xb_task_promise(xb_ctx *ctx, xb_item *r, const xb_item *token, const char *what, uint32_t argc, xb_item *argv);
/* xb_task_waiter: a waiter's task in `*r`, shared (a Mutex's LockAsync, a Semaphore's WaitAsync): code grants it
 * (xb_task_claim, then xb_task_settle_claimed); `*token` cancels it first (NIL: none), and at `deadline` (< 0: none)
 * it completes with `*timed_out` unless it was claimed by then. 0, with the argument error of `what`, when the token
 * is no token. */
int xb_task_waiter(xb_ctx *ctx, xb_item *r, const xb_item *token, int64_t deadline, const xb_item *timed_out,
                   const char *what, uint32_t argc, xb_item *argv);
/* Whether a task ended (completed, failed or canceled). */
int xb_task_is_final(const xb_item *task);
/* Whether the item is a CancellationToken, or NIL (none). */
int xb_task_token_ok(const xb_item *token);
int xb_task_settle(xb_ctx *ctx, const xb_item *task, int ok, const xb_item *value);
/* A promise taken to settle later (outside a lock, say): 1 when it was waiting, and no token cancels it from now on;
 * 0 when it had ended (canceled). xb_task_settle_claimed then completes it (`ok`) or fails it with `value`. */
int xb_task_claim(const xb_item *task);
/* A claim given back (the claimer settles nothing after all): 1, the promise waits again, its token may cancel it; 0
 * when its token asked to cancel it meanwhile: it stays claimed, and the claimer cancels it (xb_task_cancel_claimed).
 */
int xb_task_unclaim(const xb_item *task);
void xb_task_cancel_claimed(xb_ctx *ctx, const xb_item *task);
void xb_task_settle_claimed(xb_ctx *ctx, const xb_item *task, int ok, const xb_item *value);
void xb_task_completed(xb_ctx *ctx, xb_item *r, const xb_item *value);
/* Whether the runtime method running is the target of an AWAIT that fused its send, taken first thing (what it runs
 * after, a block, sees none): such a method that ends at once answers xb_task_answer( fused ): the value itself, which
 * that AWAIT takes as its result, instead of a task completed already (xb_task_completed). */
int xb_await_method(xb_ctx *ctx);
void xb_task_answer(xb_ctx *ctx, xb_item *r, const xb_item *value, int fused);
/* Waits for a task to end (no limit): 1 with its result in `*out` when it completed; 0 when it failed, was canceled,
 * or a QUIT ended the wait. */
int xb_task_wait_result(xb_ctx *ctx, const xb_item *task, xb_item *out);

/* Asynchronous generators (rtl/agen.c): the body runs as an asynchronous function's steps (rtl/tasks.c) whose step
 * record knows the enumerator `gen`. xb_async_gen_start runs its first step on the caller's thread, the arguments
 * `argv` (stable while it runs); at a YIELD (xb_async_yield) the body's task parks, waiting for nothing, and
 * xb_agen_yield takes `*value` as the element and the task `*task`, which the next MoveNextAsync() resumes
 * (xb_task_resume: its step at once on this thread when `here` (the caller found room on the stack), 1; else
 * scheduled on a worker, 0). A body's task canceled before it ran again (the program's end) tells its enumerator.
 * xb_agen_step_awaits, before a step that suspends at an AWAIT is registered (nothing can resume it yet), and
 * xb_agen_step_ended, after every step's own end, tell the enumerator how it ended (XB_AGEN_*; `*failure` the value of
 * one that failed). */
enum { XB_AGEN_YIELDED = 1, XB_AGEN_AWAITING, XB_AGEN_ENDED, XB_AGEN_FAILED, XB_AGEN_CANCELED };
void xb_async_gen_start(xb_ctx *ctx, const xb_item *gen, xb_func resume, uint32_t argc, const xb_item *argv);
void xb_agen_yield(xb_ctx *ctx, const xb_item *gen, const xb_item *value, const xb_item *task);
void xb_agen_step_awaits(xb_ctx *ctx, const xb_item *gen);
int xb_task_resume(xb_ctx *ctx, const xb_item *task, int here);
void xb_agen_step_ended(xb_ctx *ctx, const xb_item *gen, int outcome, const xb_item *failure);

#endif
