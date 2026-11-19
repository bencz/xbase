/* bench/tasks.prg in C: tasks on a pool of one thread per processor that takes them from one queue under a mutex (C
 * has no pool of its own): 200,000 short tasks (a function and its argument) started from the program's thread and
 * joined (the joiner runs those no thread has taken yet, the newest first), then a 50,000-link chain of continuations
 * (each queued when the one before ends) and 20,000 tasks that each start and wait for another (run inline when no
 * thread has taken it yet). Every task is kept until the end, when all are freed. */
#include "bench.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <unistd.h>

enum { WAITING, QUEUED, RUNNING, DONE };

typedef struct Task Task;
struct Task {
  int64_t (*fn)(Task *self);
  int64_t arg, result;
  Task *parent, *continuations, *sibling; /* a continuation: the task it follows; the continuations of a task */
  atomic_int state;
};

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t work = PTHREAD_COND_INITIALIZER, ended = PTHREAD_COND_INITIALIZER;
static Task **queue, **all;
static int64_t qhead, qcount, qcap, nall, capall, waiters;
static int closing;

/* Under the lock. */
static void enqueue(Task *t) {
  if (qcount == qcap) {
    Task **q = malloc((size_t)(qcap ? qcap * 2 : 1024) * sizeof *q);
    for (int64_t i = 0; i < qcount; ++i) q[i] = queue[(qhead + i) % qcap];
    free(queue);
    queue = q;
    qhead = 0;
    qcap = qcap ? qcap * 2 : 1024;
  }
  atomic_store(&t->state, QUEUED);
  queue[(qhead + qcount++) % qcap] = t;
  pthread_cond_signal(&work);
}

/* Under the lock. */
static Task *new_task(int64_t (*fn)(Task *), int64_t arg) {
  if (nall == capall) {
    capall = capall ? capall * 2 : 1024;
    all = realloc(all, (size_t)capall * sizeof *all);
  }
  Task *t = all[nall++] = calloc(1, sizeof *t);
  t->fn = fn;
  t->arg = arg;
  atomic_init(&t->state, WAITING);
  return t;
}

/* TaskRun( fn, arg ). */
static Task *task_run(int64_t (*fn)(Task *), int64_t arg) {
  pthread_mutex_lock(&lock);
  Task *t = new_task(fn, arg);
  enqueue(t);
  pthread_mutex_unlock(&lock);
  return t;
}

static Task *task_from_result(int64_t x) {
  pthread_mutex_lock(&lock);
  Task *t = new_task(NULL, 0);
  t->result = x;
  atomic_store(&t->state, DONE);
  pthread_mutex_unlock(&lock);
  return t;
}

/* p:ContinueWith( fn ): queued once p has ended. */
static Task *continue_with(Task *p, int64_t (*fn)(Task *)) {
  pthread_mutex_lock(&lock);
  Task *t = new_task(fn, 0);
  t->parent = p;
  if (atomic_load(&p->state) == DONE)
    enqueue(t);
  else {
    t->sibling = p->continuations;
    p->continuations = t;
  }
  pthread_mutex_unlock(&lock);
  return t;
}

/* Runs t when no thread has taken it yet. */
static int run(Task *t) {
  int expected = QUEUED;
  if (!atomic_compare_exchange_strong(&t->state, &expected, RUNNING)) return 0;
  int64_t r = t->fn(t);
  pthread_mutex_lock(&lock);
  t->result = r;
  atomic_store(&t->state, DONE);
  for (Task *c = t->continuations; c; c = c->sibling) enqueue(c);
  if (waiters) pthread_cond_broadcast(&ended);
  pthread_mutex_unlock(&lock);
  return 1;
}

/* t:Result: runs t inline when no thread has taken it, else waits for it. */
static int64_t result(Task *t) {
  if (atomic_load(&t->state) != DONE && !run(t)) {
    pthread_mutex_lock(&lock);
    ++waiters;
    while (atomic_load(&t->state) != DONE) pthread_cond_wait(&ended, &lock);
    --waiters;
    pthread_mutex_unlock(&lock);
  }
  return t->result;
}

/* TaskWhenAll( a ):Result: the tasks still to start run here, the newest first; then each is waited for. */
static void when_all(Task **a, int64_t n, int64_t *out) {
  for (int64_t k = n - 1; k >= 0; --k) run(a[k]);
  for (int64_t k = 0; k < n; ++k) out[k] = result(a[k]);
}

static void *worker(void *arg) {
  (void)arg;
  for (;;) {
    pthread_mutex_lock(&lock);
    while (qcount == 0 && !closing) pthread_cond_wait(&work, &lock);
    if (qcount == 0) {
      pthread_mutex_unlock(&lock);
      return NULL;
    }
    Task *t = queue[qhead];
    qhead = (qhead + 1) % qcap;
    --qcount;
    pthread_mutex_unlock(&lock);
    run(t);
  }
}

static int64_t mod7(Task *self) { return self->arg % 7; }
static int64_t plus_one(Task *self) { return self->parent->result + 1; }
static int64_t one(Task *self) {
  (void)self;
  return 1;
}
static int64_t start_and_wait(Task *self) {
  (void)self;
  return result(task_run(one, 0));
}

int main(void) {
  long long t = bench_ms();
  long cpus = sysconf(_SC_NPROCESSORS_ONLN);
  int nworkers = cpus > 0 ? (int)cpus : 1;
  pthread_t *th = malloc((size_t)nworkers * sizeof *th);
  for (int i = 0; i < nworkers; ++i) pthread_create(&th[i], NULL, worker, NULL);
  int64_t n = 200000, s = 0;
  Task **a = malloc((size_t)n * sizeof *a);
  int64_t *r = malloc((size_t)n * sizeof *r);
  for (int64_t k = 1; k <= n; ++k) a[k - 1] = task_run(mod7, k);
  when_all(a, n, r);
  for (int64_t k = 0; k < n; ++k) s += r[k];
  Task *c = task_from_result(0);
  for (int k = 1; k <= 50000; ++k) c = continue_with(c, plus_one);
  s += result(c);
  for (int k = 1; k <= 20000; ++k) s += result(task_run(start_and_wait, 0));
  bench_report("tasks", t, s == 599997 + 50000 + 20000);
  pthread_mutex_lock(&lock);
  closing = 1;
  pthread_cond_broadcast(&work);
  pthread_mutex_unlock(&lock);
  for (int i = 0; i < nworkers; ++i) pthread_join(th[i], NULL);
  for (int64_t i = 0; i < nall; ++i) free(all[i]);
  free(all);
  free(queue);
  free(a);
  free(r);
  free(th);
  return 0;
}
