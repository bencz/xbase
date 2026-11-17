/* Tracing garbage collector: precise, generational, with an Immix heap layout;
 * full collections move objects out of fragmented blocks (evacuation).
 *
 * Heap. Objects up to XB_GC_SMALL_MAX bytes (header included) live in blocks
 * of XB_GC_BLOCK_SIZE bytes, aligned to their size, divided into lines of
 * XB_GC_LINE_SIZE bytes; the block's first lines hold its metadata: a mark per
 * line (the epoch of the last collection that reached an object there), a flag
 * per line holding an old object, and a bit per 8 bytes where an object
 * starts. Every thread allocates by bumping a cursor through a hole, a run of
 * free lines, with no lock; an object above one line that does not fit the
 * rest of the hole goes to an overflow run of the thread instead (holes large
 * enough for it, in recyclable or free blocks), so no hole is given up for it.
 * Larger objects, up to the largest size class, live in blocks of one size
 * class each, whose slots are taken from a free list (a dead object's slot
 * serves the next object of its class: holes of many lines would fragment);
 * larger ones (Large objects below) are kept in lists. Blocks come from
 * chunks of XB_GC_CHUNK_BLOCKS blocks, each a mapping of its own from the
 * system (xb_platform_pages_map), aligned to a block.
 *
 * Memory given back. After a collection, the free blocks beyond what the next
 * allocations need (the young budget and an eighth of the blocks in use) are
 * given back to the system (xb_platform_pages_decommit), those of the chunks at
 * the highest addresses first, whole pages of the system at a time (blocks of a
 * page all free): they leave the blocks in use, and a chunk whose blocks are all
 * given back (or never used) is unmapped. A new block is taken from the free
 * blocks, then from the lowest chunk with one given back or never used, then
 * from a new chunk: memory goes back to the system after a peak, and the
 * heap's chunks stay few and low.
 *
 * Large objects. One above the largest size class takes whole units of
 * XB_GC_LARGE_UNIT bytes: a run of them, first fit from the lowest address, in
 * chunks of large objects (XB_GC_LARGE_CHUNK bytes, a mapping each), or a
 * mapping of its own from XB_GC_LARGE_MAP bytes; it never moves. A large
 * object is in the young list until it is old, then in the old list: a young
 * collection sweeps the young list only. A dead one's units are free at once
 * and its mapping goes back; the free units beyond what the next allocations
 * need go back to the system after a collection (runs of whole pages, the
 * highest chunks' first), and a chunk of large objects left with none in use
 * is unmapped.
 *
 * Objects. An 8-byte xb_gc_hdr precedes every object, so object pointers are
 * what the rest of the runtime and compiled code use (an xb_str* is still the
 * string, a cell's value is at offset 0).
 *
 * Generations. Marks are sticky: an object marked by a collection stays
 * marked, and a marked object is old. A young collection marks from the roots
 * and from the remembered set, never traverses old objects, and frees only
 * young objects it did not reach. While aging is on, a young object it reaches
 * ages and one that had already aged is promoted, so objects that die soon
 * after a young collection (a table whose entries are replaced) die young; an
 * object that is old after a young collection and still points to a young one
 * stays in the remembered set (an array keeps the card dirty) until that one
 * is promoted or dies. Aging adapts: it turns off when most aged objects
 * survive to promotion (it only doubles their tracing) and back on when a full
 * collection finds that most promoted bytes had died. The write barrier
 * (xb_gc_write, xbrt.h) puts an old object that receives a store into the
 * remembered set, so the young objects it points to are found. A full
 * collection clears every mark first and traces the whole heap; it runs when
 * the old generation has grown by half the live data the previous full one
 * found, and on the other triggers XB_GC_FULL_ALLOC and the next ones say.
 *
 * Collection. Marking starts from the item frames and C roots of every
 * registered context (every task, running or suspended) and from the global
 * roots, and uses an explicit stack. Marking an object marks the lines it
 * covers with the collection's epoch, and an old one makes them old lines.
 * Then a line is free when it is not old and no object reached in this
 * collection covers it: sweeping a block reads its line marks, never its
 * objects. A young collection only sweeps the blocks that received objects or
 * hold aged ones since their last sweep (old lines stay until a full
 * collection, which clears them and walks the objects through the start bits
 * to clear their marks). Large objects are swept right away. Collections are
 * triggered by the bytes allocated since the last one.
 *
 * Evacuation. Objects of up to several lines need holes of as many free
 * lines, so dead objects spread over many blocks leave memory the heap cannot
 * use (it would grow instead). A full collection defragments when it can: it
 * picks the emptiest blocks of lines (at most half their lines live), as many
 * as the free blocks and a reserve of new ones can take, and moves every
 * object it reaches there into free blocks, its old place forwarding to the
 * copy; every reference it sees is set to the copy (roots, fields, an array's
 * own items, a hash's tables in its store, a codeblock's cells), and the
 * blocks it emptied are free. When the heap is to grow while a quarter of it
 * is free lines of blocks it may empty, such a full collection runs first.
 * A size class's sparsest blocks are emptied into its fuller ones the same
 * way (pick_class_defrag). Large objects and those too small for the copy's
 * address never move. It runs when every attached thread's stack is
 * known, and not while C code holds the collector (xb_gc_hold): it first scans
 * those stacks (registers saved on them) for words pointing into the objects
 * it may move and pins those, the compiled code's frames aside (precise
 * roots), and pins the objects the C roots' addresses lie in.
 *
 * Threads. Global structures are protected by one lock; the allocation fast
 * path takes none. Threads that run xBase code attach. A collection stops the
 * world: the collecting thread raises XB_PENDING_SAFEPOINT on the contexts of
 * the other threads still running (a request a thread serves for nothing costs
 * its loops, docs/compiler.md) and waits until no other thread runs; each one
 * parks at its next safepoint
 * (a poll of compiled code, an allocation's slow path), or is blocked already
 * (xb_blocking_run, whose operation touches only rooted objects). The write
 * barrier lists objects in a list of the thread's own, merged when a
 * collection starts. */
#define XB_GC_ROOTS_DEFINED /* the exported functions are defined here */
#include "xbrt_int.h"

#include "core/memvar.h"
#include "core/sched.h"
#include "core/shared.h"

#include "rdd/rdd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XB_GC_BLOCK_SIZE ((size_t)32 << 10)
#define XB_GC_LINE_SIZE ((size_t)128)
#define XB_GC_LINES (XB_GC_BLOCK_SIZE / XB_GC_LINE_SIZE)
#define XB_GC_SMALL_MAX (2 * XB_GC_LINE_SIZE)     /* largest object in lines, header included */
#define XB_GC_CLASS_MIN_SLOTS 4u                  /* slots of the largest size class's block */
#define XB_GC_CLASS_STEP 8u                       /* a class is at least 1/8 larger than the one before */
#define XB_GC_CLASSES_MAX 64u
#define XB_GC_CHUNK_BLOCKS 32u                    /* blocks obtained from the system at a time */
#define XB_GC_RETAIN_MIN 32u                      /* free blocks a collection never gives back */
/* Bytes allocated between young collections: they adapt to the survival
 * rate: they double while most young objects survive (collecting them early
 * only promotes them) and halve while few do. They never exceed a quarter of
 * the live data the last full collection found (nor go below the minimum), so
 * the heap stays near the live data: the old generation grows to half again
 * the live data before a full collection, the young one to a quarter of it. */
#define XB_GC_YOUNG_MIN ((size_t)4 << 20)
#define XB_GC_YOUNG_SHARE 4u                      /* of the live data: the most the young budget takes */
#define XB_GC_FULL_GROWTH 2u                      /* old growth past the live data that starts a full one: 1/2 */
/* A full collection also runs when as much as XB_GC_FULL_ALLOC times the live data (at least XB_GC_MIN_FULL) was
 * allocated since the last one, or XB_GC_FULL_EVERY_NS passed with a young budget allocated, or this many objects with
 * destructors were made old (or half as many as were old already, when more: a program that keeps making them does not
 * turn every young collection into a full one): old objects that died, and their destructors' resources (files,
 * connections), are never kept long by a program whose old generation stopped growing. */
#define XB_GC_FULL_ALLOC 8u
#define XB_GC_FULL_EVERY_NS (30 * XB_NS_PER_SEC)
#define XB_GC_FULL_FINALIZABLE 1024u
#define XB_GC_SURVIVAL_HIGH 2u                    /* survivors above 1/2 of the allocation */
#define XB_GC_SURVIVAL_LOW 8u                     /* survivors below 1/8 */
/* Aging turns off when the bytes promoted at a young collection exceed this
 * many times the aged bytes that died. */
#define XB_GC_AGING_WASTE 4u
#define XB_GC_MIN_FULL ((size_t)8 << 20)         /* old bytes that trigger the first full collection */
#define XB_GC_ALIGN 8u
#define XB_GC_DEFRAG_RESERVE 16u                 /* new blocks a defragmentation may take: the heap's blocks / this */
#define XB_GC_CLASS_DEFRAG_MIN 16u               /* size classes' blocks worth emptying (a move costs a stack scan) */
#define XB_GC_STOP_LOOK_NS (XB_NS_PER_MS)        /* a collector waiting for threads to stop looks again after this */

/* The metadata at the start of a block (its first XB_GC_FIRST_LINE lines). */
typedef struct xb_hblock {
  struct xb_hblock *next;            /* in the list of free or of recyclable blocks */
  uint32_t freeLines;               /* after its last sweep (none in a block of a size class) */
  uint8_t cls;                      /* 0: lines; k + 1: the slots of size class k */
  uint32_t used;                    /* a size class's: the slots taken */
  xb_gc_hdr *freeSlot;              /* a size class's: the first free slot (each free slot links the next) */
  uint8_t young;                    /* received objects, or holds aged ones, since its last sweep */
  uint8_t defrag;                   /* the full collection in progress moves its objects out (Evacuation above) */
  uint8_t lineMark[XB_GC_LINES];    /* the epoch of the last collection that reached an object in the line */
  uint8_t lineOld[XB_GC_LINES];     /* an old object covers the line */
  uint16_t starts[XB_GC_LINES];     /* bit k of a line: an object starts at its byte 8k */
} xb_hblock;
#define XB_GC_FIRST_LINE ((sizeof(xb_hblock) + XB_GC_LINE_SIZE - 1) / XB_GC_LINE_SIZE)
_Static_assert(XB_GC_LINE_SIZE / XB_GC_ALIGN == 16, "one start bit per 8 bytes of a line");
_Static_assert(XB_GC_FIRST_LINE * 4 < XB_GC_LINES, "the metadata leaves most of a block");
#define XB_GC_DATA_LINES (XB_GC_LINES - XB_GC_FIRST_LINE)          /* the lines of a block that hold objects */
#define XB_GC_DATA_BYTES (XB_GC_DATA_LINES * XB_GC_LINE_SIZE)

/* A chunk: XB_GC_CHUNK_BLOCKS blocks the system mapped together. A block is in use (heap.blocks), never handed out
 * yet (`fresh`: its pages untouched, so not resident), or given back to the system (`idle`). */
typedef struct xb_chunk {
  char *start;
  uint32_t fresh, idle; /* bit k: block k */
} xb_chunk;
_Static_assert(XB_GC_CHUNK_BLOCKS <= 32, "a bit per block of a chunk");
#define XB_GC_CHUNK_BYTES (XB_GC_CHUNK_BLOCKS * XB_GC_BLOCK_SIZE)
#define XB_GC_CHUNK_ALL (XB_GC_CHUNK_BLOCKS == 32 ? UINT32_MAX : (1u << XB_GC_CHUNK_BLOCKS) - 1)

/* A large object: what it takes (a run of units in a chunk of large objects, below XB_GC_LARGE_MAP bytes; else a
 * mapping of its own), its list's link, its header, immediately followed by the object (8-aligned on every target). */
typedef struct xb_large {
  struct xb_large *next;
  size_t bytes;
  xb_gc_hdr hdr;
} xb_large;
_Static_assert(offsetof(xb_large, hdr) % 8 == 0, "a large object stays 8-aligned");

#define XB_GC_LARGE_UNIT ((size_t)4 << 10)
#define XB_GC_LARGE_CHUNK ((size_t)1 << 20)
#define XB_GC_LARGE_UNITS (XB_GC_LARGE_CHUNK / XB_GC_LARGE_UNIT)
#define XB_GC_LARGE_WORDS (XB_GC_LARGE_UNITS / 64)
#define XB_GC_LARGE_MAP (XB_GC_LARGE_CHUNK / 2)
#define XB_GC_LARGE_RETAIN ((size_t)1 << 20) /* free bytes of large objects' units a collection never gives back */

/* A chunk of large objects: bit u of `used`, unit u taken; of `idle`, given back to the system. */
typedef struct xb_lchunk {
  char *start;
  uint64_t used[XB_GC_LARGE_WORDS], idle[XB_GC_LARGE_WORDS];
  uint32_t usedUnits;
  uint32_t longest; /* no run of free units is longer (a bound, made exact when a search fails) */
} xb_lchunk;

/* An array traced a slice at a time (its items from `next` on): marking a long array queues XB_GC_SLICE of its items
 * at a time, not all of them, so the mark stack stays small. `young`: a slice found a young object. */
typedef struct xb_slice {
  xb_gc_hdr *h;
  size_t next;
  int young;
} xb_slice;
#define XB_GC_SLICE 4096u
#define XB_GC_SHRINK_MIN 65536u /* entries a collector's vector keeps whatever it used */

typedef struct xb_range { /* a run of words a scan for pinned objects leaves out */
  const char *start, *end;
} xb_range;

typedef struct xb_root {    /* items outside frames */
  struct xb_root *next;
  xb_item *items;
  size_t count;
} xb_root;

/* What a thread is doing, for a collection that stops the others (Threads above): running code that touches the heap
 * (it parks at its next safepoint), parked there, or in a blocking operation (it touches no object until it leaves). */
enum { T_RUNNING, T_PARKED, T_BLOCKED };

typedef struct xb_tlh {     /* a mutator thread's own part of the heap (xb_gc_thread_attach) */
  struct xb_tlh *next;
  uint32_t state;           /* T_*: written by the thread, read by a collector (sequentially consistent) */
  int blocking;             /* in xb_blocking_run */
  uint32_t unsaved;         /* in it without its registers saved (it was the only thread): nothing moves meanwhile */
  const char *stackBase;    /* the end of its stack (it grows down from there); NULL: unknown */
  const char *stackTop;     /* parked or blocked: the lowest address of its stack in use, its registers saved above */
  /* Old objects it recorded with the write barrier since the last collection (merged then into the remembered set). */
  xb_gc_hdr **remembered;
  size_t rememberedLen, rememberedCap;
  char *cursor, *limit;     /* the hole being filled */
  xb_hblock *block;          /* its block */
  /* The overflow run: objects above a line that do not fit the hole, in holes large enough for them. */
  char *overCursor, *overLimit;
  xb_hblock *overBlock;
  xb_hblock *cls[XB_GC_CLASSES_MAX]; /* the block each size class takes slots from */
  size_t allocated;         /* bytes since it last reported to the global count */
} xb_tlh;

static struct {
  xb_mutex lock;
  xb_hblock **blocks;               /* every block in use (not given back, not one a chunk never handed out) */
  size_t blockCount, blockCap;
  xb_hblock *freeBlocks;            /* no live object */
  xb_hblock *recyclable;            /* free lines among live ones */
  xb_chunk *chunks;                /* in address order */
  size_t spare;                    /* blocks of the chunks not in use: never handed out, or given back */
  size_t freshBlocks;              /* of those, the ones never handed out */
  size_t chunkHint;                /* no chunk before this one has a spare block */
  uint32_t classSize[XB_GC_CLASSES_MAX]; /* each size class's slot, header included */
  unsigned classes;
  uint8_t classOf[XB_GC_CLASSES_MAX * 64]; /* by (bytes - 1) / XB_GC_ALIGN above XB_GC_SMALL_MAX: the class */
  size_t classMax;                 /* the largest object in a size class */
  xb_hblock *classBlocks[XB_GC_CLASSES_MAX]; /* blocks of each class with free slots */
  size_t chunkCount, chunkCap;
  uint8_t epoch;                   /* of the last collection, from 1 (lines start at 0, never current) */
  xb_large *largeYoung, *largeOld;
  xb_lchunk *lchunks;              /* in address order */
  size_t lchunkCount, lchunkCap;
  size_t largeMapped;              /* bytes of the large objects with a mapping of their own */
  xb_tlh *threads;
  xb_ctx *contexts;
  xb_root *roots;
  size_t allocated;                /* bytes allocated since the last collection */
  size_t youngBudget;              /* allocation that triggers a young collection */
  size_t oldBytes;                 /* estimated size of the old generation */
  size_t fullThreshold;            /* old bytes that trigger a full collection */
  size_t markedBytes;              /* marked by the current collection */
  size_t markedObjects;
  size_t liveObjects;              /* found by the last full collection */
  xb_gc_hdr **stack;               /* marking stack */
  size_t stackLen, stackCap;
  xb_slice *slices;                /* long arrays traced a slice at a time */
  size_t slicesLen, slicesCap;
  size_t candidateLines;           /* free lines of the blocks a full collection may empty, as the last sweep found */
  xb_gc_hdr **remembered;          /* old objects that received stores */
  size_t rememberedLen, rememberedCap;
  int full;                        /* the collection in progress is a full one */
  int aging;                       /* young collections age objects instead of promoting them at once */
  xb_gc_hdr **survivors;           /* young objects the young collection in progress reached */
  size_t survivorsLen, survivorsCap;
  xb_gc_hdr **aged;                /* objects that aged at the last young collection */
  size_t agedLen, agedCap;
  xb_gc_hdr **retained;            /* old objects that keep pointing to young ones, with how to record them */
  uint8_t *retainedHow;
  size_t retainedLen, retainedCap;
  xb_gc_hdr **finalizable;         /* objects to finalize when a collection finds them unreachable: young ones */
  size_t finalizableLen, finalizableCap;
  xb_gc_hdr **finalizableOld;      /* old ones: only a full collection may find them unreachable */
  size_t finalizableOldLen, finalizableOldCap;
  size_t finalizablePromoted;      /* objects with destructors made old since the last full collection */
  size_t liveBytes;                /* found by the last full collection */
  int64_t lastFullNs;              /* when it ran */
  uint32_t finalizing;             /* a context runs destructors (xb_gc_finalizing) */
  int reap;                        /* the collection found tasks' environments to close (xb_env_reap) */
  xb_item *finalized;              /* found unreachable, kept alive (roots) until they are finalized, in order */
  size_t finalizedHead, finalizedLen, finalizedCap;
  int evacuating;                  /* the collection in progress moves objects out of the blocks marked defrag */
  xb_hblock **defrag;              /* those blocks */
  size_t defragLen, defragCap;
  xb_hblock **classDest;           /* by size class, the blocks of the class its sparse blocks' objects move to */
  size_t classDestLen, classDestCap;
  size_t classDestFrom[XB_GC_CLASSES_MAX], classDestTo[XB_GC_CLASSES_MAX]; /* class c's: [from, to) of classDest */
  uint64_t allocatedSinceFull;     /* bytes allocated since the last full collection (64 bits: 32-bit targets too) */
  unsigned holds;                  /* xb_gc_hold: none moves */
  char *copyCursor, *copyLimit;    /* where the collection copies objects: free blocks */
  xb_gc_hdr **pinned;              /* pinned by the collection in progress */
  size_t pinnedLen, pinnedCap;
  xb_range *frames;                /* the frames' slots on the stacks being scanned, by address */
  size_t framesLen, framesCap;
  uint32_t stopping;               /* a collection stops the other threads (sequentially consistent) */
  uint32_t threadCount;            /* attached threads */
  xb_cond stopped;                 /* a thread parked or blocked: the collector counts them again */
  xb_cond resumed;                 /* the collection ended: parked threads go on */
  xb_ctx *finalizer;               /* the context destructors run in (the first registered) */
} heap;

static XB_THREAD_LOCAL xb_tlh *tlh;

_Noreturn void xb_gc_fatal(const char *why) {
  fflush(stdout);
  fprintf(stderr, "xbrt: %s\n", why);
  exit(EXIT_FAILURE);
}

static void out_of_memory(void) { xb_gc_fatal("out of memory"); }

static void *xmalloc(size_t n) {
  void *p = malloc(n);
  if (!p) out_of_memory();
  return p;
}

static void init_heap(void) {
  if (!xb_mutex_init(&heap.lock) || !xb_cond_init(&heap.stopped) || !xb_cond_init(&heap.resumed))
    xb_gc_fatal("cannot create the heap lock");
  heap.epoch = 1; /* never 0, which a line that no collection marked has */
  /* The size classes: each one the largest slot that as many slots fit a block's lines, the next at least an eighth
   * larger, up to the class that fits XB_GC_CLASS_MIN_SLOTS slots. */
  for (size_t want = XB_GC_SMALL_MAX + XB_GC_ALIGN; heap.classes < XB_GC_CLASSES_MAX;) {
    size_t slots = XB_GC_DATA_BYTES / want;
    if (slots < XB_GC_CLASS_MIN_SLOTS) break;
    size_t size = (XB_GC_DATA_BYTES / slots) & ~(size_t)(XB_GC_ALIGN - 1);
    heap.classSize[heap.classes++] = (uint32_t)size;
    want = (size + size / XB_GC_CLASS_STEP + XB_GC_ALIGN - 1) & ~(size_t)(XB_GC_ALIGN - 1);
  }
  heap.classMax = heap.classSize[heap.classes - 1];
  if ((heap.classMax - XB_GC_SMALL_MAX) / XB_GC_ALIGN > sizeof heap.classOf) xb_gc_fatal("size classes");
  for (unsigned c = 0, k = 0; k < (heap.classMax - XB_GC_SMALL_MAX) / XB_GC_ALIGN; ++k) {
    size_t bytes = XB_GC_SMALL_MAX + (k + 1) * XB_GC_ALIGN;
    while (heap.classSize[c] < bytes) ++c;
    heap.classOf[k] = (uint8_t)c;
  }
  heap.fullThreshold = XB_GC_MIN_FULL;
  heap.youngBudget = XB_GC_YOUNG_MIN;
  heap.aging = 1;
  heap.lastFullNs = xb_platform_monotonic_ns();
}

void xb_gc_init(void) {
  static xb_once_flag made = XB_ONCE_INIT;
  xb_once(&made, init_heap);
}

static xb_tlh *thread_heap(void) {
  if (!tlh) xb_gc_fatal("a thread allocates without xb_gc_thread_attach");
  return tlh;
}

/* Parks the calling thread while a collection another thread makes runs (heap.stopping): its registers are saved on
 * its stack first (xb_platform_with_registers), and the scan of its stack starts below them. Under the lock. */
static void park_locked(void *arg) {
  xb_tlh *t = arg;
  t->stackTop = (const char *)&arg;
  xb_atomic_store32(&t->state, T_PARKED, XB_SEQ_CST);
  xb_cond_broadcast(&heap.stopped);
  while (xb_atomic_load32(&heap.stopping, XB_SEQ_CST)) xb_cond_wait(&heap.resumed, &heap.lock);
  xb_atomic_store32(&t->state, T_RUNNING, XB_SEQ_CST);
  t->stackTop = NULL;
  xb_cond_broadcast(&heap.stopped); /* a collector waiting for the threads it stopped last to go on (collect) */
}

/* Whether a thread other than the caller is still parked by the last collection (it was woken, not yet on its way). */
static int others_parked(const xb_tlh *self) {
  for (const xb_tlh *t = heap.threads; t; t = t->next)
    if (t != self && xb_atomic_load32(&t->state, XB_SEQ_CST) == T_PARKED) return 1;
  return 0;
}

/* The lock, taken by a thread between collections (another thread's collection in progress is waited for, parked):
 * to change what a collection sees (the contexts, the threads), or to collect (`collecting`: then also once the
 * threads the last collection parked have gone on, so that a thread that collects again and again, __GcCollect() in a
 * loop, never keeps the others parked from one collection to the next). Returns whether it waited for a collection. */
static int lock_between(int collecting) {
  xb_mutex_lock(&heap.lock);
  int waited = 0;
  for (;;) {
    if (heap.stopping) {
      waited = 1;
      if (tlh) xb_platform_with_registers(park_locked, tlh);
      else xb_cond_wait(&heap.resumed, &heap.lock);
    } else if (collecting && others_parked(tlh)) {
      xb_cond_wait_ns(&heap.stopped, &heap.lock, XB_GC_STOP_LOOK_NS); /* a parked thread tells as it leaves */
    } else {
      return waited;
    }
  }
}

static int lock_between_collections(void) { return lock_between(0); }

/* Stops every other thread for a collection, with the lock held: each one parks at its next safepoint (the request in
 * its context's pending bits) or is blocked already. A thread that leaves a blocking operation meanwhile finds
 * `stopping` set and parks (the store of its state and the load of `stopping` are ordered against the collector's). */
static void stop_world(xb_tlh *self) {
  xb_atomic_store32(&heap.stopping, 1, XB_SEQ_CST);
  for (;;) {
    /* Raised again at each look: a thread that served the request before `stopping` reached it sees it again. A
     * blocked thread needs none: it parks on its way back. */
    for (xb_ctx *ctx = heap.contexts; ctx; ctx = ctx->gcNext) {
      const xb_tlh *t = ctx->gcThread;
      if (!t || (t != self && xb_atomic_load32(&t->state, XB_SEQ_CST) == T_RUNNING))
        xb_pending_set(ctx, XB_PENDING_SAFEPOINT);
    }
    int running = 0;
    for (xb_tlh *t = heap.threads; t && !running; t = t->next)
      running = t != self && xb_atomic_load32(&t->state, XB_SEQ_CST) == T_RUNNING;
    if (!running) return;
    xb_cond_wait_ns(&heap.stopped, &heap.lock, XB_GC_STOP_LOOK_NS);
  }
}

/* Whether every thread's stack can be scanned, its registers saved on it: a collection moves objects only then. Under
 * the lock, the other threads stopped. */
static int stacks_known(void) {
  for (const xb_tlh *t = heap.threads; t; t = t->next)
    if (!t->stackBase || xb_atomic_load32(&t->unsaved, XB_RELAXED)) return 0; /* also read between collections */
  return heap.threads != NULL;
}

static void start_world(void) {
  xb_atomic_store32(&heap.stopping, 0, XB_SEQ_CST);
  xb_cond_broadcast(&heap.resumed);
}

uint32_t xb_storage_shared;

/* Publication (docs/concurrency.md): the program's storage becomes shared storage when a second thread attaches,
 * every other thread stopped (the lock held): what the roots outside frames hold (the STATICs, the class variables),
 * every context's memory variables and the PUBLICs are shared, then the flag tells compiled code and the runtime.
 * Sharing allocates nothing. */
static void publish(void) {
  stop_world(NULL);
  for (xb_root *r = heap.roots; r; r = r->next)
    for (size_t i = 0; i < r->count; ++i) xb_share(&r->items[i]);
  for (xb_ctx *ctx = heap.contexts; ctx; ctx = ctx->gcNext) xb_memvar_share(ctx);
  xb_memvar_share_publics();
  xb_atomic_store32(&xb_storage_shared, 1, XB_RELEASE);
  start_world();
}

void xb_gc_safepoint(void) {
  xb_tlh *t = tlh;
  if (!t || !xb_atomic_load32(&heap.stopping, XB_SEQ_CST)) return;
  xb_mutex_lock(&heap.lock);
  if (heap.stopping) xb_platform_with_registers(park_locked, t);
  xb_mutex_unlock(&heap.lock);
}

typedef struct blocked_call {
  void (*fn)(void *);
  void *arg;
  xb_tlh *t;
} blocked_call;

/* The blocking operation, with the thread's registers saved on its stack (xb_platform_with_registers) for as long as it
 * runs: the thread publishes where its stack starts to be in use and that it is blocked (release: a collector that
 * reads the state before it looks again within XB_GC_STOP_LOOK_NS), runs it, and on its way back parks if a
 * collection runs (that state and the collector's `stopping` are sequentially consistent, so neither misses the
 * other: the thread touches the heap again only when no collection counts it as stopped). */
static void run_blocked(void *p) {
  blocked_call *c = p;
  xb_tlh *t = c->t;
  t->stackTop = (const char *)&p;
  xb_atomic_store32(&t->state, T_BLOCKED, XB_RELEASE);
  if (xb_atomic_load32(&heap.stopping, XB_RELAXED)) { /* a collector waits for this thread: no need to wait for it */
    xb_mutex_lock(&heap.lock);
    xb_cond_broadcast(&heap.stopped);
    xb_mutex_unlock(&heap.lock);
  }
  c->fn(c->arg);
  xb_atomic_store32(&t->state, T_RUNNING, XB_SEQ_CST);
  if (xb_atomic_load32(&heap.stopping, XB_SEQ_CST)) { /* a collection runs: wait for it, still as blocked */
    xb_mutex_lock(&heap.lock);
    if (heap.stopping) {
      xb_atomic_store32(&t->state, T_BLOCKED, XB_SEQ_CST);
      xb_cond_broadcast(&heap.stopped);
      while (xb_atomic_load32(&heap.stopping, XB_SEQ_CST)) xb_cond_wait(&heap.resumed, &heap.lock);
      xb_atomic_store32(&t->state, T_RUNNING, XB_SEQ_CST);
    }
    xb_mutex_unlock(&heap.lock);
  }
  t->stackTop = NULL;
}

/* The registers are saved only while other threads are attached: with this one alone, nothing collects beside it, and
 * one that attaches meanwhile moves nothing until the operation ends (stacks_known). */
static void run_stopped(void (*fn)(void *), void *arg, int away) {
  xb_tlh *t = tlh;
  if (!t || t->blocking) { /* not attached, or inside a blocking operation already */
    fn(arg);
    return;
  }
  blocked_call c = {fn, arg, t};
  if (away) xb_sched_blocking(1); /* a worker running a task: another may take its place meanwhile */
  t->blocking = 1;
  if (xb_atomic_load32(&heap.threadCount, XB_RELAXED) > 1) {
    xb_platform_with_registers(run_blocked, &c);
  } else {
    xb_atomic_store32(&t->unsaved, 1, XB_RELAXED);
    run_blocked(&c);
    xb_atomic_store32(&t->unsaved, 0, XB_RELAXED);
  }
  t->blocking = 0;
  if (away) xb_sched_blocking(0);
}

void xb_blocking_run(void (*fn)(void *), void *arg) { run_stopped(fn, arg, 1); }

void xb_waiting_run(void (*fn)(void *), void *arg) { run_stopped(fn, arg, 0); }

void xb_gc_thread_attach(const void *base) {
  xb_gc_init();
  if (tlh) xb_gc_fatal("a thread attached twice");
  xb_tlh *t = xmalloc(sizeof *t);
  memset(t, 0, sizeof *t);
  const char *low, *high;
  t->stackBase = xb_platform_stack_bounds(&low, &high) ? high : base; /* the system's end of the stack, when it tells */
  lock_between_collections(); /* not attached yet: it waits unparked, and no collection counts it */
  if (heap.threadCount == 1 && !xb_atomic_load32(&xb_storage_shared, XB_RELAXED)) publish(); /* a second thread */
  t->next = heap.threads;
  heap.threads = t;
  xb_atomic_add32(&heap.threadCount, 1, XB_RELAXED);
  xb_mutex_unlock(&heap.lock);
  tlh = t;
}

static void merge_remembered(xb_tlh *t);
static void report_allocation(xb_tlh *t);
static void collect(int full, int due);

void xb_gc_thread_detach(void) {
  xb_tlh *t = tlh;
  if (!t) return;
  lock_between_collections();
  report_allocation(t);
  merge_remembered(t);
  for (xb_tlh **p = &heap.threads; *p; p = &(*p)->next)
    if (*p == t) {
      *p = t->next;
      break;
    }
  xb_atomic_add32(&heap.threadCount, (uint32_t)-1, XB_RELAXED);
  for (xb_ctx *ctx = heap.contexts; ctx; ctx = ctx->gcNext)
    if (ctx->gcThread == t) ctx->gcThread = NULL; /* still registered: asked like an unattached thread's from now on */
  xb_mutex_unlock(&heap.lock);
  free(t->remembered);
  free(t);
  tlh = NULL;
}

static void report_allocation(xb_tlh *t) {
  heap.allocated += t->allocated;
  t->allocated = 0;
}

/* Appends to one of the collector's pointer stacks, growing it. */
static void stack_push(xb_gc_hdr ***stack, size_t *len, size_t *cap, xb_gc_hdr *h) {
  if (*len == *cap) {
    size_t n = *cap ? *cap * 2 : 4096;
    xb_gc_hdr **s = realloc(*stack, n * sizeof *s);
    if (!s) out_of_memory();
    *stack = s;
    *cap = n;
  }
  (*stack)[(*len)++] = h;
}

static xb_hblock *block_of(const void *p) {
  return (xb_hblock *)((uintptr_t)p & ~(uintptr_t)(XB_GC_BLOCK_SIZE - 1));
}
static size_t line_of(const void *p) { return ((uintptr_t)p & (XB_GC_BLOCK_SIZE - 1)) / XB_GC_LINE_SIZE; }
static char *block_data(xb_hblock *b) { return (char *)b + XB_GC_FIRST_LINE * XB_GC_LINE_SIZE; }

/* Header and object, 8-byte aligned: what a block object takes. */
static size_t object_total(size_t size) {
  return sizeof(xb_gc_hdr) + ((size + XB_GC_ALIGN - 1) & ~(size_t)(XB_GC_ALIGN - 1));
}

/* Where an object holds its header's link: the next free slot of a size class's block, or the copy of a moved object.
 * Past a string's flags, which stay readable: a string's XB_STR_STATIC is tested before its header (a literal has
 * none), also in a moved string's old place. */
#define XB_GC_LINK_AT offsetof(xb_str, len)

static xb_gc_hdr *link_of(const xb_gc_hdr *h) {
  xb_gc_hdr *to;
  memcpy(&to, (const char *)(h + 1) + XB_GC_LINK_AT, sizeof to);
  return to;
}
static void set_link(xb_gc_hdr *h, xb_gc_hdr *to) { memcpy((char *)(h + 1) + XB_GC_LINK_AT, &to, sizeof to); }

/* Whether nothing live covers line `l` of `b`: no old object, and no object
 * the last collection reached. */
static int line_free(const xb_hblock *b, size_t l) { return !b->lineOld[l] && b->lineMark[l] != heap.epoch; }

/* A new chunk from the system, among the others in address order, its blocks all fresh: 0 when the system has no
 * memory. Under the lock. */
static int add_chunk(void) {
  char *start = xb_platform_pages_map(XB_GC_CHUNK_BYTES, XB_GC_BLOCK_SIZE);
  if (!start) return 0;
  if (heap.chunkCount == heap.chunkCap) {
    size_t n = heap.chunkCap ? heap.chunkCap * 2 : 64;
    xb_chunk *list = realloc(heap.chunks, n * sizeof *list);
    if (!list) out_of_memory();
    heap.chunks = list;
    heap.chunkCap = n;
  }
  size_t k = heap.chunkCount++;
  for (; k > 0 && heap.chunks[k - 1].start > start; --k) heap.chunks[k] = heap.chunks[k - 1];
  heap.chunks[k] = (xb_chunk){start, XB_GC_CHUNK_ALL, 0};
  heap.spare += XB_GC_CHUNK_BLOCKS;
  heap.freshBlocks += XB_GC_CHUNK_BLOCKS;
  if (k < heap.chunkHint) heap.chunkHint = k;
  return 1;
}

/* The chunk an address lies in, or NULL. */
static xb_chunk *chunk_of(uintptr_t w) {
  size_t lo = 0, hi = heap.chunkCount;
  while (lo < hi) { /* the last chunk starting at or before the address */
    size_t mid = (lo + hi) / 2;
    if ((uintptr_t)heap.chunks[mid].start <= w) lo = mid + 1;
    else hi = mid;
  }
  if (!lo || w >= (uintptr_t)heap.chunks[lo - 1].start + XB_GC_CHUNK_BYTES) return NULL;
  return &heap.chunks[lo - 1];
}

static uint32_t block_bit(const xb_chunk *c, const void *b) {
  return 1u << (((uintptr_t)b - (uintptr_t)c->start) / XB_GC_BLOCK_SIZE);
}

/* Whether a full collection may empty block `b` (Evacuation above): a block of lines, partly used, at most half of
 * its lines live (as its last sweep found them). */
static int defrag_candidate(const xb_hblock *b) {
  return !b->cls && b->freeLines * 2 >= XB_GC_DATA_LINES && b->freeLines != XB_GC_DATA_LINES;
}

/* Whether the heap, which is to grow (no free or recyclable block, no block a chunk never handed out: taking one back
 * from the system would be growing too), is fragmented: a quarter of it free lines of blocks a full collection may
 * empty, which objects of several lines may not fit, a young budget allocated since the last full collection (not to
 * collect over and over), and the collection able to move objects. A full collection that defragments comes first then.
 * Under the lock. */
static int fragmented(void) {
  if (heap.freshBlocks || heap.allocatedSinceFull + heap.allocated < heap.youngBudget || heap.holds ||
      !stacks_known())
    return 0;
  return heap.candidateLines * XB_GC_LINE_SIZE * 4 > heap.blockCount * XB_GC_BLOCK_SIZE;
}

/* A block with every line free: a free one, else the first spare block of the lowest chunk that has one (made usable
 * again when it was given back), else one of a new chunk; NULL when the system has no memory. Under the lock. */
static xb_hblock *block_new(void) {
  xb_hblock *b = heap.freeBlocks;
  if (b) {
    heap.freeBlocks = b->next;
  } else {
    if (!heap.spare && !add_chunk()) return NULL;
    while ((heap.chunks[heap.chunkHint].fresh | heap.chunks[heap.chunkHint].idle) == 0) ++heap.chunkHint;
    xb_chunk *c = &heap.chunks[heap.chunkHint];
    uint32_t spare = c->fresh | c->idle, bit = spare & (~spare + 1); /* the lowest */
    unsigned k = 0;
    while (!((bit >> k) & 1u)) ++k;
    b = (xb_hblock *)(c->start + k * XB_GC_BLOCK_SIZE);
    if ((c->idle & bit) && !xb_platform_pages_recommit(b, XB_GC_BLOCK_SIZE)) return NULL;
    if (c->fresh & bit) --heap.freshBlocks;
    c->fresh &= ~bit;
    c->idle &= ~bit;
    --heap.spare;
    memset(b, 0, sizeof *b);
    if (heap.blockCount == heap.blockCap) {
      size_t n = heap.blockCap ? heap.blockCap * 2 : 64;
      xb_hblock **list = realloc(heap.blocks, n * sizeof *list);
      if (!list) out_of_memory();
      heap.blocks = list;
      heap.blockCap = n;
    }
    heap.blocks[heap.blockCount++] = b;
  }
  b->next = NULL;
  b->young = 1;
  return b;
}

/* The system has no memory for a new block: a full collection (which frees what it can and gives free blocks back)
 * the first time for an allocation (`*collected`), the program's end the second. */
static void out_of_blocks(int *collected) {
  if (*collected) out_of_memory();
  *collected = 1;
  collect(1, 0);
}

/* The free bytes from `cursor` to `limit` (both null before a first hole). */
static size_t room(const char *cursor, const char *limit) { return (size_t)((uintptr_t)limit - (uintptr_t)cursor); }

/* A run of the thread's allocation: the first hole of `b` from line `from` of
 * at least `min` bytes (smaller ones are left for another run) becomes
 * [*cursor, *limit): true when there is one. Its lines lose the start bits of
 * the objects that died there. */
static int take_hole(xb_hblock *b, size_t from, size_t min, char **cursor, char **limit) {
  size_t l = from < XB_GC_FIRST_LINE ? XB_GC_FIRST_LINE : from;
  for (;;) {
    while (l < XB_GC_LINES && !line_free(b, l)) ++l;
    if (l == XB_GC_LINES) return 0;
    size_t end = l;
    while (end < XB_GC_LINES && line_free(b, end)) ++end;
    if ((end - l) * XB_GC_LINE_SIZE >= min) {
      memset(&b->starts[l], 0, (end - l) * sizeof b->starts[0]);
      *cursor = (char *)b + l * XB_GC_LINE_SIZE;
      *limit = (char *)b + end * XB_GC_LINE_SIZE;
      b->young = 1;
      return 1;
    }
    l = end;
  }
}

static void set_start(const void *p) {
  xb_hblock *b = block_of(p);
  uintptr_t off = (uintptr_t)p & (XB_GC_LINE_SIZE - 1);
  b->starts[line_of(p)] |= (uint16_t)(1u << (off / XB_GC_ALIGN));
}

/* The allocation that left the hole: the overflow block for an object above a
 * line, the next hole of the block, or another block (a recyclable one first);
 * a young collection runs first when the allocation since the last one
 * reached the budget. */
static xb_gc_hdr *alloc_slow(xb_tlh *t, size_t total) {
  xb_gc_safepoint(); /* a collection another thread asked for: an allocation is a safepoint */
  int medium = total > XB_GC_LINE_SIZE, collected = 0;
  for (;;) {
    char **cursor = medium && total > room(t->cursor, t->limit) ? &t->overCursor : &t->cursor;
    char **limit = cursor == &t->cursor ? &t->limit : &t->overLimit;
    if (total <= room(*cursor, *limit)) {
      xb_gc_hdr *h = (xb_gc_hdr *)*cursor;
      *cursor += total;
      return h;
    }
    xb_hblock **block = medium ? &t->overBlock : &t->block; /* the next hole of the run's block */
    if (*block && take_hole(*block, line_of(*limit - 1) + 1, total, cursor, limit)) continue;
    xb_mutex_lock(&heap.lock);
    report_allocation(t);
    int due = heap.allocated >= heap.youngBudget;
    xb_mutex_unlock(&heap.lock);
    if (due) {
      collect(0, 1); /* retires every thread's holes */
      continue;
    }
    xb_mutex_lock(&heap.lock);
    int defrag = !heap.recyclable && !heap.freeBlocks && fragmented();
    xb_mutex_unlock(&heap.lock);
    if (defrag) { /* rather than growing the heap */
      collect(1, 1);
      continue;
    }
    /* Another block: a recyclable one first (a new one has a hole for any object); a recyclable block without a
     * hole large enough leaves the list until the next sweep. */
    xb_mutex_lock(&heap.lock);
    xb_hblock *b;
    do {
      b = heap.recyclable;
      if (b) heap.recyclable = b->next;
      else b = block_new();
    } while (b && !take_hole(b, XB_GC_FIRST_LINE, total, cursor, limit));
    *block = b;
    xb_mutex_unlock(&heap.lock);
    if (!b) out_of_blocks(&collected);
  }
}

/* Makes a free block one of size class `c`: its slots, all free. Under the lock. */
static void class_block_init(xb_hblock *b, unsigned c) {
  size_t size = heap.classSize[c];
  char *data = block_data(b);
  size_t slots = XB_GC_DATA_BYTES / size;
  b->cls = (uint8_t)(c + 1);
  b->used = 0;
  b->freeLines = 0;
  b->freeSlot = NULL;
  for (size_t i = slots; i-- > 0;) {
    xb_gc_hdr *h = (xb_gc_hdr *)(data + i * size);
    h->kind = 0; /* a free slot */
    set_link(h, b->freeSlot);
    b->freeSlot = h;
  }
}

/* A slot of size class `c` for an object: from the thread's block of the class, else another block of it with free
 * slots, else a free block made one; a young collection runs first when the allocation since the last one reached the
 * budget. */
static xb_gc_hdr *alloc_class(xb_tlh *t, unsigned c) {
  int collected = 0;
  for (;;) {
    xb_hblock *b = t->cls[c];
    if (b && b->freeSlot) {
      xb_gc_hdr *h = b->freeSlot;
      b->freeSlot = link_of(h);
      ++b->used;
      return h;
    }
    xb_gc_safepoint(); /* a new block: a safepoint, as the slow path of the others */
    xb_mutex_lock(&heap.lock);
    report_allocation(t);
    int due = heap.allocated >= heap.youngBudget;
    if (!due) {
      b = heap.classBlocks[c];
      if (b) {
        heap.classBlocks[c] = b->next;
      } else if ((b = block_new()) != NULL) {
        class_block_init(b, c);
      }
      if (b) b->young = 1;
      t->cls[c] = b;
    }
    xb_mutex_unlock(&heap.lock);
    if (due) collect(0, 1); /* retires every thread's blocks */
    else if (!b) out_of_blocks(&collected);
  }
}

static int unit_used(const uint64_t *bits, uint32_t u) { return (int)((bits[u / 64] >> (u % 64)) & 1u); }

/* Sets (`on`) or clears bits [from, from + n) of a chunk of large objects' map. */
static void units_mark(uint64_t *bits, uint32_t from, uint32_t n, int on) {
  for (uint32_t u = from; u < from + n; ++u) {
    uint64_t bit = (uint64_t)1 << (u % 64);
    bits[u / 64] = on ? bits[u / 64] | bit : bits[u / 64] & ~bit;
  }
}

/* The first run of `n` free units of `c`, or XB_GC_LARGE_UNITS (then c->longest is the longest run it has). */
static uint32_t units_find(xb_lchunk *c, uint32_t n) {
  uint32_t run = 0, longest = 0;
  for (uint32_t u = 0; u < XB_GC_LARGE_UNITS; ++u) {
    if (c->used[u / 64] == UINT64_MAX) { /* 64 taken: the next word */
      run = 0;
      u += 63 - u % 64;
      continue;
    }
    run = unit_used(c->used, u) ? 0 : run + 1;
    if (run > longest) longest = run;
    if (run == n) return u + 1 - n;
  }
  c->longest = longest;
  return (uint32_t)XB_GC_LARGE_UNITS;
}

/* The chunk of large objects an address lies in (one that is there). */
static xb_lchunk *lchunk_of(const void *p) {
  size_t lo = 0, hi = heap.lchunkCount;
  while (lo + 1 < hi) {
    size_t mid = (lo + hi) / 2;
    if ((const char *)p >= heap.lchunks[mid].start) lo = mid;
    else hi = mid;
  }
  return &heap.lchunks[lo];
}

/* A new chunk of large objects, among the others in address order: NULL when the system has no memory. */
static xb_lchunk *lchunk_new(void) {
  char *start = xb_platform_pages_map(XB_GC_LARGE_CHUNK, xb_platform_page_size());
  if (!start) return NULL;
  if (heap.lchunkCount == heap.lchunkCap) {
    size_t n = heap.lchunkCap ? heap.lchunkCap * 2 : 16;
    xb_lchunk *list = realloc(heap.lchunks, n * sizeof *list);
    if (!list) out_of_memory();
    heap.lchunks = list;
    heap.lchunkCap = n;
  }
  size_t k = heap.lchunkCount++;
  for (; k > 0 && heap.lchunks[k - 1].start > start; --k) heap.lchunks[k] = heap.lchunks[k - 1];
  xb_lchunk *c = &heap.lchunks[k];
  memset(c, 0, sizeof *c);
  c->start = start;
  c->longest = (uint32_t)XB_GC_LARGE_UNITS;
  return c;
}

/* `bytes` (whole units) for a large object: the first run that holds them, from the lowest chunk, made usable again
 * where it was given back; NULL when the system has no memory. Under the lock. */
static xb_large *large_units(size_t bytes) {
  uint32_t n = (uint32_t)(bytes / XB_GC_LARGE_UNIT), at = (uint32_t)XB_GC_LARGE_UNITS;
  xb_lchunk *c = NULL;
  for (size_t k = 0; k < heap.lchunkCount && at == XB_GC_LARGE_UNITS; ++k)
    if (heap.lchunks[k].longest >= n) at = units_find(c = &heap.lchunks[k], n);
  if (at == XB_GC_LARGE_UNITS) {
    if (!(c = lchunk_new())) return NULL;
    at = 0;
  }
  char *p = c->start + (size_t)at * XB_GC_LARGE_UNIT;
  int idle = 0;
  for (uint32_t u = at; u < at + n && !idle; ++u) idle = unit_used(c->idle, u);
  if (idle) {
    if (!xb_platform_pages_recommit(p, bytes)) return NULL;
    units_mark(c->idle, at, n, 0);
  }
  units_mark(c->used, at, n, 1);
  c->usedUnits += n;
  return (xb_large *)p;
}

/* A large object's memory back: its units free, or its mapping unmapped. Under the lock, after its sweep. */
static void large_free(xb_large *l) {
  if (l->bytes >= XB_GC_LARGE_MAP) {
    heap.largeMapped -= l->bytes;
    xb_platform_pages_unmap(l, l->bytes);
    return;
  }
  xb_lchunk *c = lchunk_of(l);
  uint32_t n = (uint32_t)(l->bytes / XB_GC_LARGE_UNIT);
  units_mark(c->used, (uint32_t)((size_t)((char *)l - c->start) / XB_GC_LARGE_UNIT), n, 0);
  c->usedUnits -= n;
  c->longest = (uint32_t)XB_GC_LARGE_UNITS; /* not known any more */
}

/* After a collection's sweep: the free units of large objects beyond XB_GC_LARGE_RETAIN and the young budget go back
 * to the system, the highest chunks' first: a chunk with none in use is unmapped, else its runs of free units that
 * cover whole pages are given back (Large objects above). Under the lock, every other thread stopped. */
static void give_back_large(void) {
  size_t page = xb_platform_page_size(), committed = 0;
  for (size_t k = 0; k < heap.lchunkCount; ++k)
    for (uint32_t u = 0; u < XB_GC_LARGE_UNITS; ++u)
      committed += !unit_used(heap.lchunks[k].used, u) && !unit_used(heap.lchunks[k].idle, u);
  committed *= XB_GC_LARGE_UNIT;
  size_t retain = XB_GC_LARGE_RETAIN + heap.youngBudget;
  if (committed <= retain + retain / 2) return;
  size_t kept = heap.lchunkCount;
  for (size_t k = heap.lchunkCount; k-- > 0 && committed > retain;) {
    xb_lchunk *c = &heap.lchunks[k];
    if (!c->usedUnits) {
      for (uint32_t u = 0; u < XB_GC_LARGE_UNITS; ++u) committed -= !unit_used(c->idle, u) * XB_GC_LARGE_UNIT;
      xb_platform_pages_unmap(c->start, XB_GC_LARGE_CHUNK);
      c->start = NULL;
      --kept;
      continue;
    }
    for (uint32_t u = 0; u < XB_GC_LARGE_UNITS && committed > retain;) { /* each run of free units: its whole pages */
      if (unit_used(c->used, u)) {
        ++u;
        continue;
      }
      uint32_t end = u;
      while (end < XB_GC_LARGE_UNITS && !unit_used(c->used, end)) ++end;
      uintptr_t from = ((uintptr_t)(c->start + (size_t)u * XB_GC_LARGE_UNIT) + page - 1) & ~(uintptr_t)(page - 1);
      uintptr_t to = (uintptr_t)(c->start + (size_t)end * XB_GC_LARGE_UNIT) & ~(uintptr_t)(page - 1);
      if (to > from) {
        uint32_t first = (uint32_t)((from - (uintptr_t)c->start) / XB_GC_LARGE_UNIT);
        uint32_t count = (uint32_t)((to - from) / XB_GC_LARGE_UNIT);
        uint32_t fresh = 0;
        for (uint32_t v = first; v < first + count; ++v) fresh += !unit_used(c->idle, v);
        if (fresh) {
          xb_platform_pages_decommit((void *)from, (size_t)(to - from));
          units_mark(c->idle, first, count, 1);
          committed -= fresh * XB_GC_LARGE_UNIT;
        }
      }
      u = end;
    }
  }
  if (kept == heap.lchunkCount) return;
  size_t n = 0;
  for (size_t k = 0; k < heap.lchunkCount; ++k)
    if (heap.lchunks[k].start) heap.lchunks[n++] = heap.lchunks[k];
  heap.lchunkCount = n;
}

/* A large object of `size` bytes (Large objects above), in the young list; a young collection runs first when the
 * allocation since the last one reached the budget, a full one when the system has no memory, before the program ends.
 */
static void *alloc_large(size_t size) {
  xb_gc_safepoint();
  xb_mutex_lock(&heap.lock);
  heap.allocated += size;
  int due = heap.allocated >= heap.youngBudget;
  xb_mutex_unlock(&heap.lock);
  if (due) collect(0, 1);
  size_t total = offsetof(xb_large, hdr) + sizeof(xb_gc_hdr) + size;
  if (total > SIZE_MAX - XB_GC_LARGE_MAP) out_of_memory();
  size_t bytes = (total + XB_GC_LARGE_UNIT - 1) & ~(XB_GC_LARGE_UNIT - 1);
  if (bytes >= XB_GC_LARGE_MAP) {
    size_t page = xb_platform_page_size();
    bytes = (total + page - 1) & ~(page - 1);
  }
  for (int collected = 0;; out_of_blocks(&collected)) {
    xb_mutex_lock(&heap.lock);
    xb_large *l = bytes >= XB_GC_LARGE_MAP ? xb_platform_pages_map(bytes, xb_platform_page_size()) : large_units(bytes);
    if (l) {
      if (bytes >= XB_GC_LARGE_MAP) heap.largeMapped += bytes;
      l->bytes = bytes;
      l->next = heap.largeYoung;
      heap.largeYoung = l;
    }
    xb_mutex_unlock(&heap.lock);
    if (l) {
      l->hdr.flags = XB_GC_HDR_LARGE;
      return &l->hdr;
    }
  }
}

void *xb_gc_alloc(xb_gc_kind kind, size_t size) {
  if (size > SIZE_MAX / 2) out_of_memory();
  size_t total = object_total(size);
  xb_gc_hdr *h;
  if (total <= XB_GC_SMALL_MAX) {
    xb_tlh *t = thread_heap();
    if (total <= room(t->cursor, t->limit)) { /* the fast path: a bump in the hole */
      h = (xb_gc_hdr *)t->cursor;
      t->cursor += total;
    } else {
      h = alloc_slow(t, total);
    }
    t->allocated += total;
    set_start(h);
    h->flags = 0;
  } else if (total <= heap.classMax) {
    xb_tlh *t = thread_heap();
    unsigned c = heap.classOf[(total - XB_GC_SMALL_MAX - 1) / XB_GC_ALIGN];
    h = alloc_class(t, c);
    t->allocated += heap.classSize[c];
    h->flags = 0;
  } else {
    h = alloc_large(size);
  }
  h->kind = (uint8_t)kind;
  h->state = 0;
  h->size = (uint32_t)(size > UINT32_MAX ? UINT32_MAX : size);
  return h + 1;
}

/* After a collection that queued objects to finalize, or found tasks' environments to close (their tables, their
 * locks: XB_PENDING_FINALIZE closes them too, xb_env_reap): their destructors run on the program's context (the first
 * registered), at its next pending test; while its thread is in a blocking operation (a Join, a wait: it would not
 * test anything for as long), on the collecting thread's (`self`, the one that allocated), so that they never wait for
 * it and the allocating thread pays for the garbage it makes. One context runs destructors at a time
 * (xb_gc_finalizing). Under the lock, every other thread stopped. */
static void request_finalize(const xb_tlh *self) {
  int reap = heap.reap;
  heap.reap = 0;
  if ((heap.finalizedLen == heap.finalizedHead && !reap) || !heap.finalizer) return;
  xb_ctx *to = heap.finalizer;
  const xb_tlh *t = to->gcThread;
  if (t && t != self && xb_atomic_load32(&t->state, XB_SEQ_CST) == T_BLOCKED)
    for (xb_ctx *ctx = heap.contexts; ctx; ctx = ctx->gcNext)
      if (ctx->gcThread == self && self) {
        to = ctx;
        break;
      }
  xb_pending_set(to, XB_PENDING_FINALIZE);
}

int xb_gc_finalizing(int start) {
  if (!start) {
    xb_atomic_store32(&heap.finalizing, 0, XB_RELEASE);
    xb_mutex_lock(&heap.lock); /* objects queued while the turn was ending: not left for the next collection */
    int waiting = heap.finalizedLen > heap.finalizedHead;
    xb_mutex_unlock(&heap.lock);
    return waiting;
  }
  uint32_t idle = 0;
  return xb_atomic_cas32(&heap.finalizing, &idle, 1, XB_ACQUIRE);
}

void xb_gc_register(xb_ctx *ctx) {
  lock_between_collections();
  ctx->gcThread = tlh;
  ctx->gcPrev = NULL;
  ctx->gcNext = heap.contexts;
  if (heap.contexts) heap.contexts->gcPrev = ctx;
  heap.contexts = ctx;
  if (!heap.finalizer) heap.finalizer = ctx;
  xb_mutex_unlock(&heap.lock);
}

void xb_gc_unregister(xb_ctx *ctx) {
  lock_between_collections();
  if (ctx->gcPrev) ctx->gcPrev->gcNext = ctx->gcNext;
  else heap.contexts = ctx->gcNext;
  if (ctx->gcNext) ctx->gcNext->gcPrev = ctx->gcPrev;
  /* Destructors it was asked to run, or would have run as the finalizer, go to the finalizer that stays. */
  int handOver = (xb_pending_get(ctx) & XB_PENDING_FINALIZE) || heap.finalizer == ctx;
  if (heap.finalizer == ctx) heap.finalizer = heap.contexts;
  if (handOver && heap.finalizer && heap.finalizedLen > heap.finalizedHead)
    xb_pending_set(heap.finalizer, XB_PENDING_FINALIZE);
  xb_mutex_unlock(&heap.lock);
}

void xb_gc_raise_all(const xb_ctx *except, uint32_t bits) {
  lock_between_collections();
  for (xb_ctx *ctx = heap.contexts; ctx; ctx = ctx->gcNext)
    if (ctx != except) xb_pending_set(ctx, bits);
  xb_mutex_unlock(&heap.lock);
}

void xb_gc_add_root(xb_item *items, size_t count) {
  xb_gc_init();
  xb_root *r = xmalloc(sizeof *r);
  r->items = items;
  r->count = count;
  if (xb_storage_is_shared()) /* storage added once threads run: shared from the start, as the rest */
    for (size_t i = 0; i < count; ++i) xb_share(&items[i]);
  xb_mutex_lock(&heap.lock);
  r->next = heap.roots;
  heap.roots = r;
  xb_mutex_unlock(&heap.lock);
}

void xb_gc_roots_grow(xb_ctx *ctx) {
  /* Each root belongs to a C call in progress: the program's stack ends
   * the nesting long before the vector's size could overflow. */
  unsigned cap = ctx->rootCap ? ctx->rootCap * 2 : 64;
  xb_item **roots = cap > ctx->rootCap ? realloc(ctx->roots, (size_t)cap * sizeof *roots) : NULL;
  if (!roots) xb_gc_fatal("out of memory");
  ctx->roots = roots;
  ctx->rootCap = cap;
}

void xb_gc_root_push(xb_ctx *ctx, xb_item *it) { xb_root_push(ctx, it); }

void xb_gc_root_pop(xb_ctx *ctx, unsigned n) { xb_root_pop(ctx, n); }

/* A collection's state for an object (while the world is stopped), its sharing kept. */
static void set_state(xb_gc_hdr *h, uint8_t state) { h->state = (uint8_t)((h->state & XB_GC_SHARED) | state); }

/* The lines a block object covers get the collection's epoch, and become
 * old lines with `old`. */
static void mark_lines(const xb_gc_hdr *h, int old) {
  if (h->flags & XB_GC_HDR_LARGE) return;
  xb_hblock *b = block_of(h);
  if (b->cls) return; /* a size class's block keeps no line marks: its slots are swept */
  size_t last = line_of((const char *)h + object_total(h->size) - 1);
  for (size_t l = line_of(h); l <= last; ++l) {
    b->lineMark[l] = heap.epoch;
    if (old) b->lineOld[l] = 1;
  }
}

/* Marks an object and queues it for tracing. A full collection marks it
 * old; a young one marks it seen, and stops at old objects. */
static void push(xb_gc_hdr *h) {
  if (heap.full || !heap.aging) {
    if (h->state & XB_GC_MARKED) return;
    h->state |= XB_GC_MARKED; /* old now (an aged object is settled at the end) */
    mark_lines(h, 1);
  } else {
    if (h->state & (XB_GC_MARKED | XB_GC_SEEN)) return;
    h->state |= XB_GC_SEEN;
    mark_lines(h, 0);
    stack_push(&heap.survivors, &heap.survivorsLen, &heap.survivorsCap, h);
  }
  heap.markedBytes += h->size;
  ++heap.markedObjects;
  stack_push(&heap.stack, &heap.stackLen, &heap.stackCap, h);
}

/* Marks the object `h` refers to; true when it stays young after the young
 * collection in progress (reached now, and not aged before: it ages instead
 * of being promoted). */
static int mark_ref(xb_gc_hdr *h) {
  push(h);
  return !(h->state & (XB_GC_MARKED | XB_GC_AGED));
}

/* The object a word points into (its header, or anywhere in it, or just past it) in a block the collection in progress
 * empties, NULL for none: the chunks hold the blocks in use (neither fresh nor given back: their metadata is not read),
 * and the start bits where objects start (also dead ones,
 * which pinning keeps in place to no harm). Only there: a free block also keeps dead objects' start bits, and receives
 * the copies, over which a pin would write. */
static xb_gc_hdr *object_at(uintptr_t w) {
  if (!(w & (XB_GC_BLOCK_SIZE - 1))) --w; /* a block starts with its metadata: just past the last object before */
  const xb_hblock *b = block_of((const void *)w);
  const xb_chunk *c = chunk_of(w);
  if (!c || ((c->fresh | c->idle) & block_bit(c, b))) return NULL; /* not a block in use */
  size_t l = line_of((const void *)w);
  if (!b->defrag || l < XB_GC_FIRST_LINE) return NULL;
  if (b->cls) { /* a size class's block: the slot the word lies in when it holds an object, else the one just before */
    size_t size = heap.classSize[b->cls - 1], off = w - (uintptr_t)block_data((xb_hblock *)b), k = off / size;
    xb_gc_hdr *h = k < XB_GC_DATA_BYTES / size ? (xb_gc_hdr *)(block_data((xb_hblock *)b) + k * size) : NULL;
    if (h && h->kind) return h;
    if (off % size || !k) return NULL;
    h = (xb_gc_hdr *)(block_data((xb_hblock *)b) + (k - 1) * size); /* the word just past it, as for lines */
    return h->kind && w <= (uintptr_t)h + object_total(h->size) ? h : NULL;
  }
  /* The start at or before the word: in its line, then in the lines before, as far as an object reaches. */
  unsigned bit = (unsigned)((w & (XB_GC_LINE_SIZE - 1)) / XB_GC_ALIGN);
  uint32_t bits = b->starts[l] & ((2u << bit) - 1);
  for (size_t back = 0; !bits; bits = b->starts[l]) {
    if (l == XB_GC_FIRST_LINE || ++back > XB_GC_SMALL_MAX / XB_GC_LINE_SIZE) return NULL;
    --l;
  }
  unsigned k8 = 31u;
  while (!((bits >> k8) & 1u)) --k8;
  xb_gc_hdr *h = (xb_gc_hdr *)((char *)b + l * XB_GC_LINE_SIZE + k8 * XB_GC_ALIGN);
  return w <= (uintptr_t)h + object_total(h->size) ? h : NULL;
}

/* Pins the object a word points into that this collection could move, if any: it stays. */
static void pin(uintptr_t w) {
  xb_gc_hdr *h = object_at(w);
  if (!h || (h->flags & XB_GC_HDR_PINNED)) return;
  h->flags |= XB_GC_HDR_PINNED;
  stack_push(&heap.pinned, &heap.pinnedLen, &heap.pinnedCap, h);
}

static int range_order(const void *a, const void *b) {
  const char *x = ((const xb_range *)a)->start, *y = ((const xb_range *)b)->start;
  return x < y ? -1 : x > y;
}

/* The slots of the compiled code's frames on the threads' stacks, by address: the scans leave them out (they are
 * roots the collection updates). */
static void frame_ranges(void) {
  heap.framesLen = 0;
  for (xb_ctx *ctx = heap.contexts; ctx; ctx = ctx->gcNext)
    for (const xb_frame *f = ctx->frames; f; f = f->prev) {
      if (!f->nslots) continue;
      if (heap.framesLen == heap.framesCap) {
        size_t n = heap.framesCap ? heap.framesCap * 2 : 256;
        xb_range *list = realloc(heap.frames, n * sizeof *list);
        if (!list) out_of_memory();
        heap.frames = list;
        heap.framesCap = n;
      }
      heap.frames[heap.framesLen++] = (xb_range){(const char *)f->slots, (const char *)(f->slots + f->nslots)};
    }
  if (heap.framesLen) qsort(heap.frames, heap.framesLen, sizeof *heap.frames, range_order);
}

/* Pins what the words from `from` to `to` point to, the frames' slots left out. */
XB_SCANS_STACKS static void pin_range(const char *from, const char *to) {
  const char *p = (const char *)(((uintptr_t)from + sizeof(void *) - 1) & ~(uintptr_t)(sizeof(void *) - 1));
  size_t next = 0;
  while (p + sizeof(void *) <= to) {
    while (next < heap.framesLen && heap.frames[next].end <= p) ++next;
    if (next < heap.framesLen && heap.frames[next].start <= p) {
      p = heap.frames[next].end; /* slots are aligned items: the word after them is aligned too */
      continue;
    }
    uintptr_t w;
    memcpy(&w, p, sizeof w);
    pin(w);
    p += sizeof(void *);
  }
}

/* Pins what the threads' stacks point to: the collector's from here, every other one's from where it parked or
 * blocked, each with its registers saved on it above that point. */
static void pin_stacks(void *self) {
  frame_ranges();
  for (xb_tlh *t = heap.threads; t; t = t->next) {
    const char *from = t == self ? (const char *)&self : t->stackTop;
    if (!from) xb_gc_fatal("a thread runs during a collection");
    pin_range(from, t->stackBase);
  }
}

/* Where an object is now: its copy when this collection moved it. */
static xb_gc_hdr *forwarded(xb_gc_hdr *h) {
  return h->flags & XB_GC_HDR_MOVED ? link_of(h) : h;
}

/* A free slot of size class `c` for a copy: from the blocks its sparse blocks' objects move to (pick_defrag), NULL when
 * they have none left. */
static xb_gc_hdr *class_copy_slot(unsigned c) {
  for (size_t i = heap.classDestFrom[c]; i < heap.classDestTo[c]; ++i) {
    xb_hblock *b = heap.classDest[i];
    if (!b->freeSlot) continue;
    xb_gc_hdr *h = b->freeSlot;
    b->freeSlot = link_of(h);
    ++b->used;
    return h;
  }
  return NULL;
}

/* Moves an object of a block the collection empties, reached for the first time, into the copy space (free blocks),
 * its old place forwarding to the copy: the copy, or NULL when it stays (in another block, pinned, large, reached
 * already, too small to hold the forwarding address). An array whose items are its own gets them at the copy. */
static xb_gc_hdr *evacuate(xb_gc_hdr *h) {
  if (!heap.evacuating || (h->flags & (XB_GC_HDR_LARGE | XB_GC_HDR_PINNED)) ||
      (h->state & (XB_GC_MARKED | XB_GC_SEEN)) || h->size < XB_GC_LINK_AT + sizeof(xb_gc_hdr *) ||
      !block_of(h)->defrag)
    return NULL;
  size_t total = object_total(h->size);
  xb_gc_hdr *to;
  const xb_hblock *src = block_of(h);
  if (src->cls) { /* a slot of a fuller block of its class (pick_class_defrag made sure they have room) */
    if (!(to = class_copy_slot(src->cls - 1u))) return NULL;
    memcpy(to, h, total);
  } else {
    if (total > room(heap.copyCursor, heap.copyLimit)) {
      xb_hblock *b = block_new();
      if (!b) return NULL; /* no memory for the copy: the object stays */
      memset(b->starts, 0, sizeof b->starts);
      heap.copyCursor = block_data(b);
      heap.copyLimit = (char *)b + XB_GC_BLOCK_SIZE;
    }
    to = (xb_gc_hdr *)heap.copyCursor;
    heap.copyCursor += total;
    memcpy(to, h, total);
    set_start(to);
  }
  if (h->kind == XB_GC_ARRAY) {
    const xb_array *from = (const xb_array *)(h + 1);
    xb_array *a = (xb_array *)(to + 1);
    if (from->items == from->inline_items) a->items = a->inline_items;
  }
  h->flags |= XB_GC_HDR_MOVED;
  set_link(h, to);
  return to;
}

/* Marks the object a reference points to, moving it first when it can: where it is now. `young` gets whether it stays
 * young (see mark_ref). */
static xb_gc_hdr *reach(xb_gc_hdr *h, int *young) {
  h = forwarded(h);
  xb_gc_hdr *to = evacuate(h);
  if (to) h = to;
  *young |= mark_ref(h);
  return h;
}

/* A pointer to an object, set to where the object is now. */
#define REACH(ptr, young) ((ptr) = (void *)(reach(xb_gc_header(ptr), (young)) + 1))

/* Puts an old object in the remembered set (once), to be traced whole (XB_GC_REMEMBERED) or for its dirty cards
 * (XB_GC_CARDED): the write barrier's, and a collection's for what it keeps young. The thread that sets the object's
 * bit first (atomically) lists it, in a list of its own, without the lock; a collection merges the lists into the
 * remembered set (merge_remembered). */
static void record(xb_gc_hdr *h, uint8_t how) {
  uint8_t state = xb_atomic_load8(&h->state, XB_RELAXED);
  if (!(state & XB_GC_MARKED) || (state & how)) return;
  if (xb_atomic_or8(&h->state, how, XB_RELAXED) & (XB_GC_REMEMBERED | XB_GC_CARDED)) return; /* listed already */
  xb_tlh *t = tlh;
  if (t)
    stack_push(&t->remembered, &t->rememberedLen, &t->rememberedCap, h);
  else if (xb_atomic_load32(&heap.stopping, XB_RELAXED)) /* a collection by a thread not attached, under the lock */
    stack_push(&heap.remembered, &heap.rememberedLen, &heap.rememberedCap, h);
  else /* a store from a thread that never attached: the remembered set would be changed without the lock */
    xb_gc_fatal("a thread stores into the heap without xb_gc_thread_attach");
}

/* A thread's list of recorded objects joins the remembered set. Under the lock, the thread stopped or leaving. */
static void merge_remembered(xb_tlh *t) {
  for (size_t i = 0; i < t->rememberedLen; ++i)
    stack_push(&heap.remembered, &heap.rememberedLen, &heap.rememberedCap, t->remembered[i]);
  t->rememberedLen = 0;
}

void xb_gc_remember(void *obj) { record(xb_gc_header(obj), XB_GC_REMEMBERED); }

void xb_gc_write_item(xb_array *a, size_t index) {
  if (index < a->len && !xb_type_is_heap(a->items[index].type)) return; /* refers to no object */
  xb_gc_hdr *h = xb_gc_header(a);
  if ((xb_atomic_load8(&h->state, XB_RELAXED) & (XB_GC_MARKED | XB_GC_REMEMBERED)) != XB_GC_MARKED)
    return; /* young, or traced whole anyway */
  uint8_t *cards = xb_array_cards(a);
  if (!cards || index >= a->cap) {
    record(h, XB_GC_REMEMBERED);
    return;
  }
  cards[index >> XB_ARRAY_CARD_SHIFT] = 1;
  record(h, XB_GC_CARDED);
}

void xb_gc_write_items(xb_array *a, size_t from, size_t to) {
  xb_gc_hdr *h = xb_gc_header(a);
  if ((xb_atomic_load8(&h->state, XB_RELAXED) & (XB_GC_MARKED | XB_GC_REMEMBERED)) != XB_GC_MARKED)
    return; /* young, or traced whole anyway */
  for (size_t k = from; k < to; k = (k | (((size_t)1 << XB_ARRAY_CARD_SHIFT) - 1)) + 1) /* one per card */
    xb_gc_write_item(a, k);
}

/* Marks what an item refers to, the item set to where it is now; true when that stays young (see mark_ref). */
static int mark_item(xb_item *it) {
  int young = 0;
  switch ((xb_type)it->type) {
    case XB_T_STRING:
      if (!(it->v.s->flags & XB_STR_STATIC)) REACH(it->v.s, &young);
      return young;
    case XB_T_ARRAY:
    case XB_T_BLOCK:
    case XB_T_CELL:
    case XB_T_REF:
    case XB_T_OBJECT:
    case XB_T_HASH:
      REACH(it->v.p, &young);
      return young;
    default:
      return 0;
  }
}

/* The items under the dirty cards of an old array, eight cards per step. A
 * card whose items still refer to young objects stays dirty; returns whether
 * any did. */
static int trace_cards(xb_array *a) {
  uint8_t *cards = xb_array_cards(a);
  size_t n = xb_array_card_count(a->cap);
  int young = 0;
  for (size_t c = 0; c < n;) {
    uint64_t word = 0;
    if (c + sizeof word <= n) {
      memcpy(&word, cards + c, sizeof word);
      if (!word) {
        c += sizeof word;
        continue;
      }
    }
    if (cards[c]) {
      size_t from = c << XB_ARRAY_CARD_SHIFT, to = from + ((size_t)1 << XB_ARRAY_CARD_SHIFT);
      if (to > a->len) to = a->len;
      int keep = 0;
      for (size_t i = from; i < to; ++i) keep |= mark_item(&a->items[i]);
      cards[c] = (uint8_t)keep;
      young |= keep;
    }
    ++c;
  }
  return young;
}

/* Marks what an object refers to, its references set to where the objects are now; returns whether some of it
 * stays young. */
static int trace(xb_gc_hdr *h) {
  void *obj = h + 1;
  int young = 0;
  switch ((xb_gc_kind)h->kind) {
    case XB_GC_ARRAY: {
      xb_array *a = obj;
      if (a->items != a->inline_items) REACH(a->items, &young);
      for (size_t i = 0; i < a->len; ++i) young |= mark_item(&a->items[i]);
      uint8_t *cards = xb_array_cards(a); /* everything was scanned */
      if (cards) memset(cards, 0, xb_array_card_count(a->cap));
      break;
    }
    case XB_GC_BLOCK: {
      xb_block *b = obj;
      for (uint32_t i = 0; i < b->ncells; ++i)
        if (b->cells[i]) REACH(b->cells[i], &young); /* NULL until the creator fills it */
      break;
    }
    case XB_GC_CELL:
      young = mark_item(&((xb_cell *)obj)->value);
      break;
    case XB_GC_REF:
      young = mark_item(&((xb_ref *)obj)->coll);
      young |= mark_item(&((xb_ref *)obj)->value);
      young |= mark_item(&((xb_ref *)obj)->key);
      break;
    case XB_GC_HASH: {
      xb_hash *hs = obj;
      if (hs->entries) { /* the tables live in the entries' store: they move with it */
        char *before = (char *)hs->entries;
        REACH(hs->entries, &young);
        ptrdiff_t moved = (char *)hs->entries - before;
        if (moved) {
          hs->hashes = (uint32_t *)((char *)hs->hashes + moved);
          hs->index = (uint32_t *)((char *)hs->index + moved);
          if (hs->tree) hs->tree = (uint32_t *)((char *)hs->tree + moved);
        }
      }
      for (uint32_t i = 0; i < hs->used; ++i) {
        young |= mark_item(&hs->entries[i].key);
        young |= mark_item(&hs->entries[i].value);
      }
      break;
    }
    case XB_GC_OBJECT: {
      xb_object *o = obj;
      for (uint32_t i = 0; i < o->nvars; ++i) young |= mark_item(&o->vars[i]);
      break;
    }
    default:
      break;
  }
  return young;
}

/* An object that is old after this collection and still refers to young
 * objects: recorded again once the collection's state changes are done. */
static void retain(xb_gc_hdr *h, uint8_t how) {
  if (heap.retainedLen == heap.retainedCap) {
    size_t n = heap.retainedCap ? heap.retainedCap * 2 : 1024;
    xb_gc_hdr **r = realloc(heap.retained, n * sizeof *r);
    uint8_t *w = realloc(heap.retainedHow, n);
    if (!r || !w) out_of_memory();
    heap.retained = r;
    heap.retainedHow = w;
    heap.retainedCap = n;
  }
  heap.retained[heap.retainedLen] = h;
  heap.retainedHow[heap.retainedLen++] = how;
}

/* Traces a queued object; an aged one is promoted by this collection, so it is
 * retained if it still refers to young objects. */
static void trace_queued(xb_gc_hdr *h) {
  if (h->kind == XB_GC_ARRAY && ((xb_array *)(h + 1))->len > XB_GC_SLICE) { /* a slice at a time (drain) */
    xb_array *a = (xb_array *)(h + 1);
    int young = 0;
    if (a->items != a->inline_items) REACH(a->items, &young);
    if (heap.slicesLen == heap.slicesCap) {
      size_t n = heap.slicesCap ? heap.slicesCap * 2 : 64;
      xb_slice *list = realloc(heap.slices, n * sizeof *list);
      if (!list) out_of_memory();
      heap.slices = list;
      heap.slicesCap = n;
    }
    heap.slices[heap.slicesLen++] = (xb_slice){h, 0, young};
    return;
  }
  int young = trace(h);
  if (young && (h->state & XB_GC_AGED)) retain(h, XB_GC_REMEMBERED);
}

/* The next slice of the long array on top of the slices: its items marked; the array done with its last slice. */
static void trace_slice(void) {
  xb_slice *s = &heap.slices[heap.slicesLen - 1];
  xb_array *a = (xb_array *)(s->h + 1);
  size_t end = s->next + XB_GC_SLICE < a->len ? s->next + XB_GC_SLICE : a->len;
  for (size_t i = s->next; i < end; ++i) s->young |= mark_item(&a->items[i]);
  s->next = end;
  if (end < a->len) return;
  uint8_t *cards = xb_array_cards(a); /* everything was scanned */
  if (cards) memset(cards, 0, xb_array_card_count(a->cap));
  if (s->young && (s->h->state & XB_GC_AGED)) retain(s->h, XB_GC_REMEMBERED);
  --heap.slicesLen;
}

/* Marks everything queued: the objects first (depth first keeps the stack small), then the next slice of a long array.
 */
static void drain(void) {
  for (;;) {
    while (heap.stackLen) trace_queued(heap.stack[--heap.stackLen]);
    if (!heap.slicesLen) return;
    trace_slice();
  }
}

/* The capacity a collector's vector that grew past XB_GC_SHRINK_MIN entries for one collection and uses a quarter of it
 * at most now shrinks to (0: it stays): a peak (a long array's children, a burst of stores) does not keep its memory.
 */
static size_t shrunk(size_t len, size_t cap) {
  if (cap <= XB_GC_SHRINK_MIN || len > cap / 4) return 0;
  return len * 2 > XB_GC_SHRINK_MIN ? len * 2 : XB_GC_SHRINK_MIN;
}

#define SHRINK(vector, len, cap)                                      \
  do {                                                                \
    size_t n_ = shrunk((len), (cap));                                 \
    void *v_ = n_ ? realloc((vector), n_ * sizeof *(vector)) : NULL; \
    if (v_) {                                                         \
      (vector) = v_;                                                  \
      (cap) = n_;                                                     \
    }                                                                 \
  } while (0)

static void shrink_vectors(void) {
  SHRINK(heap.stack, heap.stackLen, heap.stackCap);
  SHRINK(heap.slices, heap.slicesLen, heap.slicesCap);
  SHRINK(heap.survivors, heap.survivorsLen, heap.survivorsCap);
  SHRINK(heap.aged, heap.agedLen, heap.agedCap);
  SHRINK(heap.remembered, heap.rememberedLen, heap.rememberedCap);
  SHRINK(heap.pinned, heap.pinnedLen, heap.pinnedCap);
  SHRINK(heap.frames, heap.framesLen, heap.framesCap);
  SHRINK(heap.defrag, heap.defragLen, heap.defragCap);
  SHRINK(heap.classDest, heap.classDestLen, heap.classDestCap);
  for (xb_tlh *t = heap.threads; t; t = t->next) SHRINK(t->remembered, t->rememberedLen, t->rememberedCap);
  size_t n = shrunk(heap.retainedLen, heap.retainedCap); /* two vectors in step: */
  uint8_t *how = n ? realloc(heap.retainedHow, n) : NULL;
  if (how) { /* the capacity is the smaller one's: the other may stay larger when it cannot shrink */
    heap.retainedHow = how;
    heap.retainedCap = n;
    xb_gc_hdr **r = realloc(heap.retained, n * sizeof *r);
    if (r) heap.retained = r;
  }
}

/* End of a young collection: objects that aged before and were not reached
 * die; reached ones age, or are promoted if they had aged (with aging off,
 * push already made every reached object old); then the old objects that
 * still refer to young ones are recorded again. Aging turns off when aged
 * objects mostly survived to promotion. */
static void settle_young(void) {
  size_t died = 0;
  for (size_t i = 0; i < heap.agedLen; ++i) {
    xb_gc_hdr *h = heap.aged[i];
    if (h->state & (XB_GC_SEEN | XB_GC_MARKED)) continue;
    h->state &= (uint8_t)~XB_GC_AGED;
    died += h->size;
  }
  if (!heap.aging) { /* reached aged objects were made old by push */
    for (size_t i = 0; i < heap.agedLen; ++i)
      if (heap.aged[i]->state & XB_GC_MARKED) set_state(heap.aged[i], XB_GC_MARKED);
    heap.agedLen = 0;
    heap.oldBytes += heap.markedBytes;
    return;
  }
  heap.agedLen = 0;
  size_t promoted = 0;
  for (size_t i = 0; i < heap.survivorsLen; ++i) {
    xb_gc_hdr *h = heap.survivors[i];
    if (h->state & XB_GC_AGED) {
      set_state(h, XB_GC_MARKED);
      mark_lines(h, 1);
      promoted += h->size;
    } else {
      set_state(h, XB_GC_AGED);
      stack_push(&heap.aged, &heap.agedLen, &heap.agedCap, h);
    }
  }
  heap.survivorsLen = 0;
  heap.oldBytes += promoted;
  if (promoted > died * XB_GC_AGING_WASTE) heap.aging = 0;
  for (size_t i = 0; i < heap.retainedLen; ++i) record(heap.retained[i], heap.retainedHow[i]);
  heap.retainedLen = 0;
}

static void mark_root(xb_item *it, void *data) {
  XB_UNUSED(data);
  mark_item(it);
}

static void mark_env(xb_env *env) {
  mark_item(&env->error_block);
  mark_item(&env->language);
  xb_areas_mark(env, mark_item);
  xb_memvar_bindings_each(env, mark_root, NULL);
  for (uint32_t i = 0; i < env->privateLen; ++i) mark_item(&env->privates[i].hidden);
}

static void mark_roots(void) {
  for (xb_ctx *ctx = heap.contexts; ctx; ctx = ctx->gcNext) {
    for (xb_frame *f = ctx->frames; f; f = f->prev)
      for (uint32_t i = 0; i < f->nslots; ++i) mark_item(&f->slots[i]);
    for (unsigned i = 0; i < ctx->rootLen; ++i) mark_item(ctx->roots[i]);
    mark_item(&ctx->break_value);
    for (xb_env *env = ctx->env; env; env = env->outer) mark_env(env);
    for (uint32_t i = 0; i < 2 * XB_MACRO_CACHE; ++i) mark_item(&ctx->macro_cache[i]);
  }
  for (const xb_root *r = heap.roots; r; r = r->next)
    for (size_t i = 0; i < r->count; ++i) mark_item(&r->items[i]);
  xb_memvar_publics_each(mark_root, NULL);
  xb_env_parked_each(mark_env, heap.full);
  xb_sched_mark(mark_item);
  for (size_t i = heap.finalizedHead; i < heap.finalizedLen; ++i) mark_item(&heap.finalized[i]);
}

/* An unreachable finalizable object moves to the queue of finalized ones, which keeps it, and what it refers to, alive
 * (marked now, a root later) until xb_gc_next_finalized hands it out. */
static void queue_finalized(xb_gc_hdr *h) {
  if (heap.finalizedLen == heap.finalizedCap) {
    if (heap.finalizedHead) { /* the handed-out entries make room first */
      memmove(heap.finalized, heap.finalized + heap.finalizedHead,
              (heap.finalizedLen - heap.finalizedHead) * sizeof *heap.finalized);
      heap.finalizedLen -= heap.finalizedHead;
      heap.finalizedHead = 0;
    }
    if (heap.finalizedLen == heap.finalizedCap) {
      size_t n = heap.finalizedCap ? heap.finalizedCap * 2 : 64;
      xb_item *q = realloc(heap.finalized, n * sizeof *q);
      if (!q) out_of_memory();
      heap.finalized = q;
      heap.finalizedCap = n;
    }
  }
  xb_item *it = &heap.finalized[heap.finalizedLen++];
  xb_set_object(it, (xb_object *)(h + 1));
  mark_item(it);
}

/* After marking: the finalizable objects it did not reach are finalized (queue_finalized). The young ones are looked at
 * by every collection, those made old move to the old list, which only a full collection looks at (an old object is
 * marked: a young collection never finds it unreachable), so a young collection costs what the young ones are. */
static void find_finalized(int full) {
  size_t kept = 0;
  for (size_t i = 0; i < heap.finalizableLen; ++i) {
    xb_gc_hdr *h = forwarded(heap.finalizable[i]);
    if (!(h->state & (XB_GC_SEEN | XB_GC_MARKED))) {
      queue_finalized(h);
    } else if (h->state & XB_GC_MARKED) {
      stack_push(&heap.finalizableOld, &heap.finalizableOldLen, &heap.finalizableOldCap, h);
      ++heap.finalizablePromoted;
    } else {
      heap.finalizable[kept++] = h;
    }
  }
  heap.finalizableLen = kept;
  if (!full) return;
  kept = 0;
  for (size_t i = 0; i < heap.finalizableOldLen; ++i) {
    xb_gc_hdr *h = forwarded(heap.finalizableOld[i]);
    if (h->state & (XB_GC_SEEN | XB_GC_MARKED)) heap.finalizableOld[kept++] = h;
    else queue_finalized(h);
  }
  heap.finalizableOldLen = kept;
  heap.finalizablePromoted = 0;
}

/* Where a traced object is now, or NULL when this collection found it unreachable (a weak reference's target). */
static void *live_object(void *obj) {
  xb_gc_hdr *h = forwarded((xb_gc_hdr *)obj - 1);
  return h->state & (XB_GC_SEEN | XB_GC_MARKED) ? h + 1 : NULL;
}

/* Clears the marks of every object and every line, before a full collection.
 * Block objects are found through the start bits (also dead ones whose lines
 * were not reused: clearing them changes nothing); a free block holds none
 * alive and no old line, and is skipped. */
static void unmark_all(void) {
  for (size_t k = 0; k < heap.blockCount; ++k) {
    xb_hblock *b = heap.blocks[k];
    if (!b->cls && b->freeLines == XB_GC_DATA_LINES) continue; /* a free block: no line old, no object alive */
    if (b->cls) {
      size_t size = heap.classSize[b->cls - 1];
      for (char *p = block_data(b); p + size <= (char *)b + XB_GC_BLOCK_SIZE; p += size) set_state((xb_gc_hdr *)p, 0);
      continue;
    }
    memset(b->lineOld, 0, sizeof b->lineOld); /* the marks of lines go stale with the next epoch */
    for (size_t l = XB_GC_FIRST_LINE; l < XB_GC_LINES; ++l)
      for (uint32_t bits = b->starts[l]; bits; bits &= bits - 1) {
        unsigned k8 = 0;
        while (!((bits >> k8) & 1u)) ++k8;
        set_state((xb_gc_hdr *)((char *)b + l * XB_GC_LINE_SIZE + k8 * XB_GC_ALIGN), 0);
      }
  }
  for (xb_large *l = heap.largeYoung; l; l = l->next) set_state(&l->hdr, 0);
  for (xb_large *l = heap.largeOld; l; l = l->next) set_state(&l->hdr, 0);
  heap.rememberedLen = 0;
  heap.agedLen = 0;
}

/* A size class's block after a collection (all of them after a full one, else those holding young objects): its dead
 * objects' slots free, and the block in its class's list while some are; with none taken, it becomes a free block of
 * lines again (0 is returned: it is swept as such). */
static int sweep_class(xb_hblock *b, int full) {
  unsigned c = b->cls - 1u;
  size_t size = heap.classSize[c];
  char *data = block_data(b);
  size_t slots = XB_GC_DATA_BYTES / size;
  if (full || b->young) {
    b->freeSlot = NULL;
    b->used = 0;
    b->young = 0;
    for (size_t i = slots; i-- > 0;) {
      xb_gc_hdr *h = (xb_gc_hdr *)(data + i * size);
      if (h->kind && (h->state & (XB_GC_MARKED | XB_GC_AGED))) { /* live: old, or aged young */
        ++b->used;
        b->young |= (uint8_t)((h->state & XB_GC_AGED) != 0);
        continue;
      }
      h->kind = 0;
      set_link(h, b->freeSlot);
      b->freeSlot = h;
    }
  }
  if (!b->used) { /* free: lines again, none marked */
    b->cls = 0;
    memset(b->lineMark, 0, sizeof b->lineMark);
    memset(b->lineOld, 0, sizeof b->lineOld);
    memset(b->starts, 0, sizeof b->starts);
    b->freeLines = (uint32_t)XB_GC_DATA_LINES;
    b->young = 0;
    return 0;
  }
  if (b->used < slots) {
    b->next = heap.classBlocks[c];
    heap.classBlocks[c] = b;
  }
  return 1;
}

/* The large objects after a collection (the old ones too after a full one): the dead ones' memory back, the old ones
 * in the old list, the aged ones in the young list still. */
static void sweep_large(int full) {
  xb_large *young = NULL, *old = full ? NULL : heap.largeOld;
  for (int list = 0; list < 1 + full; ++list) {
    xb_large *l = list ? heap.largeOld : heap.largeYoung;
    while (l) {
      xb_large *next = l->next;
      if (l->hdr.state & XB_GC_MARKED) {
        l->next = old;
        old = l;
      } else if (l->hdr.state & XB_GC_AGED) {
        l->next = young;
        young = l;
      } else {
        large_free(l);
      }
      l = next;
    }
  }
  heap.largeYoung = young;
  heap.largeOld = old;
}

/* After a collection: the free lines of the blocks that may have changed (all
 * of them after a full collection; else those that received objects or hold
 * aged ones), and the lists of free and recyclable blocks. A block keeps being
 * swept while aged objects (reached now, young still) may die there. */
static void sweep_blocks(int full) {
  heap.freeBlocks = NULL;
  heap.recyclable = NULL;
  heap.candidateLines = 0;
  memset(heap.classBlocks, 0, sizeof heap.classBlocks);
  const uint32_t lines = (uint32_t)XB_GC_DATA_LINES;
  for (size_t k = heap.blockCount; k-- > 0;) {
    xb_hblock *b = heap.blocks[k];
    if (b->cls && sweep_class(b, full)) continue; /* a size class's block with live objects */
    if (full || b->young) {
      uint32_t freeLines = 0;
      uint8_t aged = 0;
      for (size_t l = XB_GC_FIRST_LINE; l < XB_GC_LINES; ++l) {
        freeLines += (uint32_t)line_free(b, l);
        aged |= (uint8_t)(!b->lineOld[l] && b->lineMark[l] == heap.epoch);
      }
      b->freeLines = freeLines;
      b->young = aged;
    }
    if (defrag_candidate(b)) heap.candidateLines += b->freeLines;
    if (b->freeLines == lines) {
      b->next = heap.freeBlocks;
      heap.freeBlocks = b;
    } else if (b->freeLines) {
      b->next = heap.recyclable;
      heap.recyclable = b;
    }
  }
}

static uint32_t bits_in(uint32_t x) {
  uint32_t n = 0;
  for (; x; x &= x - 1) ++n;
  return n;
}

/* Whether block `b` (in use before) was just given back: its chunk says so (its metadata is not read). */
static int given_back(const xb_hblock *b) {
  const xb_chunk *c = chunk_of((uintptr_t)b);
  return (c->idle & block_bit(c, b)) != 0;
}

/* After a collection's sweep: the free blocks beyond those the next allocations need (the young budget, an eighth of
 * the blocks in use, at least XB_GC_RETAIN_MIN, half again before any goes, so that a heap near the line does not give
 * blocks back and take them again at every collection) go back to the system (Memory given back above): the highest
 * chunks' first, each whole page of the system whose blocks are all free or spare; then the chunks left with no block
 * in use are unmapped. Under the lock, every other thread stopped: no thread has a block of its own now. */
static void give_back(void) {
  size_t nfree = 0;
  for (const xb_hblock *b = heap.freeBlocks; b; b = b->next) ++nfree;
  size_t retain = heap.youngBudget / XB_GC_BLOCK_SIZE + (heap.blockCount - nfree) / 8;
  if (retain < XB_GC_RETAIN_MIN) retain = XB_GC_RETAIN_MIN;
  size_t page = xb_platform_page_size();
  if (nfree <= retain + retain / 2 || page > XB_GC_CHUNK_BYTES) return;
  uint32_t per = page > XB_GC_BLOCK_SIZE ? (uint32_t)(page / XB_GC_BLOCK_SIZE) : 1u; /* blocks of a page */
  uint32_t *freeMask = calloc(2 * heap.chunkCount, sizeof *freeMask), *chosen = freeMask + heap.chunkCount;
  if (!freeMask) return; /* nothing given back this time */
  for (const xb_hblock *b = heap.freeBlocks; b; b = b->next) {
    const xb_chunk *c = chunk_of((uintptr_t)b);
    freeMask[c - heap.chunks] |= block_bit(c, b);
  }
  /* The pages chosen first (the chunks' masks say so), the lists of blocks made without them while every block is
   * still readable, the pages given back last: no block's metadata is read once its page is gone. */
  size_t want = nfree - retain, given = 0;
  for (size_t k = heap.chunkCount; k-- > 0 && given < want;) {
    xb_chunk *c = &heap.chunks[k];
    for (uint32_t g = 0; g < XB_GC_CHUNK_BLOCKS && given < want; g += per) {
      uint32_t group = (per >= 32 ? UINT32_MAX : (1u << per) - 1) << g;
      uint32_t freed = freeMask[k] & group;
      if (!freed || ((freeMask[k] | c->fresh | c->idle) & group) != group) continue;
      chosen[k] |= group;
      heap.freshBlocks -= bits_in(c->fresh & group);
      c->idle |= group; /* fresh ones too: their pages go back with the others' */
      c->fresh &= ~group;
      given += bits_in(freed);
      if (k < heap.chunkHint) heap.chunkHint = k;
    }
  }
  if (given) {
    heap.spare += given;
    size_t kept = 0;
    for (size_t k = 0; k < heap.blockCount; ++k)
      if (!given_back(heap.blocks[k])) heap.blocks[kept++] = heap.blocks[k];
    heap.blockCount = kept;
    xb_hblock **tail = &heap.freeBlocks;
    for (xb_hblock *b = heap.freeBlocks; b; b = b->next)
      if (!given_back(b)) {
        *tail = b;
        tail = &b->next;
      }
    *tail = NULL;
    size_t chunks = 0;
    for (size_t k = 0; k < heap.chunkCount; ++k) {
      xb_chunk *c = &heap.chunks[k];
      if ((c->fresh | c->idle) == XB_GC_CHUNK_ALL) { /* none in use: unmapped whole */
        xb_platform_pages_unmap(c->start, XB_GC_CHUNK_BYTES);
        heap.spare -= XB_GC_CHUNK_BLOCKS;
        heap.freshBlocks -= bits_in(c->fresh);
        continue;
      }
      for (uint32_t g = 0; g < XB_GC_CHUNK_BLOCKS; g += per)
        if (chosen[k] & ((per >= 32 ? UINT32_MAX : (1u << per) - 1) << g))
          xb_platform_pages_decommit(c->start + (size_t)g * XB_GC_BLOCK_SIZE, (size_t)per * XB_GC_BLOCK_SIZE);
      heap.chunks[chunks++] = *c;
    }
    heap.chunkCount = chunks;
    heap.chunkHint = 0;
  }
  free(freeMask);
}

/* The next collection's epoch, for the line marks; when it wraps, every mark
 * is cleared first (a stale one would read as current). */
static void next_epoch(void) {
  if (++heap.epoch == 0) {
    for (size_t k = 0; k < heap.blockCount; ++k) memset(heap.blocks[k]->lineMark, 0, sizeof heap.blocks[k]->lineMark);
    heap.epoch = 1;
  }
}

static int fewer_live(const void *a, const void *b) {
  uint32_t x = (*(xb_hblock *const *)a)->freeLines, y = (*(xb_hblock *const *)b)->freeLines;
  return x > y ? -1 : x < y; /* more free lines first */
}

static int more_used(const void *a, const void *b) {
  const xb_hblock *x = *(xb_hblock *const *)a, *y = *(xb_hblock *const *)b;
  if (x->cls != y->cls) return x->cls < y->cls ? -1 : 1;
  return x->used > y->used ? -1 : x->used < y->used; /* by class, the fullest first */
}

/* The size classes' blocks the full collection in progress empties: for each class, its blocks by how full they are,
 * the sparsest ones (at most half their slots taken) whose objects all fit the free slots of the fuller ones, which
 * become the class's copy blocks (class_copy_slot): no new memory, and the emptied blocks are free after the sweep, so
 * a class whose objects died but one in many gives its memory back. The blocks are those swept last (in use). The
 * number of blocks emptied: none below XB_GC_CLASS_DEFRAG_MIN, which would not pay for the move. */
static size_t pick_class_defrag(void) {
  heap.classDestLen = 0;
  for (size_t i = 0; i < heap.blockCount; ++i) {
    xb_hblock *b = heap.blocks[i];
    if (!b->cls) continue;
    if (heap.classDestLen == heap.classDestCap) {
      size_t n = heap.classDestCap ? heap.classDestCap * 2 : 256;
      xb_hblock **list = realloc(heap.classDest, n * sizeof *list);
      if (!list) out_of_memory();
      heap.classDest = list;
      heap.classDestCap = n;
    }
    heap.classDest[heap.classDestLen++] = b;
  }
  if (heap.classDestLen) qsort(heap.classDest, heap.classDestLen, sizeof *heap.classDest, more_used);
  size_t emptied = 0, i = 0;
  for (unsigned c = 0; c < heap.classes; ++c) {
    heap.classDestFrom[c] = i;
    size_t end = i;
    while (end < heap.classDestLen && heap.classDest[end]->cls == c + 1u) ++end;
    size_t slots = XB_GC_DATA_BYTES / heap.classSize[c], live = 0, room = 0, j = end;
    for (size_t k = i; k < end; ++k) room += slots - heap.classDest[k]->used;
    while (j > i + 1) { /* the next sparsest joins the emptied ones while the fuller ones can take them all */
      const xb_hblock *b = heap.classDest[j - 1];
      if (b->used * 2 > slots || live + b->used > room - (slots - b->used)) break;
      live += b->used;
      room -= slots - b->used;
      --j;
    }
    for (size_t k = j; k < end; ++k) heap.classDest[k]->defrag = 1;
    emptied += end - j;
    heap.classDestTo[c] = j; /* the copy blocks: the fuller ones, before the emptied ones */
    i = end;
  }
  if (emptied < XB_GC_CLASS_DEFRAG_MIN) { /* too few to pay for the move's pinning scan */
    for (size_t k = 0; k < heap.classDestLen; ++k) heap.classDest[k]->defrag = 0;
    memset(heap.classDestTo, 0, sizeof heap.classDestTo);
    return 0;
  }
  return emptied;
}

/* The blocks the full collection in progress empties: the emptiest of those partly used (at most half their lines
 * live), as long as the free blocks and a reserve of new ones (a sixteenth of the heap, which the emptied blocks give
 * back) can take what lives there. True when there are some. */
static int pick_defrag(void) {
  const uint32_t lines = (uint32_t)XB_GC_DATA_LINES;
  size_t space = heap.blockCount / XB_GC_DEFRAG_RESERVE * lines;
  for (const xb_hblock *b = heap.freeBlocks; b; b = b->next) space += lines;
  heap.defragLen = 0;
  for (size_t i = 0; i < heap.blockCount; ++i) { /* the lists lose blocks allocation found no hole in: all of them */
    xb_hblock *b = heap.blocks[i];
    if (!defrag_candidate(b)) continue;
    if (heap.defragLen == heap.defragCap) {
      size_t n = heap.defragCap ? heap.defragCap * 2 : 256;
      xb_hblock **list = realloc(heap.defrag, n * sizeof *list);
      if (!list) out_of_memory();
      heap.defrag = list;
      heap.defragCap = n;
    }
    heap.defrag[heap.defragLen++] = b;
  }
  if (heap.defragLen) qsort(heap.defrag, heap.defragLen, sizeof *heap.defrag, fewer_live);
  size_t k = 0;
  for (; k < heap.defragLen && lines - heap.defrag[k]->freeLines <= space; ++k) {
    space -= lines - heap.defrag[k]->freeLines;
    heap.defrag[k]->defrag = 1;
  }
  heap.defragLen = k;
  return pick_class_defrag() + k > 0;
}

/* A collection, full or young, with every other thread stopped. `due`: one an allocation asks for, which a
 * collection another thread made meanwhile may have made needless. */
static void collect(int full, int due) {
  xb_tlh *self = tlh;
  /* Another thread's collection, waited for, may have made this one needless. */
  if (lock_between(1) && due && !full && heap.allocated < heap.youngBudget) {
    xb_mutex_unlock(&heap.lock);
    return;
  }
  stop_world(self);
  for (xb_tlh *t = heap.threads; t; t = t->next) { /* the threads' holes end: blocks are swept anew */
    report_allocation(t);
    merge_remembered(t);
    t->cursor = t->limit = t->overCursor = t->overLimit = NULL;
    t->block = t->overBlock = NULL;
    memset(t->cls, 0, sizeof t->cls);
  }
  heap.full = full;
  if (full) unmark_all();
  next_epoch();
  heap.markedBytes = 0;
  heap.markedObjects = 0;
  heap.evacuating = full && !heap.holds && tlh && stacks_known() && pick_defrag();
  if (heap.evacuating) { /* pins first: what the stacks point to, and the objects C roots sit in */
    xb_platform_with_registers(pin_stacks, tlh);
    for (xb_ctx *ctx = heap.contexts; ctx; ctx = ctx->gcNext)
      for (unsigned i = 0; i < ctx->rootLen; ++i) pin((uintptr_t)ctx->roots[i]);
  }
  mark_roots();
  for (size_t i = 0; i < heap.rememberedLen; ++i) {
    xb_gc_hdr *h = heap.remembered[i];
    uint8_t how = h->state & (XB_GC_REMEMBERED | XB_GC_CARDED);
    set_state(h, XB_GC_MARKED);
    int young = (how & XB_GC_REMEMBERED) ? trace(h) : trace_cards((xb_array *)(h + 1));
    if (young) retain(h, (how & XB_GC_REMEMBERED) ? XB_GC_REMEMBERED : XB_GC_CARDED);
  }
  heap.rememberedLen = 0;
  drain();
  if (heap.finalizableLen || (full && heap.finalizableOldLen)) {
    find_finalized(full);
    drain();
  }
  if (xb_env_parked_settle(live_object, full)) heap.reap = 1; /* closed soon: their tables, their locks */
  if (!full) settle_young();
  for (size_t i = 0; i < heap.pinnedLen; ++i) heap.pinned[i]->flags &= (uint16_t)~XB_GC_HDR_PINNED;
  heap.pinnedLen = 0;
  for (size_t i = 0; i < heap.defragLen; ++i) heap.defrag[i]->defrag = 0;
  heap.defragLen = 0;
  for (size_t i = 0; i < heap.classDestLen; ++i) heap.classDest[i]->defrag = 0;
  heap.classDestLen = 0;
  memset(heap.classDestTo, 0, sizeof heap.classDestTo);
  heap.evacuating = 0;
  heap.copyCursor = heap.copyLimit = NULL; /* the rest of the copy space's block is free lines */

  sweep_large(full);
  sweep_blocks(full);
  int64_t now = xb_platform_monotonic_ns();
  if (full) {
    if (heap.markedBytes < heap.oldBytes / 2) heap.aging = 1; /* most promoted bytes had died */
    heap.oldBytes = heap.liveBytes = heap.markedBytes;
    heap.liveObjects = heap.markedObjects;
    size_t growth = heap.liveBytes / XB_GC_FULL_GROWTH;
    heap.fullThreshold = heap.liveBytes + (growth > XB_GC_MIN_FULL ? growth : XB_GC_MIN_FULL);
    heap.lastFullNs = now;
  } else {
    if (heap.markedBytes > heap.allocated / XB_GC_SURVIVAL_HIGH) heap.youngBudget *= 2;
    else if (heap.markedBytes < heap.allocated / XB_GC_SURVIVAL_LOW) heap.youngBudget /= 2;
  }
  if (heap.youngBudget > heap.liveBytes / XB_GC_YOUNG_SHARE) heap.youngBudget = heap.liveBytes / XB_GC_YOUNG_SHARE;
  if (heap.youngBudget < XB_GC_YOUNG_MIN) heap.youngBudget = XB_GC_YOUNG_MIN;
  heap.allocatedSinceFull = full ? 0 : heap.allocatedSinceFull + heap.allocated;
  heap.allocated = 0;
  give_back();
  give_back_large();
  shrink_vectors();
  size_t allocLimit = heap.liveBytes > XB_GC_MIN_FULL ? heap.liveBytes : XB_GC_MIN_FULL;
  int fullNext = heap.oldBytes >= heap.fullThreshold || heap.allocatedSinceFull / XB_GC_FULL_ALLOC >= allocLimit ||
                 heap.finalizablePromoted >= (heap.finalizableOldLen / 2 > XB_GC_FULL_FINALIZABLE
                                                  ? heap.finalizableOldLen / 2
                                                  : XB_GC_FULL_FINALIZABLE) ||
                 (now - heap.lastFullNs >= XB_GC_FULL_EVERY_NS && heap.allocatedSinceFull >= heap.youngBudget);
  request_finalize(self);
  start_world();
  xb_mutex_unlock(&heap.lock);
  if (!full && fullNext) collect(1, 0);
}

void xb_gc_hold(void) {
  xb_mutex_lock(&heap.lock);
  ++heap.holds;
  xb_mutex_unlock(&heap.lock);
}

void xb_gc_release(void) {
  xb_mutex_lock(&heap.lock);
  if (!heap.holds) xb_gc_fatal("the collector released more often than held");
  --heap.holds;
  xb_mutex_unlock(&heap.lock);
}

void xb_gc_collect(void) { collect(1, 0); }
void xb_gc_collect_young(void) { collect(0, 0); }

void xb_gc_finalizable(void *obj) {
  xb_mutex_lock(&heap.lock);
  stack_push(&heap.finalizable, &heap.finalizableLen, &heap.finalizableCap, xb_gc_header(obj));
  xb_mutex_unlock(&heap.lock);
}

int xb_gc_next_finalized(xb_item *out) {
  xb_mutex_lock(&heap.lock);
  int found = heap.finalizedHead < heap.finalizedLen;
  if (found) *out = heap.finalized[heap.finalizedHead++];
  if (heap.finalizedHead == heap.finalizedLen) heap.finalizedHead = heap.finalizedLen = 0;
  xb_mutex_unlock(&heap.lock);
  return found;
}

size_t xb_gc_finalizable_count(void) {
  xb_mutex_lock(&heap.lock);
  size_t n = heap.finalizableLen + heap.finalizableOldLen + (heap.finalizedLen - heap.finalizedHead);
  xb_mutex_unlock(&heap.lock);
  return n;
}

int xb_gc_consistent(void) {
  xb_mutex_lock(&heap.lock);
  int ok = 1;
  size_t listed = 0, freeInUse = 0;
  for (const xb_hblock *b = heap.freeBlocks; b && ok; b = b->next) {
    const xb_chunk *c = chunk_of((uintptr_t)b);
    ok = c && !((c->fresh | c->idle) & block_bit(c, b)) && !b->cls && b->freeLines == XB_GC_DATA_LINES;
    ++listed;
  }
  for (size_t k = 0; k < heap.blockCount && ok; ++k) {
    const xb_hblock *b = heap.blocks[k];
    const xb_chunk *c = chunk_of((uintptr_t)b);
    ok = c && !((c->fresh | c->idle) & block_bit(c, b));
    freeInUse += !b->cls && b->freeLines == XB_GC_DATA_LINES;
  }
  size_t spare = 0, fresh = 0;
  for (size_t k = 0; k < heap.chunkCount; ++k) {
    spare += bits_in(heap.chunks[k].fresh | heap.chunks[k].idle);
    fresh += bits_in(heap.chunks[k].fresh);
  }
  ok = ok && listed == freeInUse && spare == heap.spare && fresh == heap.freshBlocks &&
       heap.blockCount + spare == heap.chunkCount * XB_GC_CHUNK_BLOCKS;
  xb_mutex_unlock(&heap.lock);
  return ok;
}

size_t xb_gc_heap_bytes(void) {
  xb_mutex_lock(&heap.lock);
  size_t bytes = heap.blockCount * XB_GC_BLOCK_SIZE + heap.largeMapped;
  for (size_t k = 0; k < heap.lchunkCount; ++k)
    for (uint32_t u = 0; u < XB_GC_LARGE_UNITS; ++u) bytes += !unit_used(heap.lchunks[k].idle, u) * XB_GC_LARGE_UNIT;
  xb_mutex_unlock(&heap.lock);
  return bytes;
}

size_t xb_gc_live_objects(void) {
  xb_mutex_lock(&heap.lock);
  size_t n = heap.liveObjects;
  xb_mutex_unlock(&heap.lock);
  return n;
}
