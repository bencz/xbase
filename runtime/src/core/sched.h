/* The scheduler of tasks (core/sched.c, docs/concurrency.md#scheduler): a pool of worker threads that run the tasks
 * submitted to it (rtl/tasks.c runs each one: xb_task_run). */
#ifndef XB_SCHED_H
#define XB_SCHED_H

#include "core/context.h"

/* Schedules `task` (an object of the Task class, shared, its state scheduled): on the calling worker's own queue, the
 * next it runs when `next`, else on the pool's; workers are woken or started as the work needs. */
void xb_sched_submit(xb_object *task, int next);

/* Whether the calling thread is a worker running a task. */
int xb_sched_in_worker(void);

/* xb_blocking_run's: a worker running a task enters (1) or leaves (0) an operation that may block it; while it is
 * blocked and work waits, another worker takes its place (blocking handoff). */
void xb_sched_blocking(int entering);

/* The program ends (xb_quit_all): the workers and the timers' thread stop. xb_sched_end, once they ended
 * (xb_threads_end): the timers left fire as the program ends them (xb_task_timer), the tasks that never started are
 * canceled (xb_task_cancel), and the pool and the timers may start again. */
void xb_sched_stop(void);
void xb_sched_end(xb_ctx *ctx);

/* A timer: at the monotonic time `deadline`, the timers' thread fires `target` (an object of the Task class:
 * xb_task_timer). */
void xb_timer_add(xb_object *target, int64_t deadline);

/* The number of workers that run tasks at once (//WORKERS:n; 0: one per processor the program may run on). */
void xb_sched_set_workers(unsigned n);
/* The workers alive now, the number that run tasks at once (the target) and the most there may be (TaskWorkers()). */
void xb_sched_workers(uint32_t *alive, uint32_t *runAtOnce, uint32_t *most);

/* The tasks the queues hold, for the collector (every other thread stopped): `mark` may move them. */
void xb_sched_mark(int (*mark)(xb_item *));

/* rtl/tasks.c: runs the task in `*task` (rooted) on `ctx` when it is still to start, else nothing; cancels one that
 * will never run. */
void xb_task_run(xb_ctx *ctx, xb_item *task);
void xb_task_cancel(xb_ctx *ctx, xb_item *task);

/* rtl/tasks.c: the timer of `*target` (rooted) set for `deadline` is due; with `ending`, the program ends first: a
 * delay or a timeout is canceled, a cancellation source canceled. xb_task_timer_dead: whether a timer for `target`
 * (under the timers' lock, no safepoint) is needed no more: its target settled, or a later CancelAfter replaced it. */
void xb_task_timer(xb_ctx *ctx, xb_item *target, int64_t deadline, int ending);
int xb_task_timer_dead(void *target, int64_t deadline);

/* The program's start procedure returned `*value`: a Task (an asynchronous start procedure's) is waited for, and its
 * failure or cancellation raised as Result raises them (the default error handler reports an error and ends the
 * program). */
void xb_task_await_start(xb_ctx *ctx, const xb_item *value);

#endif
