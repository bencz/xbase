/* bench/work_stealing.prg in C: fork-join over an unbalanced tree (as the unbalanced tree search benchmark): the root
 * and the next four levels have 8 children each, every node below has 5 children or none as its hash says (2,859,034
 * nodes in all), each node hashed 40 times. The four levels below the root are tasks, 4,680 of them, on a pool of one
 * thread per processor that takes them from one queue under a mutex (C has no pool of its own; this one has no queue
 * per thread to steal from); a task waits for its children's results in turn: one no thread has taken yet runs inline,
 * and while one runs elsewhere the waiting thread runs queued tasks. The levels below run inside the task. The node
 * count and the sum of the hashes must be the tree's, whatever the scheduling. */
#include "bench.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_TASKS 8192

enum { QUEUED, RUNNING, DONE };

typedef struct {
  int64_t s, d, count, sum;
  atomic_int state;
} Task;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER; /* a task queued or ended, or the pool closing */
static Task *queue[MAX_TASKS]; /* every task submitted: those taken stay until the end, when all are freed */
static int head, tail, closing;

static int64_t hash(int64_t s) {
  for (int i = 1; i <= 40; ++i) s = s * 16807 % 2147483647;
  return s;
}

static void visit(int64_t s, int64_t d, int64_t *count, int64_t *sum) {
  int64_t h = hash(s);
  ++*count;
  *sum += h % 1000;
  if (d < 5 || h % 1000 < 198)
    for (int64_t k = 1; k <= (d < 5 ? 8 : 5); ++k) visit((s * 16807 + k * 48271) % 2147483647, d + 1, count, sum);
}

static void branch(int64_t s, int64_t d, int64_t *count, int64_t *sum);

/* Runs t when no thread has taken it yet. */
static int run(Task *t) {
  int expected = QUEUED;
  if (!atomic_compare_exchange_strong(&t->state, &expected, RUNNING)) return 0;
  branch(t->s, t->d, &t->count, &t->sum);
  pthread_mutex_lock(&lock);
  atomic_store(&t->state, DONE);
  pthread_cond_broadcast(&changed);
  pthread_mutex_unlock(&lock);
  return 1;
}

static Task *submit(int64_t s, int64_t d) {
  Task *t = malloc(sizeof *t);
  t->s = s;
  t->d = d;
  t->count = t->sum = 0;
  atomic_init(&t->state, QUEUED);
  pthread_mutex_lock(&lock);
  queue[tail++] = t;
  pthread_cond_broadcast(&changed);
  pthread_mutex_unlock(&lock);
  return t;
}

/* The next queued task, or NULL; with wait, waits for one until the pool closes or stop says. */
static Task *take(int wait, const Task *stop) {
  pthread_mutex_lock(&lock);
  while (wait && head == tail && !closing && !(stop && atomic_load(&stop->state) == DONE))
    pthread_cond_wait(&changed, &lock);
  Task *t = head < tail ? queue[head++] : NULL;
  pthread_mutex_unlock(&lock);
  return t;
}

/* Waits for t's result: runs it inline when no thread has taken it, runs queued tasks while it runs elsewhere. */
static void join(Task *t) {
  if (run(t)) return;
  while (atomic_load(&t->state) != DONE) {
    Task *other = take(1, t);
    if (other) run(other);
  }
}

static void branch(int64_t s, int64_t d, int64_t *count, int64_t *sum) {
  if (d >= 4) {
    visit(s, d, count, sum);
    return;
  }
  int64_t h = hash(s), n = 0, total = 0;
  Task *tasks[8];
  for (int64_t k = 1; k <= 8; ++k) tasks[k - 1] = submit((s * 16807 + k * 48271) % 2147483647, d + 1);
  for (int k = 8; k >= 1; --k) {
    join(tasks[k - 1]);
    n += tasks[k - 1]->count;
    total += tasks[k - 1]->sum;
  }
  *count = n + 1;
  *sum = total + h % 1000;
}

static void *worker(void *arg) {
  (void)arg;
  Task *t;
  while ((t = take(1, NULL)) != NULL) run(t);
  return NULL;
}

int main(void) {
  long long t = bench_ms();
  long cpus = sysconf(_SC_NPROCESSORS_ONLN);
  int nworkers = cpus > 0 ? (int)cpus : 1;
  pthread_t *th = malloc((size_t)nworkers * sizeof *th);
  for (int i = 0; i < nworkers; ++i) pthread_create(&th[i], NULL, worker, NULL);
  int64_t count = 0, sum = 0;
  branch(19937, 0, &count, &sum);
  bench_report("work_stealing", t, count == 2859034 && sum == 1428166988);
  pthread_mutex_lock(&lock);
  closing = 1;
  pthread_cond_broadcast(&changed);
  pthread_mutex_unlock(&lock);
  for (int i = 0; i < nworkers; ++i) pthread_join(th[i], NULL);
  for (int i = 0; i < tail; ++i) free(queue[i]);
  free(th);
  return 0;
}
