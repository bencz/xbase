/* Tasks (docs/concurrency.md#tasks): TaskRun() and ContinueWith() run a block on the scheduler's workers
 * (core/sched.c); TaskCompletionSource() makes a task code completes; TaskWhenAll() and TaskWhenAny() complete with
 * others; TaskFromResult() is one completed already.
 *
 * A task is an object of the Task class, shared from the start. Its state word goes from created (a continuation, a
 * combinator, a promise) or scheduled to running (claimed with a compare-and-swap, by a worker or by a thread that
 * waits for it and runs it inline), then to one final state; waiters park on it. Completing takes the task's stripe
 * lock: the result and the state are stored, and the list of continuations (nodes {target, next}, pushed under the
 * same lock) is taken; then the continuations fire: a continuation is scheduled on the completing worker, next; a
 * combinator counts. */
#include "rtl/flows.h"

#include "core/sched.h"
#include "core/shared.h"
#include "core/sync.h"

#include <string.h>

void xbf_BREAK(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

enum { S_CREATED, S_SCHEDULED, S_RUNNING, S_COMPLETED, S_FAILED, S_CANCELED };
#define FINAL(s) ((s) >= S_COMPLETED)

/* What completes it: its block (TaskRun, ContinueWith), code (a promise), or the tasks it waits for. */
enum { K_RUN, K_PROMISE, K_WHEN_ALL, K_WHEN_ANY };

/* The variables: the state and the kind (words), the result (or the failure's value), the block and its arguments
 * and the environment's snapshot (until it runs), the continuations, a combinator's count (when all: the tasks still
 * to complete; when any: whether one did) and the tasks it waits for. */
enum { V_STATE, V_KIND, V_RESULT, V_WORK, V_ENV, V_NEXT, V_COUNT, V_TASKS, V_COUNT_ };
static const xb_class_var task_vars[] = {
  {"(STATE)", XB_VAR_ANY}, {"(KIND)", XB_VAR_ANY}, {"(RESULT)", XB_VAR_ANY}, {"(WORK)", XB_VAR_ANY},
  {"(ENV)", XB_VAR_ANY},   {"(NEXT)", XB_VAR_ANY}, {"(COUNT)", XB_VAR_ANY},  {"(TASKS)", XB_VAR_ANY}};
_Static_assert(XB_TABLE_SIZE(task_vars) == V_COUNT_, "one V_ index per Task variable");

static const xb_class task_class;

static int is_task(const xb_item *it) {
  return it->type == XB_T_OBJECT && ((const xb_object *)it->v.p)->cls == &task_class;
}

static uint32_t state_of(const xb_item *task) { return xb_atomic_load32(xb_object_word(task, V_STATE), XB_ACQUIRE); }

/* A new task in `*r` (rooted), local until it is shared, of `kind`, in `state`. */
static xb_object *task_new(xb_item *r, uint32_t kind, uint32_t state) {
  xb_object *t = xb_object_new(&task_class);
  xb_set_object(r, t);
  xb_set_logical(&t->vars[V_STATE], 0);
  xb_set_logical(&t->vars[V_KIND], 0);
  xb_set_logical(&t->vars[V_COUNT], 0);
  t->vars[V_STATE].v.l = (int32_t)state;
  t->vars[V_KIND].v.l = (int32_t)kind;
  return t;
}

/* The task's final state and result, set once: 1, with the continuations it had in `*next` (rooted by the caller);
 * 0 when it was final already. */
static int finish(const xb_item *task, uint32_t state, const xb_item *value, xb_item *next) {
  if (FINAL(state_of(task))) return 0;
  xb_object *o = task->v.p;
  xb_item nil;
  xb_set_nil(&nil);
  xb_share(value);
  xb_seq_lock(o);
  if (FINAL(xb_atomic_load32(xb_object_word(task, V_STATE), XB_RELAXED))) {
    xb_seq_unlock(o);
    return 0;
  }
  *next = o->vars[V_NEXT]; /* the writers hold the lock */
  xb_seq_store(&o->vars[V_RESULT], value);
  xb_seq_store(&o->vars[V_NEXT], &nil);
  xb_seq_store(&o->vars[V_WORK], &nil);
  xb_seq_store(&o->vars[V_ENV], &nil);
  xb_gc_write(o);
  xb_atomic_store32(xb_object_word(task, V_STATE), state, XB_RELEASE);
  xb_seq_unlock(o);
  xb_park_wake(xb_object_word(task, V_STATE), 1);
  return 1;
}

/* What completions fire is run from a stack of frames {list, antecedent, below} (local arrays), not by recursion: a
 * combinator that completes pushes its own continuations, so a chain of them runs in constant stack. */
enum { F_LIST, F_ANTECEDENT, F_BELOW, F_COUNT_ };

static void push_list(xb_item *stack, const xb_item *list, const xb_item *antecedent) {
  if (list->type != XB_T_ARRAY) return;
  xb_array *f = xb_array_alloc(F_COUNT_); /* `f`, on the stack, stays where it is */
  f->items[F_LIST] = *list;
  f->items[F_ANTECEDENT] = *antecedent;
  f->items[F_BELOW] = *stack;
  xb_gc_write(f);
  xb_set_array(stack, f);
}

/* A combinator when all its tasks completed: failed with the first failure (in their order), else canceled when one
 * was, else completed with their results (an array); its continuations pushed on `stack`. */
static void when_all_done(xb_ctx *ctx, const xb_item *combinator, xb_item *stack) {
  xb_item tasks, results, it, next;
  xb_set_nil(&results);
  xb_set_nil(&next);
  xb_object_get(combinator->v.p, V_TASKS, &tasks);
  xb_gc_root_push(ctx, &tasks);
  xb_gc_root_push(ctx, &results);
  xb_gc_root_push(ctx, &next);
  size_t n = xb_array_length(tasks.v.p);
  uint32_t outcome = S_COMPLETED;
  for (size_t k = 0; k < n && outcome != S_FAILED; ++k) {
    xb_array_get(tasks.v.p, k, &it);
    uint32_t s = state_of(&it);
    if (s == S_FAILED) {
      xb_object_get(it.v.p, V_RESULT, &results);
      outcome = S_FAILED;
    } else if (s == S_CANCELED) {
      outcome = S_CANCELED;
    }
  }
  if (outcome == S_COMPLETED) {
    xb_array *a = xb_array_alloc(n); /* `a`, on the stack, stays where it is */
    xb_set_array(&results, a);
    for (size_t k = 0; k < n; ++k) {
      xb_array_get(tasks.v.p, k, &it);
      xb_object_get(it.v.p, V_RESULT, &a->items[k]);
    }
    xb_gc_write_items(a, 0, n);
  } else if (outcome == S_CANCELED) {
    xb_set_nil(&results);
  }
  if (finish(combinator, outcome, &results, &next)) push_list(stack, &next, combinator);
  xb_gc_root_pop(ctx, 3);
}

/* `target` fires as `antecedent` completed: a continuation is scheduled (to run next on this worker), a combinator
 * counts, or completes; what that completion fires goes on `stack`. */
static void fire(xb_ctx *ctx, const xb_item *target, const xb_item *antecedent, xb_item *stack) {
  switch ((uint32_t)xb_atomic_load32(xb_object_word(target, V_KIND), XB_RELAXED)) {
  case K_RUN: {
    uint32_t created = S_CREATED;
    if (xb_atomic_cas32(xb_object_word(target, V_STATE), &created, S_SCHEDULED, XB_ACQ_REL))
      xb_sched_submit(target->v.p, 1);
    break;
  }
  case K_WHEN_ALL:
    if (xb_atomic_add32(xb_object_word(target, V_COUNT), (uint32_t)-1, XB_ACQ_REL) == 1)
      when_all_done(ctx, target, stack);
    break;
  case K_WHEN_ANY: {
    uint32_t none = 0;
    xb_item next;
    xb_set_nil(&next);
    xb_gc_root_push(ctx, &next);
    if (xb_atomic_cas32(xb_object_word(target, V_COUNT), &none, 1, XB_ACQ_REL) &&
        finish(target, S_COMPLETED, antecedent, &next))
      push_list(stack, &next, target);
    xb_gc_root_pop(ctx, 1);
    break;
  }
  default:
    break;
  }
}

/* Fires, one node at a time, what the frames of `*stack` (rooted) hold, until it is empty. */
static void drain(xb_ctx *ctx, xb_item *stack) {
  xb_item target, antecedent;
  xb_set_nil(&target);
  xb_set_nil(&antecedent);
  xb_gc_root_push(ctx, &target);
  xb_gc_root_push(ctx, &antecedent);
  while (stack->type == XB_T_ARRAY) {
    xb_array *f = stack->v.p; /* local: read and written in place */
    if (f->items[F_LIST].type != XB_T_ARRAY) {
      *stack = f->items[F_BELOW];
      continue;
    }
    xb_item node = f->items[F_LIST];
    xb_array_get(node.v.p, 0, &target);
    xb_array_get(node.v.p, 1, &f->items[F_LIST]);
    xb_gc_write(f);
    antecedent = f->items[F_ANTECEDENT];
    fire(ctx, &target, &antecedent, stack);
  }
  xb_gc_root_pop(ctx, 2);
}

/* The task's final state and result, once, and what that fires: 0 when it was final already. */
static int complete(xb_ctx *ctx, const xb_item *task, uint32_t state, const xb_item *value) {
  xb_item next, stack;
  xb_set_nil(&next);
  xb_set_nil(&stack);
  xb_gc_root_push(ctx, &next);
  xb_gc_root_push(ctx, &stack);
  int done = finish(task, state, value, &next);
  if (done) {
    push_list(&stack, &next, task);
    drain(ctx, &stack);
  }
  xb_gc_root_pop(ctx, 2);
  return done;
}

/* `target` fires when `antecedent` completes; at once when it has. Finished combinators at the head of the list are
 * left out of the new node (a TaskWhenAny over a task that lives long, called again and again, would keep them). */
static void add_continuation(xb_ctx *ctx, const xb_item *antecedent, const xb_item *target) {
  xb_item node, head, kept, stack;
  xb_set_nil(&node);
  xb_set_nil(&kept);
  xb_set_nil(&stack);
  xb_gc_root_push(ctx, &node);
  xb_gc_root_push(ctx, &kept);
  xb_gc_root_push(ctx, &stack);
  for (;;) {
    xb_object_get(antecedent->v.p, V_NEXT, &head);
    if (FINAL(state_of(antecedent))) break;
    kept = head;
    for (xb_item t; kept.type == XB_T_ARRAY && xb_array_get(kept.v.p, 0, &t) && FINAL(state_of(&t));)
      xb_array_get(kept.v.p, 1, &kept);
    xb_array *a = xb_array_alloc(2); /* `a`, on the stack, stays where it is; `head` too */
    xb_set_array(&node, a);
    a->items[0] = *target;
    a->items[1] = kept;
    xb_gc_write(a);
    xb_share(&node);
    xb_object *o = antecedent->v.p;
    xb_seq_lock(o);
    int pushed = !FINAL(xb_atomic_load32(xb_object_word(antecedent, V_STATE), XB_RELAXED)) &&
                 o->vars[V_NEXT].type == head.type && o->vars[V_NEXT].v.p == head.v.p;
    if (pushed) {
      xb_seq_store(&o->vars[V_NEXT], &node);
      xb_gc_write(o);
    }
    xb_seq_unlock(o);
    if (pushed) {
      xb_gc_root_pop(ctx, 3);
      return;
    }
  }
  fire(ctx, target, antecedent, &stack);
  drain(ctx, &stack);
  xb_gc_root_pop(ctx, 3);
}

/* Runs a task claimed (running) on `ctx`: in an environment of its own, made from the snapshot its creator took, with
 * no Type() trap of the waiter's around it (its errors are its own). A failure (an error the default handler gave
 * up on, XB_PENDING_FAIL) or a BREAK that leaves its block fails it with the value; a QUIT ends the program, and leaves
 * it canceled. */
static void execute(xb_ctx *ctx, const xb_item *task) {
  xb_item work, env, result;
  xb_object_get(task->v.p, V_WORK, &work);
  xb_object_get(task->v.p, V_ENV, &env);
  xb_set_nil(&result);
  xb_gc_root_push(ctx, &work);
  xb_gc_root_push(ctx, &env);
  xb_gc_root_push(ctx, &result);
  uint32_t trap = ctx->error_trap;
  int32_t trapped = ctx->error_trapped;
  ctx->error_trap = 0;
  xb_env_enter(ctx, &env);
  ++ctx->task_depth;
  xb_eval_work(ctx, &result, &work);
  xb_env_leave(ctx); /* its tables closed: errors there are still the task's */
  --ctx->task_depth;
  ctx->error_trap = trap;
  ctx->error_trapped = trapped;
  uint32_t ended = xb_pending_get(ctx) & XB_PENDING_UNWIND;
  if (ended & XB_PENDING_QUIT) {
    xb_quit_all(ctx);
    xb_pending_clear(ctx, XB_PENDING_UNWIND & ~(uint32_t)XB_PENDING_QUIT);
    xb_set_nil(&ctx->break_value);
    xb_set_nil(&result);
    complete(ctx, task, S_CANCELED, &result); /* the QUIT stays: the thread ends, or unwinds */
  } else if (ended) {
    result = ctx->break_value;
    xb_set_nil(&ctx->break_value);
    xb_pending_clear(ctx, ended);
    complete(ctx, task, S_FAILED, &result);
  } else {
    complete(ctx, task, S_COMPLETED, &result);
  }
  xb_gc_root_pop(ctx, 3);
}

/* The task claimed (scheduled to running) by this thread: 1, and it runs it. */
static int claim(const xb_item *task) {
  uint32_t scheduled = S_SCHEDULED;
  return xb_atomic_cas32(xb_object_word(task, V_STATE), &scheduled, S_RUNNING, XB_ACQ_REL);
}

void xb_task_run(xb_ctx *ctx, xb_item *task) {
  if (claim(task)) execute(ctx, task);
}

void xb_task_cancel(xb_ctx *ctx, xb_item *task) {
  if (!claim(task)) return;
  xb_item nil;
  xb_set_nil(&nil);
  complete(ctx, task, S_CANCELED, &nil);
}

/* Waits for the task to end: one still to start runs on this thread (inline) when there is no limit. The
 * outcome of the wait (XB_WAIT_*). */
static int wait_task(xb_ctx *ctx, const xb_item *task, int64_t deadline) {
  if (deadline < 0 && state_of(task) == S_SCHEDULED && claim(task)) execute(ctx, task);
  for (uint32_t s; !FINAL(s = state_of(task));) {
    int ended = xb_wait_until(ctx, task, V_STATE, s, deadline);
    if (ended != XB_WAIT_CHANGED) return ended;
  }
  return XB_WAIT_CHANGED;
}

/* oTask:Wait( [nMs] ): waits for the task to end (up to nMs milliseconds): whether it did. */
static void task_wait(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  int64_t deadline;
  xb_set_nil(r);
  if (!xb_deadline_arg(ctx, "TASK:WAIT", XB_CODE_TASK, 1, argc, argv, &deadline)) return;
  int ended = wait_task(ctx, &argv[0], deadline);
  if (ended != XB_WAIT_QUIT && !(xb_pending_get(ctx) & XB_PENDING_UNWIND)) xb_set_logical(r, ended == XB_WAIT_CHANGED);
}

/* The outcome of a task that ended, for whoever waited for it: its result; a failure that carries an Error object
 * raised again, through the waiter's error block (whose result `r` takes); one that carries another value as
 * Break( value ); a cancellation as the error "Task canceled". */
static void outcome(xb_ctx *ctx, xb_item *r, const xb_item *task, const char *operation) {
  uint32_t s = state_of(task);
  xb_item value;
  xb_object_get(task->v.p, V_RESULT, &value);
  if (s == S_COMPLETED) {
    *r = value;
  } else if (s == S_FAILED && xb_error_is(&value)) {
    xb_gc_root_push(ctx, &value);
    xb_set_nil(r);
    xb_error_raise(ctx, r, &value);
    xb_gc_root_pop(ctx, 1);
  } else if (s == S_FAILED) {
    xb_set_nil(r);
    xbf_BREAK(ctx, r, 1, &value);
  } else {
    xb_set_nil(r);
    xb_error_described(ctx, r, 0, XB_CODE_TASK_CANCELED, "Task canceled", operation, XB_ERR_SUBSTITUTE, 1, task);
  }
}

/* oTask:Result: waits for the task to end; its result (a failure or a cancellation raised: `outcome`). */
static void task_result(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  xb_set_nil(r);
  if (wait_task(ctx, &argv[0], -1) == XB_WAIT_CHANGED && !(xb_pending_get(ctx) & XB_PENDING_UNWIND))
    outcome(ctx, r, &argv[0], "TASK:RESULT");
}

static void task_is_completed(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  xb_set_logical(r, FINAL(state_of(&argv[0])));
}

static void task_is_failed(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  xb_set_logical(r, state_of(&argv[0]) == S_FAILED);
}

static void task_is_canceled(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  xb_set_logical(r, state_of(&argv[0]) == S_CANCELED);
}

/* oTask:Error: the value its failure carries (the error, for one an error ended); NIL when it did not fail. */
static void task_error(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  xb_set_nil(r);
  if (state_of(&argv[0]) == S_FAILED) xb_object_get(argv[0].v.p, V_RESULT, r);
}

/* A task of `kind` (a block's: K_RUN) in `*ret`, with the work array of argv[0 .. argc - 1] (the block first) and the
 * snapshot of the caller's environment, shared. */
static void run_task_new(xb_ctx *ctx, xb_item *ret, uint32_t state, uint32_t argc, const xb_item *argv) {
  xb_object *t = task_new(ret, K_RUN, state); /* `t`, on the stack, stays where it is */
  xb_array *work = xb_array_alloc(argc);
  memcpy(work->items, argv, argc * sizeof *argv);
  xb_gc_write_items(work, 0, argc);
  xb_set_array(&t->vars[V_WORK], work);
  xb_gc_write(t);
  xb_item env;
  xb_set_nil(&env);
  xb_gc_root_push(ctx, &env);
  xb_env_capture(ctx, &env);
  t->vars[V_ENV] = env;
  xb_gc_write(t);
  xb_gc_root_pop(ctx, 1);
  xb_share(ret);
}

/* oTask:ContinueWith( bWork ): a task that runs Eval( bWork, oTask ) once oTask ends. */
static void task_continue_with(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  xb_set_nil(r);
  if (argc < 2 || !xb_is_work(&argv[1])) {
    xb_flow_arg_error(ctx, XB_CODE_TASK, "TASK:CONTINUEWITH", argc, argv);
    return;
  }
  xb_item work[2] = {argv[1], argv[0]};
  run_task_new(ctx, r, S_CREATED, 2, work);
  add_continuation(ctx, &argv[0], r);
}

static const xb_class_method task_methods[] = {
  {"RESULT", task_result},       {"WAIT", task_wait},   {"ISCOMPLETED", task_is_completed},
  {"ISFAILED", task_is_failed},  {"ISCANCELED", task_is_canceled}, {"ERROR", task_error},
  {"CONTINUEWITH", task_continue_with}};
static const xb_class task_class = {"TASK", V_COUNT_, task_vars, NULL, XB_TABLE_SIZE(task_methods), task_methods};

/* TaskRun( bWork, [xArgs...] ): a task running Eval( bWork, xArgs... ) on the scheduler's workers. */
void xbf_TASKRUN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (!argc || !xb_is_work(&argv[0])) {
    xb_flow_arg_error(ctx, XB_CODE_TASK, "TASKRUN", argc, argv);
    return;
  }
  run_task_new(ctx, ret, S_SCHEDULED, argc, argv);
  xb_sched_submit(ret->v.p, xb_sched_in_worker());
}

/* TaskFromResult( xValue ): a task completed with xValue. */
void xbf_TASKFROMRESULT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  xb_object *t = task_new(ret, K_PROMISE, S_COMPLETED);
  if (argc) t->vars[V_RESULT] = argv[0];
  xb_gc_write(t);
  xb_share(ret);
}

/* The combinators: a task of `kind` over the tasks of array argv[0] (each one a Task, else an argument error). */
static void combinator(xb_ctx *ctx, xb_item *ret, uint32_t kind, const char *what, uint32_t argc, xb_item *argv) {
  if (!argc || argv[0].type != XB_T_ARRAY) {
    xb_flow_arg_error(ctx, XB_CODE_TASK, what, argc, argv);
    return;
  }
  xb_item tasks, it;
  xb_set_array(&tasks, xb_array_snapshot(argv[0].v.p)); /* checked and used as one read saw it */
  xb_gc_root_push(ctx, &tasks);
  size_t n = xb_array_length(tasks.v.p);
  int ok = (n || kind == K_WHEN_ALL) && n < UINT32_MAX;
  for (size_t k = 0; k < n && ok; ++k) ok = xb_array_get(tasks.v.p, k, &it) && is_task(&it);
  if (!ok) {
    xb_gc_root_pop(ctx, 1);
    xb_flow_arg_error(ctx, XB_CODE_TASK, what, argc, argv);
    return;
  }
  xb_object *t = task_new(ret, kind, S_CREATED); /* `t`, on the stack, stays where it is */
  t->vars[V_TASKS] = tasks;
  t->vars[V_COUNT].v.l = kind == K_WHEN_ALL ? (int32_t)n : 0;
  xb_gc_write(t);
  xb_share(ret);
  if (!n) {
    xb_item stack;
    xb_set_nil(&stack);
    xb_gc_root_push(ctx, &stack);
    when_all_done(ctx, ret, &stack);
    drain(ctx, &stack);
    xb_gc_root_pop(ctx, 2);
    return;
  }
  for (size_t k = 0; k < n; ++k) {
    xb_array_get(tasks.v.p, k, &it);
    xb_gc_root_push(ctx, &it);
    add_continuation(ctx, &it, ret);
    xb_gc_root_pop(ctx, 1);
  }
  xb_gc_root_pop(ctx, 1);
}

/* TaskWhenAll( aTasks ): a task that completes once all of aTasks did, with their results (an array, in their order);
 * failed with the first failure, canceled when one was. */
void xbf_TASKWHENALL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  combinator(ctx, ret, K_WHEN_ALL, "TASKWHENALL", argc, argv);
}

/* TaskWhenAny( aTasks ): a task that completes once one of aTasks (not empty) did, with that task as its result. */
void xbf_TASKWHENANY(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  combinator(ctx, ret, K_WHEN_ANY, "TASKWHENANY", argc, argv);
}

/* Task completion sources: the promise task they complete. */

enum { P_TASK, P_COUNT_ };
static const xb_class_var source_vars[] = {{"(TASK)", XB_VAR_ANY}};
_Static_assert(XB_TABLE_SIZE(source_vars) == P_COUNT_, "one P_ index per TaskCompletionSource variable");

static void source_task(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  xb_object_get(argv[0].v.p, P_TASK, r);
}

/* Completes the source's task in `state` with argv[1] (NIL without it): whether it did; `strict`: an error when it had
 * ended already. */
static void source_set(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv, uint32_t state, int strict,
                       const char *what) {
  xb_item task, value;
  xb_object_get(argv[0].v.p, P_TASK, &task);
  xb_gc_root_push(ctx, &task);
  if (argc > 1 && state != S_CANCELED) value = argv[1];
  else xb_set_nil(&value);
  int done = complete(ctx, &task, state, &value);
  xb_gc_root_pop(ctx, 1);
  xb_set_logical(r, done);
  if (!done && strict)
    xb_error_described(ctx, NULL, EG_ARG, XB_CODE_TASK_COMPLETED, "Task already completed", what, 0, argc, argv);
}

static void source_set_result(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  source_set(ctx, r, argc, argv, S_COMPLETED, 1, "TASKCOMPLETIONSOURCE:SETRESULT");
}
static void source_set_error(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  source_set(ctx, r, argc, argv, S_FAILED, 1, "TASKCOMPLETIONSOURCE:SETERROR");
}
static void source_set_canceled(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  source_set(ctx, r, argc, argv, S_CANCELED, 1, "TASKCOMPLETIONSOURCE:SETCANCELED");
}
static void source_try_set_result(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  source_set(ctx, r, argc, argv, S_COMPLETED, 0, NULL);
}
static void source_try_set_error(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  source_set(ctx, r, argc, argv, S_FAILED, 0, NULL);
}
static void source_try_set_canceled(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  source_set(ctx, r, argc, argv, S_CANCELED, 0, NULL);
}

static const xb_class_method source_methods[] = {
  {"TASK", source_task},
  {"SETRESULT", source_set_result},
  {"SETERROR", source_set_error},
  {"SETCANCELED", source_set_canceled},
  {"TRYSETRESULT", source_try_set_result},
  {"TRYSETERROR", source_try_set_error},
  {"TRYSETCANCELED", source_try_set_canceled}};
static const xb_class source_class = {
  "TASKCOMPLETIONSOURCE", P_COUNT_, source_vars, NULL, XB_TABLE_SIZE(source_methods), source_methods};

/* TaskCompletionSource(): a source whose task (:Task) its Set...() methods complete. */
void xbf_TASKCOMPLETIONSOURCE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_item task;
  xb_set_nil(&task);
  xb_gc_root_push(ctx, &task);
  task_new(&task, K_PROMISE, S_CREATED);
  xb_share(&task);
  xb_object *s = xb_object_new(&source_class);
  s->vars[P_TASK] = task;
  xb_gc_write(s);
  xb_set_object(ret, s);
  xb_gc_root_pop(ctx, 1);
}

/* __TaskFail( xValue ): in a task, fails it with xValue (the default error handler's, for an error nobody handled): a
 * request that unwinds to the task's end, which no sequence takes: .T.; elsewhere nothing, .F. */
void xbf___TASKFAIL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  int in_task = ctx->task_depth > 0;
  xb_set_logical(ret, in_task);
  if (!in_task) return;
  if (argc) ctx->break_value = argv[0];
  else xb_set_nil(&ctx->break_value);
  xb_pending_set(ctx, XB_PENDING_FAIL);
}

void xb_task_await_start(xb_ctx *ctx, const xb_item *value) {
  if (!is_task(value)) return;
  xb_item task = *value, result;
  xb_set_nil(&result);
  xb_gc_root_push(ctx, &task);
  xb_gc_root_push(ctx, &result);
  if (wait_task(ctx, &task, -1) == XB_WAIT_CHANGED && !(xb_pending_get(ctx) & XB_PENDING_UNWIND))
    outcome(ctx, &result, &task, "MAIN");
  xb_gc_root_pop(ctx, 2);
}
