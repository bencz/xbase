/* The scheduler of tasks (docs/concurrency.md#scheduler).
 *
 * Workers are program threads (xb_program_thread_start), started when tasks first need them; `target` of them run tasks
 * at once (one per processor, or //WORKERS:n). Each has a queue of its own, a ring of RUNQ tasks that it fills at the
 * tail and takes from at the head, which other workers steal half of at once (one compare-and-swap of the head), and a
 * slot for the task to run next (a task it just made: locality), which a thief takes only when the ring is empty. A
 * chain of tasks run from that slot (each one making the next ready) shares one slice of time: once a link of it
 * starts NEXT_SLICE_NS or more after the chain began, a task that waits (in the ring, the pool's queue, another
 * worker's ring or a feeder's) runs before the next link, which begins a new chain; with none waiting, the chain gets
 * another slice. So a chain never starves the other tasks. Tasks
 * from other threads go to their feeders (below); those a full ring sheds (half of it) go to the pool's queue, under
 * the pool's lock, which a worker looks at first once every GLOBAL_FIRST tasks.
 *
 * A worker with nothing to run searches for SEARCH_NS (it spins: one at a time, and only while another runs a task),
 * then parks on a word of its own, listed as idle under the pool's lock, after it looked once more for work (the count
 * of queued tasks, read after its listing, against a submission that counts its task before it looks for an idle
 * worker: one of the two sees the other). A submission wakes an idle worker, or starts one (or leaves the start to the
 * monitor, past twice the target: may_spawn_locked), while fewer than `target` are active (searching or running,
 * neither parked nor away: BLOCKED or LONG) and none searches (the searcher finds the task; one that finds work wakes
 * another for what is left, one that gives up looks at the count before it parks; a worker woken from its park wakes
 * none: more wakes cost more than the work they find, and the monitor puts a worker beside one that runs long). A thief
 * takes another worker's next task only once that worker's task has run STEAL_NEXT_NS: a task that ends soon runs the
 * task it made ready itself (a continuation, a value handed over), without a thread switch. A worker running a task
 * that waits (XB_BLOCK_WAITS: a park, a pause) counts as inactive meanwhile: when work waits, another worker takes its
 * place (blocking handoff): one started at once while fewer than twice the target are alive, past that one at a time,
 * paced (may_spawn_locked), up to the most workers (workersMax); a worker beyond the target that stays idle
 * SURPLUS_IDLE_NS ends. One that enters an operation that usually returns at once (XB_BLOCK_MAY: a file's I/O) keeps
 * its place: handing it over for each of them would wake a thread a call (most of a short call's cost, and threads
 * past the target fighting for the processors); the monitor, which looks every CALL_LOOK_NS while such a worker and
 * waiting work are there, hands over the place of one it finds in the same operation CALL_LOOK_NS or more after it
 * first saw it there, as many as tasks wait. With no monitor (its thread could not start), such a worker gives its
 * place at once, as one that waits.
 *
 * A worker's `run` word says what it does: IDLE (no task), RUNNING one, CALL in an operation that usually returns at
 * once, BLOCKED in one that blocks, or LONG: running one longer than LONG_TASK_NS while work waits, seen by the
 * monitor. The last two count as away (not active: another worker takes the place); the word changes with
 * compare-and-swap (RUNNING to CALL, BLOCKED or LONG and back, CALL to BLOCKED by the monitor, any to IDLE at the
 * task's end, which repays a LONG), so that each absence is counted and repaid once. Besides the places of workers in
 * a CALL too long, the monitor hands over at most one place per look for a task that runs long, and only when no task
 * ended since its last look, keeping the workers away as LONG to the target's number (a computation never multiplies
 * the threads past twice the target).
 *
 * Timers: a binary heap of {deadline, target} under the timers' lock, which one program thread serves (started with the
 * pool): it sleeps until the earliest deadline, then fires what is due (xb_task_timer), one at a time, without the
 * lock. It is also the monitor: while tasks wait in the queues it looks every MONITOR_NS for workers that run one task
 * long, and puts another worker in their place; with nothing queued it sleeps until its next timer (or a new, earlier
 * one) at most, and a submission or a search that finds every place taken while work waits wakes it (`timerIdle`).
 *
 * The queues and the timers hold object addresses: the collector marks them (xb_sched_mark), every other thread
 * stopped. */
#include "xbrt_int.h"

#include "core/sched.h"
#include "core/sync.h"

#include <stdlib.h>
#include <string.h>

#define RUNQ 256u
#define WORKERS_MAX 1024u
#define GLOBAL_FIRST 61u
#define SURPLUS_IDLE_NS ((int64_t)2000 * XB_NS_PER_MS)
#define LONG_TASK_NS ((int64_t)20 * XB_NS_PER_MS)
#define MONITOR_NS ((int64_t)10 * XB_NS_PER_MS)
#define CALL_LOOK_NS ((int64_t)50 * XB_NS_PER_US) /* the monitor's looks while a worker is in a CALL */
#define SEARCH_NS ((int64_t)50 * XB_NS_PER_US)
#define STEAL_NEXT_NS ((int64_t)5 * XB_NS_PER_US)
#define NEXT_SLICE_NS ((int64_t)1 * XB_NS_PER_MS)
#define STARVED_NS ((int64_t)2000 * XB_NS_PER_MS)
/* The pace of new workers past twice the target: see may_spawn_locked. */
#define SPAWN_STEP_NS ((int64_t)25 * XB_NS_PER_MS)
#define SPAWN_MAX_NS ((int64_t)250 * XB_NS_PER_MS)
#define SEARCH_LOOK 64u /* looks for work between looks at the clock (and yields of the processor) */
#define SEARCH_RETRY 16u /* looks between two searches of the queues, which found nothing that waits there */

enum { RUN_IDLE, RUN_RUNNING, RUN_CALL, RUN_BLOCKED, RUN_LONG };

typedef struct worker {
  uint32_t head, tail; /* the ring's: the head taken with compare-and-swap by any, the tail written by its owner */
  void *ring[RUNQ];
  void *next;          /* the task to run next: exchanged */
  uint32_t wake;       /* 1 once a waker took it off the idle list: parked on */
  uint32_t alive;      /* a thread runs it (the pool's lock) */
  uint32_t run;        /* RUN_*: BLOCKED and LONG are not counted active */
  uint32_t runs;       /* tasks it started: the monitor's mark is undone when it finds another task than it saw */
  uint32_t calls;      /* operations it entered as CALL (its owner writes it) */
  XB_ALIGN8 uint64_t runStart; /* the monotonic time its task started, or came back from blocking (the monitor's) */
  uint64_t lastStart;  /* its owner's only: when its last task started, */
  uint64_t chainStart; /* when the chain of tasks run from `next` that it belongs to began (its slice's start), */
  int fromNext;        /* and whether the task found last came from `next` */
  uint32_t ticks;
  uint32_t seed;       /* where its search for a victim starts */
  struct worker *idleNext;
} worker;

static worker *workers[WORKERS_MAX];
static uint32_t slots;   /* workers[0 .. slots) made so far (the pool's lock; read atomically) */
static uint32_t total;   /* workers alive */
/* The counters every worker changes, each on a cache line of its own: padded to the line, and the whole aligned to
 * one, so that nothing else shares their lines. */
typedef struct own_line {
  uint32_t n;
  char pad[XB_CACHE_LINE - sizeof(uint32_t)];
} own_line;
static _Alignas(XB_CACHE_LINE) struct {
  own_line active;    /* workers searching or running, neither parked nor away (BLOCKED, LONG) */
  own_line queued;    /* tasks in the queues */
  own_line ended;     /* tasks ended (the monitor tells progress) */
  own_line searching; /* workers searching for work before they park */
} counts;
static uint32_t longCount; /* workers LONG */
static uint32_t handoffs;  /* places handed over (xb_sched_handoffs) */
static int64_t lastSpawn;  /* when the last worker was started (the pool's lock) */
static uint32_t target;  /* workers that run at once; 0 until the pool is made (make_pool) */
static uint32_t stopping;
static unsigned requested;

static xb_mutex poolLock;
static xb_once_flag poolMade = XB_ONCE_INIT;
static worker *idle;
static void **global; /* the pool's queue: a ring of gCap entries from gHead, gLen of them */
static size_t gHead, gLen, gCap;

static XB_THREAD_LOCAL worker *me;

/* Feeders: the ring of a thread that submits tasks without being a worker (the program's thread, ThreadStart()'s),
 * which it fills without the pool's lock and the workers steal from as from one another; when its thread ends
 * (xb_sched_thread_end) the feeder, with the tasks left in it, which thieves take as before, serves the next thread
 * that submits. A worker's struct, of which only the ring is used. Past FEEDERS_MAX threads at once, the others submit
 * to the pool's queue. */
#define FEEDERS_MAX 1024u
static worker *feeders[FEEDERS_MAX];
static uint32_t feederSlots; /* feeders[0 .. feederSlots) made so far (the pool's lock; read atomically) */
static worker *feederFree;   /* feeders whose threads ended (the pool's lock), linked by idleNext */
static XB_THREAD_LOCAL worker *fed;

typedef struct timer {
  int64_t deadline;
  void *target;
} timer;

static xb_mutex timerLock;
static timer *timers; /* a binary heap, the earliest deadline first */
static size_t timerLen, timerCap;
static uint32_t timerWake;    /* changed to wake the timers' thread: an earlier deadline, the program's end */
static uint32_t timerRunning; /* its thread exists (the timers' lock) */
static uint32_t timerIdle;    /* its thread sleeps without watching the workers (nothing was queued), up to its next
                                 timer at most */
static size_t timerCompactAt = 1024; /* the heap's size that has it compacted (timers_compact) */

static void timers_wake(void);
static void timers_compact(void);

/* The most workers: WORKERS_MAX, or as many as have their stacks in a quarter of the address space the process may map
 * when that is fewer (a 32-bit process, a limit set on the process): the rest is the heap's, which the collector
 * needs to work, and the other threads'. The space is what the system allows, at most what user-space pointers reach
 * (USER_SPACE). Set once, before any worker starts. */
#define USER_SPACE (sizeof(void *) >= 8 ? (uint64_t)1 << 47 : (uint64_t)1 << 31)
static uint32_t workersMax = WORKERS_MAX;

static void make_pool(void) {
  if (!xb_mutex_init(&poolLock) || !xb_mutex_init(&timerLock)) xb_gc_fatal("cannot create the scheduler's locks");
  uint64_t space = xb_platform_address_space();
  if (space > USER_SPACE) space = USER_SPACE;
  uint64_t room = space / 4 / XB_WORKER_STACK;
  if (room < workersMax) workersMax = room ? (uint32_t)room : 1;
  unsigned n = requested ? requested : xb_platform_cpu_count();
  if (n > workersMax) n = workersMax;
  xb_atomic_store32(&target, n ? n : 1, XB_RELEASE);
}

void xb_sched_set_workers(unsigned n) { requested = n; }

void xb_sched_workers(uint32_t *alive, uint32_t *runAtOnce, uint32_t *most) {
  xb_once(&poolMade, make_pool);
  *alive = xb_atomic_load32(&total, XB_ACQUIRE);
  *runAtOnce = xb_atomic_load32(&target, XB_ACQUIRE);
  *most = workersMax;
}

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

/* Whether a thief may take `v`'s next task: `v` is away (BLOCKED, LONG), or its task has run STEAL_NEXT_NS (a CALL is
 * part of its run). An idle `v` takes it in a moment. */
static int next_stealable(worker *v) {
  uint32_t run = xb_atomic_load32(&v->run, XB_ACQUIRE);
  if (run == RUN_BLOCKED || run == RUN_LONG) return 1;
  return (run == RUN_RUNNING || run == RUN_CALL) &&
         xb_platform_monotonic_ns() - (int64_t)xb_atomic_load64(&v->runStart, XB_RELAXED) >= STEAL_NEXT_NS;
}

/* Half of victim `v`'s ring moved into thief `w`'s (empty), the last of them returned; or `v`'s next task, when its
 * ring is empty and it may be taken. */
static void *steal_from(worker *w, worker *v) {
  for (;;) {
    uint32_t h = xb_atomic_load32(&v->head, XB_ACQUIRE), tl = xb_atomic_load32(&v->tail, XB_ACQUIRE);
    uint32_t n = tl - h;
    n -= n / 2;
    if (!n) {
      void *t = xb_atomic_loadptr(&v->next, XB_ACQUIRE);
      if (!t || !next_stealable(v)) return NULL;
      return xb_atomic_casptr(&v->next, &t, NULL, XB_ACQ_REL) ? t : NULL;
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

/* Half of another worker's ring, or of a feeder's, from a victim chosen at random. */
static void *steal(worker *w) {
  uint32_t n = xb_atomic_load32(&slots, XB_ACQUIRE), f = xb_atomic_load32(&feederSlots, XB_ACQUIRE);
  w->seed = w->seed * 1103515245u + 12345u;
  uint32_t all = n + f, start = all ? (w->seed >> 16) % all : 0;
  for (uint32_t k = 0; k < all; ++k) {
    uint32_t i = (start + k) % all;
    worker *v = xb_atomic_loadptr(i < n ? (void *const *)&workers[i] : (void *const *)&feeders[i - n], XB_ACQUIRE);
    if (!v || v == w) continue;
    void *t = steal_from(w, v);
    if (t) return t;
  }
  return NULL;
}

/* The calling thread's feeder, made or reused at its first submission; NULL past FEEDERS_MAX. */
static worker *feeder(void) {
  if (fed) return fed;
  xb_mutex_lock(&poolLock);
  worker *w = feederFree;
  if (w) {
    feederFree = w->idleNext;
  } else if (feederSlots < FEEDERS_MAX) {
    w = calloc(1, sizeof *w);
    if (!w) xb_gc_fatal("out of memory");
    xb_atomic_storeptr((void **)&feeders[feederSlots], w, XB_RELEASE);
    xb_atomic_store32(&feederSlots, feederSlots + 1, XB_RELEASE);
  }
  xb_mutex_unlock(&poolLock);
  fed = w;
  return w;
}

static void *find_work(worker *w) {
  void *t = NULL;
  if (++w->ticks % GLOBAL_FIRST == 0) t = global_get(w);
  if (!t && w->lastStart - w->chainStart >= (uint64_t)NEXT_SLICE_NS) { /* the chain's slice is over: what waits first */
    t = runq_get(w);
    if (!t) t = global_get(w);
    if (!t) t = steal(w); /* other workers' rings, the feeders' */
    if (!t) w->chainStart = w->lastStart; /* nothing waits: the chain goes on for another slice */
  }
  w->fromNext = 0;
  if (!t && (t = xb_atomic_xchgptr(&w->next, NULL, XB_ACQ_REL)) != NULL) w->fromNext = 1;
  if (!t) t = runq_get(w);
  if (!t) t = global_get(w);
  if (!t) t = steal(w);
  if (t) xb_atomic_add32(&counts.queued.n, (uint32_t)-1, XB_RELAXED);
  return t;
}

static void worker_body(xb_ctx *ctx, void *arg);

/* A worker made or reused, its thread started (under the pool's lock): NULL when none can be. */
static worker *spawn_locked(void) {
  if (xb_atomic_load32(&stopping, XB_ACQUIRE) || xb_atomic_load32(&total, XB_RELAXED) >= workersMax) return NULL;
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
  xb_atomic_add32(&counts.active.n, 1, XB_ACQ_REL);
  if (xb_program_thread_start(worker_body, w, XB_WORKER_STACK)) return w;
  xb_atomic_store32(&w->alive, 0, XB_RELEASE);
  xb_atomic_add32(&total, (uint32_t)-1, XB_ACQ_REL);
  xb_atomic_add32(&counts.active.n, (uint32_t)-1, XB_ACQ_REL);
  return NULL;
}

/* Whether a new worker may start now (the pool's lock): at once while fewer than twice the target are alive (the
 * places of workers that block a moment are filled without delay); past that, one after the last by SPAWN_STEP_NS for
 * each `target` workers beyond twice the target, at most SPAWN_MAX_NS: tasks that block for long, many of them, cost
 * a few threads a second, not a thread each. */
static int may_spawn_locked(int64_t now) {
  uint32_t alive = xb_atomic_load32(&total, XB_RELAXED), t = xb_atomic_load32(&target, XB_RELAXED);
  if (alive < 2 * t) return 1;
  int64_t delay = SPAWN_STEP_NS * (int64_t)(1 + (alive - 2 * t) / t);
  return now - lastSpawn >= (delay < SPAWN_MAX_NS ? delay : SPAWN_MAX_NS);
}

/* An idle worker woken, or a new one started when it may be, for work that waits; when none may start yet, the
 * monitor, which looks again while work waits, is made to watch. */
static void wake_or_spawn(void) {
  int deferred = 0;
  xb_mutex_lock(&poolLock);
  worker *w = idle;
  if (w) {
    idle = w->idleNext;
    xb_atomic_add32(&counts.active.n, 1, XB_ACQ_REL); /* for it: a second submission sees it active */
  } else {
    int64_t now = xb_platform_monotonic_ns();
    /* deferred to the monitor, which runs on the timers' thread: at once when that thread could not start */
    if (!may_spawn_locked(now) && xb_atomic_load32(&timerRunning, XB_ACQUIRE)) deferred = 1;
    else if (spawn_locked()) lastSpawn = now;
  }
  xb_mutex_unlock(&poolLock);
  if (w) {
    xb_atomic_store32(&w->wake, 1, XB_RELEASE);
    xb_park_wake(&w->wake, 1);
  } else if (deferred && xb_atomic_load32(&timerIdle, XB_SEQ_CST)) {
    timers_wake();
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
  xb_atomic_add32(&counts.active.n, (uint32_t)-1, XB_SEQ_CST);
  xb_mutex_unlock(&poolLock);
  xb_atomic_fence(XB_SEQ_CST);
  if ((xb_atomic_load32(&counts.queued.n, XB_SEQ_CST) || xb_atomic_load32(&stopping, XB_SEQ_CST)) && idle_remove(w)) {
    if (xb_atomic_load32(&stopping, XB_ACQUIRE)) return 0;
    xb_atomic_add32(&counts.active.n, 1, XB_ACQ_REL);
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
  xb_atomic_add32(&w->runs, 1, XB_RELAXED);
  uint64_t now = (uint64_t)xb_platform_monotonic_ns();
  w->lastStart = now;
  if (!w->fromNext) w->chainStart = now;
  xb_atomic_store64(&w->runStart, now, XB_RELAXED);
  xb_atomic_store32(&w->run, RUN_RUNNING, XB_SEQ_CST);
  xb_task_run(ctx, &task);
  if (xb_atomic_xchg32(&w->run, RUN_IDLE, XB_ACQ_REL) == RUN_LONG) { /* the monitor gave its place away */
    xb_atomic_add32(&counts.active.n, 1, XB_ACQ_REL);
    xb_atomic_add32(&longCount, (uint32_t)-1, XB_ACQ_REL);
  }
  xb_atomic_add32(&counts.ended.n, 1, XB_RELAXED);
  xb_gc_root_pop(ctx, 1);
}

void xb_sched_thread_end(void) {
  worker *w = fed;
  if (!w) return;
  fed = NULL;
  xb_mutex_lock(&poolLock); /* the tasks left in its ring stay there, for thieves and the feeder's next thread */
  w->idleNext = feederFree;
  feederFree = w;
  xb_mutex_unlock(&poolLock);
}

/* A worker's tasks go to the pool's queue as it ends. */
static void hand_back(worker *w) {
  void *t = xb_atomic_xchgptr(&w->next, NULL, XB_ACQ_REL);
  xb_mutex_lock(&poolLock);
  if (t) global_put(&t, 1);
  while ((t = runq_get(w)) != NULL) global_put(&t, 1);
  xb_mutex_unlock(&poolLock);
}

static void fill_place(void);

/* The search of a worker that found nothing: SEARCH_NS more of looking, while it is the only searcher and another
 * worker runs a task (work may come from it). The task found, or NULL. */
static void *search(xb_ctx *ctx, worker *w) {
  uint32_t none = 0;
  if (xb_atomic_load32(&counts.active.n, XB_ACQUIRE) < 2 ||
      !xb_atomic_cas32(&counts.searching.n, &none, 1, XB_SEQ_CST))
    return NULL;
  int64_t until = xb_platform_monotonic_ns() + SEARCH_NS;
  void *t = NULL;
  for (uint32_t k = 1; !t; ++k) {
    if (xb_atomic_load32(&stopping, XB_ACQUIRE) || xb_pending_get(ctx)) break; /* a collection waits: park */
    if (k % SEARCH_RETRY == 1 && xb_atomic_load32(&counts.queued.n, XB_ACQUIRE) && (t = find_work(w)) != NULL) break;
    if (k % SEARCH_LOOK == 0) {
      if (xb_platform_monotonic_ns() >= until) break;
      xb_thread_yield();
    }
  }
  xb_atomic_store32(&counts.searching.n, 0, XB_SEQ_CST);
  if (t) fill_place(); /* what is left waits for another */
  return t;
}

static void worker_body(xb_ctx *ctx, void *arg) {
  worker *w = arg;
  me = w;
  for (;;) {
    if (xb_atomic_load32(&stopping, XB_ACQUIRE) || (xb_pending_get(ctx) & XB_PENDING_QUIT)) {
      xb_atomic_add32(&counts.active.n, (uint32_t)-1, XB_ACQ_REL);
      break;
    }
    void *t = find_work(w);
    if (!t) t = search(ctx, w);
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

static void timers_start(void);

void xb_sched_submit(xb_object *task, int next) {
  xb_once(&poolMade, make_pool);
  timers_start(); /* the monitor too */
  xb_atomic_add32(&counts.queued.n, 1, XB_SEQ_CST);
  worker *w = me;
  if (w && next) {
    void *before = xb_atomic_xchgptr(&w->next, task, XB_ACQ_REL);
    if (before) runq_put(w, before);
  } else if (w) {
    runq_put(w, task);
  } else if ((w = feeder()) != NULL) {
    runq_put(w, task);
  } else {
    void *t = task;
    xb_mutex_lock(&poolLock);
    global_put(&t, 1);
    xb_mutex_unlock(&poolLock);
  }
  xb_atomic_fence(XB_SEQ_CST);
  /* the searcher finds it, or sees it counted before it parks */
  if (xb_atomic_load32(&counts.searching.n, XB_SEQ_CST)) return;
  if (xb_atomic_load32(&counts.active.n, XB_SEQ_CST) < xb_atomic_load32(&target, XB_ACQUIRE)) wake_or_spawn();
  else if (xb_atomic_load32(&timerIdle, XB_SEQ_CST)) timers_wake(); /* every worker busy: the monitor looks */
}

uint32_t xb_sched_handoffs(void) { return xb_atomic_load32(&handoffs, XB_RELAXED); }

int xb_sched_in_worker(void) { return me && xb_atomic_load32(&me->run, XB_RELAXED) != RUN_IDLE; }

/* Another worker woken or started when work waits and fewer than `target` are active. */
static void fill_place(void) {
  if (!xb_atomic_load32(&counts.queued.n, XB_SEQ_CST)) return;
  if (xb_atomic_load32(&counts.active.n, XB_SEQ_CST) < xb_atomic_load32(&target, XB_ACQUIRE)) wake_or_spawn();
  else if (xb_atomic_load32(&timerIdle, XB_SEQ_CST)) timers_wake(); /* the monitor looks: a worker may be in a CALL */
}

/* A place given to another worker while its worker is away: counted inactive, then filled when work waits. */
static void hand_over(void) {
  xb_atomic_add32(&handoffs, 1, XB_RELAXED);
  xb_atomic_add32(&counts.active.n, (uint32_t)-1, XB_SEQ_CST);
  fill_place();
}

void xb_sched_blocking(int how) {
  worker *w = me;
  if (!w) return;
  uint32_t running = RUN_RUNNING;
  if (how == XB_BLOCK_WAITS || !xb_atomic_load32(&timerRunning, XB_ACQUIRE)) { /* no monitor: as one that waits */
    if (xb_atomic_cas32(&w->run, &running, RUN_BLOCKED, XB_ACQ_REL)) hand_over(); /* LONG: away already; IDLE: none */
    return;
  }
  if (how == XB_BLOCK_MAY) {
    /* counted before the word says CALL, so that the monitor, which tells calls apart by the count, sees a new call
     * with its own count (a call that ends and another that begins between its read of the count and its swap of the
     * word only cost a place handed over early) */
    xb_atomic_store32(&w->calls, xb_atomic_load32(&w->calls, XB_RELAXED) + 1, XB_RELAXED);
    xb_atomic_cas32(&w->run, &running, RUN_CALL, XB_SEQ_CST);
    return;
  }
  uint32_t call = RUN_CALL, blocked = RUN_BLOCKED;
  if (xb_atomic_cas32(&w->run, &call, RUN_RUNNING, XB_ACQ_REL)) return; /* back before the monitor gave its place */
  if (xb_atomic_cas32(&w->run, &blocked, RUN_RUNNING, XB_ACQ_REL)) {
    xb_atomic_store64(&w->runStart, (uint64_t)xb_platform_monotonic_ns(), XB_RELAXED);
    xb_atomic_add32(&counts.active.n, 1, XB_ACQ_REL);
  }
}

/* What the program's standard error is told once tasks starve (starved). */
typedef struct starving {
  uint32_t tasks, workers;
} starving;

static void tell_starving(void *p) {
  const starving *s = p;
  xb_output_lock();
  fflush(stdout);
  int seconds = (int)(STARVED_NS / XB_NS_PER_MS / 1000);
  if (s->workers)
    fprintf(stderr, "\nxbrt: %u tasks have waited %d seconds while all %u workers wait or run long, and more workers "
                    "start only slowly",
            s->tasks, seconds, s->workers);
  else
    fprintf(stderr, "\nxbrt: %u tasks have waited %d seconds and no worker could start", s->tasks, seconds);
  fprintf(stderr, ": a task that waits for a lock, a semaphore, a condition, a task or a channel holds its worker, "
                  "which awaiting it (LockAsync, WaitAsync, ReceiveAsync, AWAIT of the task) does not\n");
  fflush(stderr);
  xb_output_unlock();
}

/* Tasks starve when they wait in the queues, none ended for STARVED_NS, and then no worker is active: each one alive
 * waits (blocked) or runs one task long, or none could start (`active`: a worker just started counts a moment, until
 * it blocks too, so it is looked at when the time is up, not along it), whatever workers still start (paced,
 * may_spawn_locked) or cannot (the most workers, threads the system refuses). It tells that the program's throughput
 * is held by tasks that hold their workers, not that it is deadlocked: the standard error is told so once, until
 * tasks end again. */
static void starved(int progress, int waiting) {
  static int64_t since;
  static int told;
  if (progress || !waiting) {
    since = 0;
    told = 0;
    return;
  }
  int64_t now = xb_platform_monotonic_ns();
  if (!since) since = now;
  if (told || now - since < STARVED_NS) return;
  if (xb_atomic_load32(&counts.active.n, XB_ACQUIRE)) return;
  told = 1;
  starving s = {xb_atomic_load32(&counts.queued.n, XB_ACQUIRE), xb_atomic_load32(&total, XB_ACQUIRE)};
  xb_blocking_run(tell_starving, &s); /* a full pipe holds the write */
}

/* While work waits: the workers found in the same CALL as CALL_LOOK_NS or more before have their places given to
 * others (fill_place), as many as tasks wait; whether a worker is in a CALL now. What the monitor saw of each worker
 * is its own (not on the worker's line, which the worker writes). */
static int calls_handed(int waiting) {
  static uint32_t seen[WORKERS_MAX]; /* each worker's `calls` when the monitor first saw it in that CALL, */
  static int64_t since[WORKERS_MAX]; /* and when */
  int inCall = 0;
  int64_t now = xb_platform_monotonic_ns();
  uint32_t n = xb_atomic_load32(&slots, XB_ACQUIRE), queued = xb_atomic_load32(&counts.queued.n, XB_ACQUIRE);
  for (uint32_t k = 0; k < n; ++k) {
    worker *w = xb_atomic_loadptr((void *const *)&workers[k], XB_ACQUIRE);
    if (xb_atomic_load32(&w->run, XB_SEQ_CST) != RUN_CALL) continue;
    uint32_t calls = xb_atomic_load32(&w->calls, XB_ACQUIRE), call = RUN_CALL;
    if (calls != seen[k] || !since[k]) {
      seen[k] = calls;
      since[k] = now;
      inCall = 1;
    } else if (!waiting || !queued || now - since[k] < CALL_LOOK_NS) {
      inCall = 1;
    } else if (xb_atomic_cas32(&w->run, &call, RUN_BLOCKED, XB_SEQ_CST)) {
      hand_over(); /* the worker's return finds BLOCKED and repays it */
      --queued;
    }
  }
  return inCall;
}

/* The monitor: while tasks wait in the queues, the places that a thread which did not start left empty are filled
 * again (fill_place), workers in one CALL too long have theirs given to others (calls_handed), and when no task ended
 * since its last look, the first worker found running one task longer than LONG_TASK_NS has its place given to
 * another, as long as fewer than `target` are LONG; tasks that starve are told. Whether it should look again after
 * CALL_LOOK_NS (a worker in a CALL while work waits). */
static int monitor(void) {
  static uint32_t lastEnded;
  uint32_t now_ended = xb_atomic_load32(&counts.ended.n, XB_RELAXED), progress = now_ended != lastEnded;
  lastEnded = now_ended;
  int waiting = xb_atomic_load32(&counts.queued.n, XB_ACQUIRE) != 0;
  int inCall = calls_handed(waiting) && waiting;
  if (waiting) fill_place();
  starved((int)progress, waiting);
  if (progress || !waiting || xb_atomic_load32(&longCount, XB_ACQUIRE) >= xb_atomic_load32(&target, XB_ACQUIRE))
    return inCall;
  uint64_t now = (uint64_t)xb_platform_monotonic_ns();
  uint32_t n = xb_atomic_load32(&slots, XB_ACQUIRE);
  for (uint32_t k = 0; k < n; ++k) {
    worker *w = xb_atomic_loadptr((void *const *)&workers[k], XB_ACQUIRE);
    uint32_t running = RUN_RUNNING, seen = xb_atomic_load32(&w->runs, XB_ACQUIRE);
    if (now - xb_atomic_load64(&w->runStart, XB_RELAXED) < (uint64_t)LONG_TASK_NS ||
        !xb_atomic_cas32(&w->run, &running, RUN_LONG, XB_SEQ_CST))
      continue;
    /* Marked: counted away now (its task's end repays it). When the task is another than the one seen long, the
     * mark is undone, if the worker did not repay it first. */
    xb_atomic_add32(&longCount, 1, XB_ACQ_REL);
    xb_atomic_add32(&counts.active.n, (uint32_t)-1, XB_SEQ_CST);
    uint32_t marked = RUN_LONG;
    if (xb_atomic_load32(&w->runs, XB_SEQ_CST) != seen &&
        xb_atomic_cas32(&w->run, &marked, RUN_RUNNING, XB_ACQ_REL)) {
      xb_atomic_add32(&longCount, (uint32_t)-1, XB_ACQ_REL);
      xb_atomic_add32(&counts.active.n, 1, XB_ACQ_REL);
      continue;
    }
    fill_place();
    return inCall;
  }
  return inCall;
}

/* Timers. */

static void timer_swap(size_t a, size_t b) {
  timer t = timers[a];
  timers[a] = timers[b];
  timers[b] = t;
}

static void timer_up(size_t k) {
  while (k && timers[(k - 1) / 2].deadline > timers[k].deadline) {
    timer_swap(k, (k - 1) / 2);
    k = (k - 1) / 2;
  }
}

static void timer_down(size_t k) {
  for (;;) {
    size_t l = 2 * k + 1, r = l + 1, least = k;
    if (l < timerLen && timers[l].deadline < timers[least].deadline) least = l;
    if (r < timerLen && timers[r].deadline < timers[least].deadline) least = r;
    if (least == k) return;
    timer_swap(k, least);
    k = least;
  }
}

static void timer_body(xb_ctx *ctx, void *arg);

static void timers_wake(void) {
  xb_atomic_add32(&timerWake, 1, XB_RELEASE);
  xb_park_wake(&timerWake, 1);
}

/* Starts the timers' thread unless it runs (or the program ends). One that cannot start (the system gives no thread)
 * is said once on standard error, and tried again by the next timer added: till then no timer fires. */
static void timers_start(void) {
  static int told;
  if (xb_atomic_load32(&timerRunning, XB_ACQUIRE)) return;
  xb_mutex_lock(&timerLock);
  if (!timerRunning && !xb_atomic_load32(&stopping, XB_ACQUIRE)) {
    xb_atomic_store32(&timerRunning, 1, XB_RELEASE);
    if (!xb_program_thread_start(timer_body, NULL, XB_WORKER_STACK)) {
      xb_atomic_store32(&timerRunning, 0, XB_RELEASE);
      if (!told) {
        told = 1;
        fprintf(stderr, "\nxbrt: the timers' thread cannot start: timers wait until one can\n");
      }
    }
  }
  xb_mutex_unlock(&timerLock);
}

void xb_timer_add(xb_object *object, int64_t deadline) {
  xb_once(&poolMade, make_pool);
  xb_mutex_lock(&timerLock);
  if (timerLen == timerCap) {
    size_t cap = timerCap ? timerCap * 2 : 64;
    timer *grown = realloc(timers, cap * sizeof *grown);
    if (!grown) xb_gc_fatal("out of memory");
    timers = grown;
    timerCap = cap;
  }
  if (timerLen >= timerCompactAt) timers_compact();
  timers[timerLen] = (timer){deadline, object};
  timer_up(timerLen++);
  int earliest = timers[0].target == object && timers[0].deadline == deadline;
  xb_mutex_unlock(&timerLock);
  timers_start();
  if (earliest) timers_wake();
}

/* The earliest timer due by `now` (all of them when `now` is INT64_MAX), taken off the heap: NULL for none; its
 * deadline in `*deadline`, the earliest left in `*next` (-1: none). */
static void *timer_due(int64_t now, int64_t *deadline, int64_t *next) {
  void *t = NULL;
  xb_mutex_lock(&timerLock);
  if (timerLen && timers[0].deadline <= now) {
    t = timers[0].target;
    *deadline = timers[0].deadline;
    timers[0] = timers[--timerLen];
    timer_down(0);
  }
  *next = timerLen ? timers[0].deadline : -1;
  xb_mutex_unlock(&timerLock);
  return t;
}

/* The timers whose targets need them no more (xb_task_timer_dead: settled, or a later CancelAfter replaced them) are
 * dropped once the heap doubled since the last time (under the timers' lock), so a timeout settled long before its
 * deadline does not keep its task. */
static void timers_compact(void) {
  size_t kept = 0;
  for (size_t k = 0; k < timerLen; ++k)
    if (!xb_task_timer_dead(timers[k].target, timers[k].deadline)) timers[kept++] = timers[k];
  timerLen = kept;
  for (size_t k = timerLen / 2; k-- > 0;) timer_down(k);
  timerCompactAt = 2 * timerLen > 1024 ? 2 * timerLen : 1024;
}

/* Fires one timer taken off the heap (its target rooted meanwhile): at its time, or at the program's end. */
static void timer_fire(xb_ctx *ctx, void *t, int64_t deadline, int ending_now) {
  xb_item due;
  xb_set_ptr(&due, XB_T_OBJECT, t);
  xb_gc_root_push(ctx, &due);
  xb_task_timer(ctx, &due, deadline, ending_now);
  xb_gc_root_pop(ctx, 1);
}

static void timer_body(xb_ctx *ctx, void *arg) {
  XB_UNUSED(arg);
  for (;;) {
    uint32_t seen = xb_atomic_load32(&timerWake, XB_ACQUIRE);
    if (xb_atomic_load32(&stopping, XB_ACQUIRE) || (xb_pending_get(ctx) & XB_PENDING_QUIT)) break;
    int64_t deadline, next;
    void *t;
    while ((t = timer_due(xb_platform_monotonic_ns(), &deadline, &next)) != NULL) {
      timer_fire(ctx, t, deadline, 0);
      xb_pending_clear(ctx, XB_PENDING_UNWIND & ~(uint32_t)XB_PENDING_QUIT); /* what the firing left */
    }
    int inCall = monitor();
    /* tasks wait: the monitor looks every MONITOR_NS, every CALL_LOOK_NS while a worker is in a CALL */
    int watching = xb_atomic_load32(&counts.queued.n, XB_ACQUIRE) != 0;
    int64_t now = xb_platform_monotonic_ns(), slice = watching ? inCall ? CALL_LOOK_NS : MONITOR_NS : -1;
    if (next >= 0 && (slice < 0 || next - now < slice)) slice = next - now > 0 ? next - now : 0;
    /* Not watching, it says so before it reads the count again, as a submission counts its task before it reads that:
     * one of the two sees the other, and a submission that finds every worker busy wakes it. */
    xb_atomic_store32(&timerIdle, !watching, XB_SEQ_CST);
    if (watching || !xb_atomic_load32(&counts.queued.n, XB_SEQ_CST)) xb_park_wait_blocking(&timerWake, seen, slice);
    xb_atomic_store32(&timerIdle, 0, XB_RELAXED);
  }
}

void xb_sched_stop(void) {
  if (!xb_atomic_load32(&target, XB_ACQUIRE)) return; /* the pool was never made */
  xb_atomic_store32(&stopping, 1, XB_SEQ_CST);
  timers_wake();
  xb_mutex_lock(&poolLock);
  worker *w = idle;
  idle = NULL;
  xb_mutex_unlock(&poolLock);
  while (w) {
    worker *n = w->idleNext;
    xb_atomic_add32(&counts.active.n, 1, XB_ACQ_REL);
    xb_atomic_store32(&w->wake, 1, XB_RELEASE);
    xb_park_wake(&w->wake, 1);
    w = n;
  }
}

void xb_sched_end(xb_ctx *ctx) {
  if (!xb_atomic_load32(&target, XB_ACQUIRE)) return;
  xb_atomic_store32(&stopping, 1, XB_SEQ_CST); /* no worker starts while the tasks left are canceled */
  xb_mutex_lock(&timerLock); /* its thread ended (it is restarted when needed) */
  xb_atomic_store32(&timerRunning, 0, XB_RELEASE);
  xb_mutex_unlock(&timerLock);
  int64_t deadline, next;
  for (void *t; (t = timer_due(INT64_MAX, &deadline, &next)) != NULL;) timer_fire(ctx, t, deadline, 1);
  for (;;) {
    xb_mutex_lock(&poolLock);
    void *t = gLen ? global_take() : NULL;
    for (uint32_t k = 0; !t && k < feederSlots; ++k) t = runq_get(feeders[k]); /* no thief: every worker ended */
    xb_mutex_unlock(&poolLock);
    if (!t) break;
    xb_atomic_add32(&counts.queued.n, (uint32_t)-1, XB_RELAXED);
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
  for (uint32_t k = 0; k < feederSlots; ++k) {
    worker *w = feeders[k];
    for (uint32_t i = w->head; i != w->tail; ++i) mark_slot(&w->ring[i % RUNQ], mark);
  }
  for (size_t k = 0; k < gLen; ++k) mark_slot(&global[(gHead + k) % gCap], mark);
  for (size_t k = 0; k < timerLen; ++k) mark_slot(&timers[k].target, mark);
}
