/* The scheduler of tasks (docs/concurrency.md#scheduler).
 *
 * Workers are program threads (xb_program_thread_start), started when tasks first need them; `target` of them run
 * tasks at once (one per processor, or //WORKERS:n). Each has a queue of its own, a ring of RUNQ tasks that it fills
 * at the tail and takes from at the head, which other workers steal half of at once (one compare-and-swap of the
 * head), and a slot for the task to run next (a task it just made: locality), which a thief takes only when the ring
 * is empty. Tasks from other threads, and those a full ring sheds (half of it), go to the pool's queue, under the
 * pool's lock, which a worker looks at first once every GLOBAL_FIRST tasks.
 *
 * A worker with nothing to run parks on a word of its own, listed as idle under the pool's lock, after it looked once
 * more for work (the count of queued tasks, read after its listing, against a submission that counts its task before
 * it looks for an idle worker: one of the two sees the other). A submission wakes an idle worker, or starts one, while
 * fewer than `target` are active (searching or running, neither parked nor blocked). A worker running a task that
 * enters an operation that may block counts as inactive meanwhile: when work waits, another worker takes its place
 * (blocking handoff), up to WORKERS_MAX; a worker beyond the target that stays idle SURPLUS_IDLE_NS ends.
 *
 * The queues hold object addresses: the collector marks them (xb_sched_mark), every other thread stopped. */
#include "xbrt_int.h"

#include "core/sched.h"
#include "core/sync.h"

#include <stdlib.h>
#include <string.h>

#define RUNQ 256u
#define WORKERS_MAX 1024u
#define GLOBAL_FIRST 61u
#define SURPLUS_IDLE_NS ((int64_t)2000 * XB_NS_PER_MS)

typedef struct worker {
  uint32_t head, tail; /* the ring's: the head taken with compare-and-swap by any, the tail written by its owner */
  void *ring[RUNQ];
  void *next;          /* the task to run next: exchanged */
  uint32_t wake;       /* 1 once a waker took it off the idle list: parked on */
  uint32_t alive;      /* a thread runs it (the pool's lock) */
  uint32_t inTask;     /* running a task: its blocking operations hand its place over */
  uint32_t ticks;
  uint32_t seed;       /* where its search for a victim starts */
  struct worker *idleNext;
} worker;

static worker *workers[WORKERS_MAX];
static uint32_t slots;   /* workers[0 .. slots) made so far (the pool's lock; read atomically) */
static uint32_t total;   /* workers alive */
static uint32_t active;  /* workers searching or running, neither parked nor blocked */
static uint32_t queued;  /* tasks in the queues */
static uint32_t target;  /* workers that run at once; 0 until the pool first starts */
static uint32_t stopping;
static unsigned requested;

static xb_mutex poolLock;
static xb_once_flag poolMade = XB_ONCE_INIT;
static worker *idle;
static void **global; /* the pool's queue: a ring of gCap entries from gHead, gLen of them */
static size_t gHead, gLen, gCap;

static XB_THREAD_LOCAL worker *me;

static void make_pool(void) {
  if (!xb_mutex_init(&poolLock)) xb_gc_fatal("cannot create the scheduler's lock");
  unsigned n = requested ? requested : xb_platform_cpu_count();
  if (n > WORKERS_MAX) n = WORKERS_MAX;
  xb_atomic_store32(&target, n ? n : 1, XB_RELEASE);
}

void xb_sched_set_workers(unsigned n) { requested = n; }

/* The pool's queue (under the pool's lock). */
static void global_put(void *const *tasks, size_t n) {
  if (gLen + n > gCap) {
    size_t cap = gCap ? gCap : 256;
    while (cap < gLen + n) cap *= 2;
    void **grown = malloc(cap * sizeof *grown);
    if (!grown) xb_gc_fatal("out of memory");
    for (size_t k = 0; k < gLen; ++k) grown[k] = global[(gHead + k) % gCap];
    free(global);
    global = grown;
    gCap = cap;
    gHead = 0;
  }
  for (size_t k = 0; k < n; ++k) global[(gHead + gLen + k) % gCap] = tasks[k];
  xb_atomic_storesz(&gLen, gLen + n, XB_RELEASE);
}

static void *global_take(void) {
  void *t = global[gHead];
  gHead = (gHead + 1) % gCap;
  xb_atomic_storesz(&gLen, gLen - 1, XB_RELEASE);
  return t;
}

static int runq_put(worker *w, void *t);

/* A task of the pool's queue for worker `w`, which takes its share of the rest into its own ring, as far as the ring
 * has room: moved after the lock is released (a full ring sheds into the pool's queue, under that lock). */
static void *global_get(worker *w) {
  if (!xb_atomic_loadsz(&gLen, XB_ACQUIRE)) return NULL;
  void *batch[RUNQ / 2];
  size_t share = 0;
  xb_mutex_lock(&poolLock);
  void *t = NULL;
  if (gLen) {
    t = global_take();
    share = gLen / xb_atomic_load32(&total, XB_RELAXED);
    size_t room = RUNQ - (w->tail - xb_atomic_load32(&w->head, XB_ACQUIRE));
    if (share > RUNQ / 2) share = RUNQ / 2;
    if (share > room) share = room;
    for (size_t k = 0; k < share; ++k) batch[k] = global_take();
  }
  xb_mutex_unlock(&poolLock);
  for (size_t k = 0; k < share; ++k) runq_put(w, batch[k]);
  return t;
}

/* The owner's ring. Slots are read and written atomically: a thief may read one the owner writes after the ring
 * wrapped, and finds the head moved then. */
static int runq_put_slow(worker *w, void *t, uint32_t h, uint32_t tl) {
  void *batch[RUNQ / 2 + 1];
  uint32_t n = (tl - h) / 2;
  for (uint32_t k = 0; k < n; ++k) batch[k] = xb_atomic_loadptr(&w->ring[(h + k) % RUNQ], XB_RELAXED);
  if (!xb_atomic_cas32(&w->head, &h, h + n, XB_RELEASE)) return 0; /* thieves took some: there is room now */
  batch[n] = t;
  xb_mutex_lock(&poolLock);
  global_put(batch, n + 1);
  xb_mutex_unlock(&poolLock);
  return 1;
}

static int runq_put(worker *w, void *t) {
  for (;;) {
    uint32_t h = xb_atomic_load32(&w->head, XB_ACQUIRE), tl = w->tail;
    if (tl - h < RUNQ) {
      xb_atomic_storeptr(&w->ring[tl % RUNQ], t, XB_RELAXED);
      xb_atomic_store32(&w->tail, tl + 1, XB_RELEASE);
      return 1;
    }
    if (runq_put_slow(w, t, h, tl)) return 1;
  }
}

static void *runq_get(worker *w) {
  uint32_t h = xb_atomic_load32(&w->head, XB_ACQUIRE);
  for (;;) {
    if (h == w->tail) return NULL;
    void *t = xb_atomic_loadptr(&w->ring[h % RUNQ], XB_RELAXED);
    if (xb_atomic_cas32(&w->head, &h, h + 1, XB_ACQ_REL)) return t;
  }
}

/* Half of victim `v`'s ring moved into thief `w`'s (empty), the last of them returned; or `v`'s next task, when its
 * ring is empty. */
static void *steal_from(worker *w, worker *v) {
  for (;;) {
    uint32_t h = xb_atomic_load32(&v->head, XB_ACQUIRE), tl = xb_atomic_load32(&v->tail, XB_ACQUIRE);
    uint32_t n = tl - h;
    n -= n / 2;
    if (!n) {
      void *t = xb_atomic_loadptr(&v->next, XB_ACQUIRE);
      return t && xb_atomic_casptr(&v->next, &t, NULL, XB_ACQ_REL) ? t : NULL;
    }
    if (n > RUNQ / 2) continue; /* the head and the tail read at different moments */
    uint32_t wt = w->tail;
    for (uint32_t k = 0; k < n; ++k)
      xb_atomic_storeptr(&w->ring[(wt + k) % RUNQ], xb_atomic_loadptr(&v->ring[(h + k) % RUNQ], XB_RELAXED),
                         XB_RELAXED);
    if (!xb_atomic_cas32(&v->head, &h, h + n, XB_ACQ_REL)) continue;
    void *t = xb_atomic_loadptr(&w->ring[(wt + n - 1) % RUNQ], XB_RELAXED);
    if (n > 1) xb_atomic_store32(&w->tail, wt + n - 1, XB_RELEASE);
    return t;
  }
}

static void *steal(worker *w) {
  uint32_t n = xb_atomic_load32(&slots, XB_ACQUIRE);
  w->seed = w->seed * 1103515245u + 12345u;
  uint32_t start = n ? (w->seed >> 16) % n : 0;
  for (uint32_t k = 0; k < n; ++k) {
    worker *v = xb_atomic_loadptr((void *const *)&workers[(start + k) % n], XB_ACQUIRE);
    if (!v || v == w) continue;
    void *t = steal_from(w, v);
    if (t) return t;
  }
  return NULL;
}

static void *find_work(worker *w) {
  void *t = NULL;
  if (++w->ticks % GLOBAL_FIRST == 0) t = global_get(w);
  if (!t) t = xb_atomic_xchgptr(&w->next, NULL, XB_ACQ_REL);
  if (!t) t = runq_get(w);
  if (!t) t = global_get(w);
  if (!t) t = steal(w);
  if (t) xb_atomic_add32(&queued, (uint32_t)-1, XB_RELAXED);
  return t;
}

static void worker_body(xb_ctx *ctx, void *arg);

/* A worker made or reused, its thread started (under the pool's lock): NULL when none can be. */
static worker *spawn_locked(void) {
  if (xb_atomic_load32(&stopping, XB_ACQUIRE) || xb_atomic_load32(&total, XB_RELAXED) >= WORKERS_MAX) return NULL;
  worker *w = NULL;
  for (uint32_t k = 0; k < slots && !w; ++k)
    if (!xb_atomic_load32(&workers[k]->alive, XB_ACQUIRE)) w = workers[k];
  if (!w) {
    w = calloc(1, sizeof *w);
    if (!w) xb_gc_fatal("out of memory");
    w->seed = slots + 1;
    xb_atomic_storeptr((void **)&workers[slots], w, XB_RELEASE);
    xb_atomic_store32(&slots, slots + 1, XB_RELEASE);
  }
  xb_atomic_store32(&w->wake, 0, XB_RELAXED);
  xb_atomic_store32(&w->alive, 1, XB_RELEASE);
  xb_atomic_add32(&total, 1, XB_ACQ_REL);
  xb_atomic_add32(&active, 1, XB_ACQ_REL);
  if (xb_program_thread_start(worker_body, w)) return w;
  xb_atomic_store32(&w->alive, 0, XB_RELEASE);
  xb_atomic_add32(&total, (uint32_t)-1, XB_ACQ_REL);
  xb_atomic_add32(&active, (uint32_t)-1, XB_ACQ_REL);
  return NULL;
}

/* An idle worker woken, or a new one started, for work that waits. */
static void wake_or_spawn(void) {
  xb_mutex_lock(&poolLock);
  worker *w = idle;
  if (w) {
    idle = w->idleNext;
    xb_atomic_add32(&active, 1, XB_ACQ_REL); /* for it: a second submission sees it active */
  } else {
    spawn_locked();
  }
  xb_mutex_unlock(&poolLock);
  if (w) {
    xb_atomic_store32(&w->wake, 1, XB_RELEASE);
    xb_park_wake(&w->wake, 1);
  }
}

/* `w` off the idle list, by itself: 0 when a waker took it off first (it wakes it). */
static int idle_remove(worker *w) {
  xb_mutex_lock(&poolLock);
  worker **p = &idle;
  while (*p && *p != w) p = &(*p)->idleNext;
  int found = *p != NULL;
  if (found) *p = w->idleNext;
  xb_mutex_unlock(&poolLock);
  return found;
}

/* Parks `w`, which found nothing to run: 1 when it is active again, 0 when it is to end (not counted active). */
static int park(worker *w) {
  xb_mutex_lock(&poolLock);
  w->idleNext = idle;
  idle = w;
  xb_atomic_add32(&active, (uint32_t)-1, XB_SEQ_CST);
  xb_mutex_unlock(&poolLock);
  xb_atomic_fence(XB_SEQ_CST);
  if ((xb_atomic_load32(&queued, XB_SEQ_CST) || xb_atomic_load32(&stopping, XB_SEQ_CST)) && idle_remove(w)) {
    if (xb_atomic_load32(&stopping, XB_ACQUIRE)) return 0;
    xb_atomic_add32(&active, 1, XB_ACQ_REL);
    return 1;
  }
  int64_t since = xb_platform_monotonic_ns();
  for (;;) {
    if (xb_atomic_load32(&w->wake, XB_ACQUIRE)) {
      xb_atomic_store32(&w->wake, 0, XB_RELAXED);
      return 1; /* its waker counted it active */
    }
    int surplus = xb_atomic_load32(&total, XB_ACQUIRE) > xb_atomic_load32(&target, XB_ACQUIRE);
    if (surplus && xb_platform_monotonic_ns() - since >= SURPLUS_IDLE_NS && idle_remove(w)) return 0;
    xb_park_wait_blocking(&w->wake, 0, surplus ? SURPLUS_IDLE_NS : -1);
  }
}

static void run_task(xb_ctx *ctx, worker *w, void *t) {
  xb_item task;
  xb_set_ptr(&task, XB_T_OBJECT, t);
  xb_gc_root_push(ctx, &task);
  xb_atomic_store32(&w->inTask, 1, XB_RELAXED);
  xb_task_run(ctx, &task);
  xb_atomic_store32(&w->inTask, 0, XB_RELAXED);
  xb_gc_root_pop(ctx, 1);
}

/* A worker's tasks go to the pool's queue as it ends. */
static void hand_back(worker *w) {
  void *t = xb_atomic_xchgptr(&w->next, NULL, XB_ACQ_REL);
  xb_mutex_lock(&poolLock);
  if (t) global_put(&t, 1);
  while ((t = runq_get(w)) != NULL) global_put(&t, 1);
  xb_mutex_unlock(&poolLock);
}

static void worker_body(xb_ctx *ctx, void *arg) {
  worker *w = arg;
  me = w;
  for (;;) {
    if (xb_atomic_load32(&stopping, XB_ACQUIRE) || (xb_pending_get(ctx) & XB_PENDING_QUIT)) {
      xb_atomic_add32(&active, (uint32_t)-1, XB_ACQ_REL);
      break;
    }
    void *t = find_work(w);
    if (t) run_task(ctx, w, t);
    else if (!park(w)) break;
  }
  hand_back(w);
  me = NULL;
  xb_mutex_lock(&poolLock);
  xb_atomic_store32(&w->alive, 0, XB_RELEASE);
  xb_atomic_add32(&total, (uint32_t)-1, XB_ACQ_REL);
  xb_mutex_unlock(&poolLock);
}

void xb_sched_submit(xb_object *task, int next) {
  xb_once(&poolMade, make_pool);
  xb_atomic_add32(&queued, 1, XB_SEQ_CST);
  worker *w = me;
  if (w && next) {
    void *before = xb_atomic_xchgptr(&w->next, task, XB_ACQ_REL);
    if (before) runq_put(w, before);
  } else if (w) {
    runq_put(w, task);
  } else {
    void *t = task;
    xb_mutex_lock(&poolLock);
    global_put(&t, 1);
    xb_mutex_unlock(&poolLock);
  }
  xb_atomic_fence(XB_SEQ_CST);
  if (xb_atomic_load32(&active, XB_SEQ_CST) < xb_atomic_load32(&target, XB_ACQUIRE)) wake_or_spawn();
}

int xb_sched_in_worker(void) { return me && xb_atomic_load32(&me->inTask, XB_RELAXED); }

void xb_sched_blocking(int entering) {
  worker *w = me;
  if (!w || !xb_atomic_load32(&w->inTask, XB_RELAXED)) return;
  if (!entering) {
    xb_atomic_add32(&active, 1, XB_ACQ_REL);
    return;
  }
  xb_atomic_add32(&active, (uint32_t)-1, XB_SEQ_CST);
  if (xb_atomic_load32(&queued, XB_SEQ_CST) &&
      xb_atomic_load32(&active, XB_SEQ_CST) < xb_atomic_load32(&target, XB_ACQUIRE))
    wake_or_spawn();
}

void xb_sched_stop(void) {
  if (!xb_atomic_load32(&target, XB_ACQUIRE)) return; /* the pool never started */
  xb_atomic_store32(&stopping, 1, XB_SEQ_CST);
  xb_mutex_lock(&poolLock);
  worker *w = idle;
  idle = NULL;
  xb_mutex_unlock(&poolLock);
  while (w) {
    worker *n = w->idleNext;
    xb_atomic_add32(&active, 1, XB_ACQ_REL);
    xb_atomic_store32(&w->wake, 1, XB_RELEASE);
    xb_park_wake(&w->wake, 1);
    w = n;
  }
}

void xb_sched_end(xb_ctx *ctx) {
  if (!xb_atomic_load32(&target, XB_ACQUIRE)) return;
  xb_atomic_store32(&stopping, 1, XB_SEQ_CST); /* no worker starts while the tasks left are canceled */
  for (;;) {
    xb_mutex_lock(&poolLock);
    void *t = gLen ? global_take() : NULL;
    xb_mutex_unlock(&poolLock);
    if (!t) break;
    xb_atomic_add32(&queued, (uint32_t)-1, XB_RELAXED);
    xb_item task;
    xb_set_ptr(&task, XB_T_OBJECT, t);
    xb_gc_root_push(ctx, &task);
    xb_task_cancel(ctx, &task); /* its continuations come to the queue, and are canceled in turn */
    xb_gc_root_pop(ctx, 1);
  }
  xb_atomic_store32(&stopping, 0, XB_SEQ_CST);
}

static void mark_slot(void **p, int (*mark)(xb_item *)) {
  if (!*p) return;
  xb_item it;
  xb_set_ptr(&it, XB_T_OBJECT, *p);
  mark(&it);
  *p = it.v.p;
}

void xb_sched_mark(int (*mark)(xb_item *)) {
  for (uint32_t k = 0; k < slots; ++k) {
    worker *w = workers[k];
    mark_slot(&w->next, mark);
    for (uint32_t i = w->head; i != w->tail; ++i) mark_slot(&w->ring[i % RUNQ], mark);
  }
  for (size_t k = 0; k < gLen; ++k) mark_slot(&global[(gHead + k) % gCap], mark);
}
