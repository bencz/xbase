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
  XB_CODE_MUTEX_OUTER
};

/* The word of an object's own variable `k`: its 32-bit value (v.l), changed atomically. An object moves only while
 * every thread is stopped, and one a waiter waits on is pinned by its stack: the address is taken again after any
 * wait or allocation. */
static inline uint32_t *xb_object_word(const xb_item *object, uint32_t k) {
  return (uint32_t *)&((xb_object *)object->v.p)->vars[k].v.l;
}

/* A recursive lock (xb_rwords, core/sync.h) over the words of an object's variables k, k + 1 and k + 2 (a Mutex's, a
 * Channel's). xb_object_lock takes it for the flow (xb_flow_self), waiting as Lock() does: 0 when a QUIT or the
 * program's end ended the wait, or when a flow the running one runs inside holds it (a wait that could never end:
 * BASE/9031 raised). */
xb_rwords xb_object_rwords(const xb_item *object, uint32_t k);
int xb_object_lock(xb_ctx *ctx, const xb_item *object, uint32_t k);
/* xb_object_lock for a lock held a moment (a Channel's), owned by the thread: no QUIT ends the wait, which keeps the
 * worker's place. */
void xb_object_lock_short(const xb_item *object, uint32_t k);

/* How a wait ended. */
enum { XB_WAIT_QUIT = -1, XB_WAIT_TIMED_OUT, XB_WAIT_CHANGED };

/* Waits while the word of `object`'s variable `k` is `value`, until the monotonic clock reaches `deadline` (< 0: no
 * limit), a while at a time in a blocking operation (collections go on without the waiter), so that a QUIT finds it,
 * and the program's end (code an ALWAYS runs while a QUIT is set aside waits no more). `object` is an item that keeps
 * the object. xb_wait_while: no limit, 1 once the word changed, 0 for a QUIT or the program's end. */
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
/* Whether a task ended (completed, failed or canceled). */
int xb_task_is_final(const xb_item *task);
/* Whether the item is a CancellationToken, or NIL (none). */
int xb_task_token_ok(const xb_item *token);
int xb_task_settle(xb_ctx *ctx, const xb_item *task, int ok, const xb_item *value);
void xb_task_completed(xb_ctx *ctx, xb_item *r, const xb_item *value);
/* Waits for a task to end (no limit): 1 with its result in `*out` when it completed; 0 when it failed, was canceled,
 * or a QUIT ended the wait. */
int xb_task_wait_result(xb_ctx *ctx, const xb_item *task, xb_item *out);

#endif
