/* What the runtime's threads and tasks share (rtl/threads.c, rtl/tasks.c): their error codes, the words of their
 * objects that threads wait on, waits with a limit, and the checks of their arguments. */
#ifndef XB_FLOWS_H
#define XB_FLOWS_H

#include "xbrt_int.h"

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
  XB_CODE_TASK_CANCELED
};

/* The word of an object's own variable `k`: its 32-bit value (v.l), changed atomically. An object moves only while
 * every thread is stopped, and one a waiter waits on is pinned by its stack: the address is taken again after any
 * wait or allocation. */
static inline uint32_t *xb_object_word(const xb_item *object, uint32_t k) {
  return (uint32_t *)&((xb_object *)object->v.p)->vars[k].v.l;
}

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

/* The argument error `code` of `what` (a function or "CLASS:METHOD"), not substitutable. */
void xb_flow_arg_error(xb_ctx *ctx, int32_t code, const char *what, uint32_t argc, xb_item *argv);

/* The deadline of a wait of argument `k` milliseconds (NIL, missing or negative: none, -1) into `*deadline`; anything
 * but a number or NIL is the argument error `code` of `what`: 0. */
int xb_deadline_arg(xb_ctx *ctx, const char *what, int32_t code, uint32_t k, uint32_t argc, xb_item *argv,
                    int64_t *deadline);

/* The integer of argument `k` (a number, truncated toward zero, saturated), or the argument error `code` of `what`:
 * 0. */
int xb_integer_arg(xb_ctx *ctx, const char *what, int32_t code, uint32_t k, uint32_t argc, xb_item *argv, int64_t *n);

#endif
