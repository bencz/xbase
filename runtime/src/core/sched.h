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

/* The program ends (xb_quit_all): the workers stop. xb_sched_end, once they ended (xb_threads_end): the tasks that
 * never started are canceled (xb_task_cancel), and the pool may start again. */
void xb_sched_stop(void);
void xb_sched_end(xb_ctx *ctx);

/* The number of workers that run tasks at once (//WORKERS:n; 0: one per processor the program may run on). */
void xb_sched_set_workers(unsigned n);

/* The tasks the queues hold, for the collector (every other thread stopped): `mark` may move them. */
void xb_sched_mark(int (*mark)(xb_item *));

/* rtl/tasks.c: runs the task in `*task` (rooted) on `ctx` when it is still to start, else nothing; cancels one that
 * will never run. */
void xb_task_run(xb_ctx *ctx, xb_item *task);
void xb_task_cancel(xb_ctx *ctx, xb_item *task);

/* The program's start procedure returned `*value`: a Task (an asynchronous start procedure's) is waited for, and its
 * failure or cancellation raised as Result raises them (the default error handler reports an error and ends the
 * program). */
void xb_task_await_start(xb_ctx *ctx, const xb_item *value);

#endif
