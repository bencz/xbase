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
 * larger ones are allocated individually and kept in a list. Blocks come from
 * chunks of XB_GC_CHUNK_BLOCKS blocks, aligned by hand.
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
 * the old generation has doubled since the previous full one.
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
 * Objects in size classes' blocks, large ones and those too small for the
 * copy's address never move. It runs on a thread whose stack start is known,
 * and not while C code holds the collector (xb_gc_hold): it first scans that
 * stack (registers saved on it) for words pointing into the objects it may
 * move and pins those, the compiled code's frames aside (precise roots), and
 * pins the objects the C roots' addresses lie in.
 *
 * Threads. Global structures are protected by one lock; the allocation fast
 * path takes none. A collection must run with every mutator thread stopped at
 * a safepoint (the XB_PENDING_SAFEPOINT handshake); until tasks run on several
 * threads, the allocating thread is the only mutator. */
#include "xbrt_int.h"

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
/* Bytes allocated between young collections: they adapt to the survival
 * rate: they double while most young objects survive (collecting them early
 * only promotes them) and halve while few do. They never exceed the size of
 * the old generation, so memory stays proportional to the live data. */
#define XB_GC_YOUNG_MIN ((size_t)4 << 20)
#define XB_GC_SURVIVAL_HIGH 2u                    /* survivors above 1/2 of the allocation */
#define XB_GC_SURVIVAL_LOW 8u                     /* survivors below 1/8 */
/* Aging turns off when the bytes promoted at a young collection exceed this
 * many times the aged bytes that died. */
#define XB_GC_AGING_WASTE 4u
#define XB_GC_MIN_FULL ((size_t)8 << 20)         /* old bytes that trigger the first full collection */
#define XB_GC_ALIGN 8u
#define XB_GC_DEFRAG_RESERVE 16u                 /* new blocks a defragmentation may take: the heap's blocks / this */

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

typedef struct xb_large {
  union {                   /* 8 bytes on every target, so the object stays 8-aligned */
    struct xb_large *next;
    uint64_t align;
  } link;
  xb_gc_hdr hdr;            /* immediately followed by the object */
} xb_large;

typedef struct xb_range { /* a run of words a scan for pinned objects leaves out */
  const char *start, *end;
} xb_range;

typedef struct xb_root {    /* items outside frames */
  struct xb_root *next;
  xb_item *items;
  size_t count;
} xb_root;

typedef struct xb_tlh {     /* thread-local heap */
  struct xb_tlh *next;
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
  int ready;
  xb_hblock **blocks;               /* every block */
  size_t blockCount, blockCap;
  xb_hblock *freeBlocks;            /* no live object */
  xb_hblock *recyclable;            /* free lines among live ones */
  char *chunkNext, *chunkEnd;      /* blocks of the last chunk not used yet */
  char **chunks;                   /* the start of each chunk, in address order */
  uint32_t classSize[XB_GC_CLASSES_MAX]; /* each size class's slot, header included */
  unsigned classes;
  uint8_t classOf[XB_GC_CLASSES_MAX * 64]; /* by (bytes - 1) / XB_GC_ALIGN above XB_GC_SMALL_MAX: the class */
  size_t classMax;                 /* the largest object in a size class */
  xb_hblock *classBlocks[XB_GC_CLASSES_MAX]; /* blocks of each class with free slots */
  size_t chunkCount, chunkCap;
  uint8_t epoch;                   /* of the last collection, from 1 (lines start at 0, never current) */
  xb_large *large;
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
  xb_gc_hdr **finalizable;         /* objects to finalize when a collection finds them unreachable */
  size_t finalizableLen, finalizableCap;
  xb_item *finalized;              /* found unreachable, kept alive (roots) until they are finalized, in order */
  size_t finalizedHead, finalizedLen, finalizedCap;
  int evacuating;                  /* the collection in progress moves objects out of the blocks marked defrag */
  xb_hblock **defrag;              /* those blocks */
  size_t defragLen, defragCap;
  size_t allocatedSinceFull;       /* bytes allocated since the last full collection */
  unsigned holds;                  /* xb_gc_hold: none moves */
  char *copyCursor, *copyLimit;    /* where the collection copies objects: free blocks */
  xb_gc_hdr **pinned;              /* pinned by the collection in progress */
  size_t pinnedLen, pinnedCap;
  xb_range *frames;                /* the frames' slots on the stack being scanned, by address */
  size_t framesLen, framesCap;
} heap;

static XB_THREAD_LOCAL xb_tlh *tlh;
static XB_THREAD_LOCAL const char *stackBase; /* xb_gc_thread_stack */

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

void xb_gc_init(void) {
  if (heap.ready) return;
  if (!xb_mutex_init(&heap.lock)) xb_gc_fatal("cannot create the heap lock");
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
  heap.ready = 1;
}

static xb_tlh *thread_heap(void) {
  if (!tlh) {
    tlh = xmalloc(sizeof *tlh);
    memset(tlh, 0, sizeof *tlh);
    xb_mutex_lock(&heap.lock);
    tlh->next = heap.threads;
    heap.threads = tlh;
    xb_mutex_unlock(&heap.lock);
  }
  return tlh;
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

/* Keeps a chunk's start among the others, in address order. Under the lock. */
static void add_chunk(char *start) {
  if (heap.chunkCount == heap.chunkCap) {
    size_t n = heap.chunkCap ? heap.chunkCap * 2 : 64;
    char **list = realloc(heap.chunks, n * sizeof *list);
    if (!list) out_of_memory();
    heap.chunks = list;
    heap.chunkCap = n;
  }
  size_t k = heap.chunkCount++;
  for (; k > 0 && heap.chunks[k - 1] > start; --k) heap.chunks[k] = heap.chunks[k - 1];
  heap.chunks[k] = start;
}

/* Whether a full collection may empty block `b` (Evacuation above): a block of lines, partly used, at most half of
 * its lines live (as its last sweep found them). */
static int defrag_candidate(const xb_hblock *b) {
  return !b->cls && b->freeLines * 2 >= XB_GC_DATA_LINES && b->freeLines != XB_GC_DATA_LINES;
}

/* Whether the heap, which is to grow, is fragmented: a quarter of it free lines of blocks a full collection may empty,
 * which objects of several lines may not fit, a young budget allocated since the last full collection (not to collect
 * over and over), and this thread able to move objects. A full collection that defragments comes first then. Under
 * the lock. */
static int fragmented(void) {
  if (heap.chunkNext != heap.chunkEnd || heap.allocatedSinceFull + heap.allocated < heap.youngBudget || heap.holds ||
      !stackBase)
    return 0;
  size_t freeLines = 0;
  for (size_t k = 0; k < heap.blockCount; ++k)
    if (defrag_candidate(heap.blocks[k])) freeLines += heap.blocks[k]->freeLines;
  return freeLines * XB_GC_LINE_SIZE * 4 > heap.blockCount * XB_GC_BLOCK_SIZE;
}

/* A block with every line free, from the free blocks or the current chunk (a
 * new chunk when it is used up). Under the lock. */
static xb_hblock *block_new(void) {
  xb_hblock *b = heap.freeBlocks;
  if (b) {
    heap.freeBlocks = b->next;
  } else {
    if (heap.chunkNext == heap.chunkEnd) { /* never returned to the system */
      char *raw = xmalloc(XB_GC_CHUNK_BLOCKS * XB_GC_BLOCK_SIZE + XB_GC_BLOCK_SIZE);
      heap.chunkNext = (char *)(((uintptr_t)raw + XB_GC_BLOCK_SIZE - 1) & ~(uintptr_t)(XB_GC_BLOCK_SIZE - 1));
      heap.chunkEnd = heap.chunkNext + XB_GC_CHUNK_BLOCKS * XB_GC_BLOCK_SIZE;
      add_chunk(heap.chunkNext);
    }
    b = (xb_hblock *)heap.chunkNext;
    heap.chunkNext += XB_GC_BLOCK_SIZE;
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
  int medium = total > XB_GC_LINE_SIZE;
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
    int collect = heap.allocated >= heap.youngBudget;
    xb_mutex_unlock(&heap.lock);
    if (collect) {
      xb_gc_collect_young(); /* retires every thread's holes */
      continue;
    }
    xb_mutex_lock(&heap.lock);
    int defrag = !heap.recyclable && !heap.freeBlocks && fragmented();
    xb_mutex_unlock(&heap.lock);
    if (defrag) { /* rather than growing the heap */
      xb_gc_collect();
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
    } while (!take_hole(b, XB_GC_FIRST_LINE, total, cursor, limit));
    *block = b;
    xb_mutex_unlock(&heap.lock);
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
  for (;;) {
    xb_hblock *b = t->cls[c];
    if (b && b->freeSlot) {
      xb_gc_hdr *h = b->freeSlot;
      b->freeSlot = link_of(h);
      ++b->used;
      return h;
    }
    xb_mutex_lock(&heap.lock);
    report_allocation(t);
    int collect = heap.allocated >= heap.youngBudget;
    if (!collect) {
      b = heap.classBlocks[c];
      if (b) {
        heap.classBlocks[c] = b->next;
      } else {
        b = block_new();
        class_block_init(b, c);
      }
      b->young = 1;
      t->cls[c] = b;
    }
    xb_mutex_unlock(&heap.lock);
    if (collect) xb_gc_collect_young(); /* retires every thread's blocks */
  }
}

static void *alloc_large(size_t size) {
  xb_mutex_lock(&heap.lock);
  heap.allocated += size;
  int collect = heap.allocated >= heap.youngBudget;
  xb_mutex_unlock(&heap.lock);
  if (collect) xb_gc_collect_young();
  xb_large *l = xmalloc(sizeof(xb_large) + size);
  xb_mutex_lock(&heap.lock);
  l->link.next = heap.large;
  heap.large = l;
  xb_mutex_unlock(&heap.lock);
  l->hdr.flags = XB_GC_HDR_LARGE;
  return &l->hdr;
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

void xb_gc_register(xb_ctx *ctx) {
  xb_mutex_lock(&heap.lock);
  ctx->gcPrev = NULL;
  ctx->gcNext = heap.contexts;
  if (heap.contexts) heap.contexts->gcPrev = ctx;
  heap.contexts = ctx;
  xb_mutex_unlock(&heap.lock);
}

void xb_gc_unregister(xb_ctx *ctx) {
  xb_mutex_lock(&heap.lock);
  if (ctx->gcPrev) ctx->gcPrev->gcNext = ctx->gcNext;
  else heap.contexts = ctx->gcNext;
  if (ctx->gcNext) ctx->gcNext->gcPrev = ctx->gcPrev;
  xb_mutex_unlock(&heap.lock);
}

void xb_gc_add_root(xb_item *items, size_t count) {
  xb_gc_init();
  xb_root *r = xmalloc(sizeof *r);
  r->items = items;
  r->count = count;
  xb_mutex_lock(&heap.lock);
  r->next = heap.roots;
  heap.roots = r;
  xb_mutex_unlock(&heap.lock);
}

void xb_gc_root_push(xb_ctx *ctx, xb_item *it) {
  if (ctx->rootLen == ctx->rootCap) {
    /* Each root belongs to a C call in progress: the program's stack ends
     * the nesting long before the vector's size could overflow. */
    unsigned cap = ctx->rootCap ? ctx->rootCap * 2 : 64;
    xb_item **roots = cap > ctx->rootCap ? realloc(ctx->roots, (size_t)cap * sizeof *roots) : NULL;
    if (!roots) xb_gc_fatal("out of memory");
    ctx->roots = roots;
    ctx->rootCap = cap;
  }
  ctx->roots[ctx->rootLen++] = it;
}

void xb_gc_root_pop(xb_ctx *ctx, unsigned n) { ctx->rootLen -= n; }

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
 * empties, NULL for none: the chunks hold the blocks in use, and the start bits where objects start (also dead ones,
 * which pinning keeps in place to no harm). Only there: a free block also keeps dead objects' start bits, and receives
 * the copies, over which a pin would write. */
static xb_gc_hdr *object_at(uintptr_t w) {
  if (!(w & (XB_GC_BLOCK_SIZE - 1))) --w; /* a block starts with its metadata: just past the last object before */
  size_t lo = 0, hi = heap.chunkCount;
  while (lo < hi) { /* the last chunk starting at or before the word */
    size_t mid = (lo + hi) / 2;
    if ((uintptr_t)heap.chunks[mid] <= w) lo = mid + 1;
    else hi = mid;
  }
  if (!lo) return NULL;
  uintptr_t chunk = (uintptr_t)heap.chunks[lo - 1];
  if (w >= chunk + XB_GC_CHUNK_BLOCKS * XB_GC_BLOCK_SIZE) return NULL;
  if (chunk == (uintptr_t)heap.chunkEnd - XB_GC_CHUNK_BLOCKS * XB_GC_BLOCK_SIZE && w >= (uintptr_t)heap.chunkNext)
    return NULL; /* blocks of the last chunk not used yet */
  const xb_hblock *b = block_of((const void *)w);
  size_t l = line_of((const void *)w);
  if (!b->defrag || l < XB_GC_FIRST_LINE) return NULL;
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

/* Pins what the words of this thread's stack point to, from here to its start, the slots of the compiled code's
 * frames on it left out (they are roots the collection updates); called with the registers saved on the stack. */
static void pin_stack(void *unused) {
  XB_UNUSED(unused);
  const char *here = (const char *)&unused;
  heap.framesLen = 0;
  for (xb_ctx *ctx = heap.contexts; ctx; ctx = ctx->gcNext)
    for (const xb_frame *f = ctx->frames; f; f = f->prev) {
      const char *start = (const char *)f->slots, *end = (const char *)(f->slots + f->nslots);
      if (start >= stackBase || end <= here) continue; /* not on this stack */
      if (heap.framesLen == heap.framesCap) {
        size_t n = heap.framesCap ? heap.framesCap * 2 : 256;
        xb_range *list = realloc(heap.frames, n * sizeof *list);
        if (!list) out_of_memory();
        heap.frames = list;
        heap.framesCap = n;
      }
      heap.frames[heap.framesLen++] = (xb_range){start, end};
    }
  if (heap.framesLen) qsort(heap.frames, heap.framesLen, sizeof *heap.frames, range_order);
  const char *p = (const char *)(((uintptr_t)here + sizeof(void *) - 1) & ~(uintptr_t)(sizeof(void *) - 1));
  size_t next = 0;
  while (p + sizeof(void *) <= stackBase) {
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

/* Where an object is now: its copy when this collection moved it. */
static xb_gc_hdr *forwarded(xb_gc_hdr *h) {
  return h->flags & XB_GC_HDR_MOVED ? link_of(h) : h;
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
  if (total > room(heap.copyCursor, heap.copyLimit)) {
    xb_hblock *b = block_new();
    memset(b->starts, 0, sizeof b->starts);
    heap.copyCursor = block_data(b);
    heap.copyLimit = (char *)b + XB_GC_BLOCK_SIZE;
  }
  xb_gc_hdr *to = (xb_gc_hdr *)heap.copyCursor;
  heap.copyCursor += total;
  memcpy(to, h, total);
  set_start(to);
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

/* Puts an old object in the remembered set (once), to be traced whole
 * (XB_GC_REMEMBERED) or for its dirty cards (XB_GC_CARDED). Under the lock. */
static void record_locked(xb_gc_hdr *h, uint8_t how) {
  if ((h->state & XB_GC_MARKED) && !(h->state & how)) {
    if (!(h->state & (XB_GC_REMEMBERED | XB_GC_CARDED)))
      stack_push(&heap.remembered, &heap.rememberedLen, &heap.rememberedCap, h);
    h->state = (uint8_t)(h->state | how);
  }
}

static void record(xb_gc_hdr *h, uint8_t how) {
  xb_mutex_lock(&heap.lock);
  record_locked(h, how);
  xb_mutex_unlock(&heap.lock);
}

void xb_gc_remember(void *obj) { record(xb_gc_header(obj), XB_GC_REMEMBERED); }

void xb_gc_write_item(xb_array *a, size_t index) {
  if (index < a->len && !xb_type_is_heap(a->items[index].type)) return; /* refers to no object */
  xb_gc_hdr *h = xb_gc_header(a);
  if ((h->state & (XB_GC_MARKED | XB_GC_REMEMBERED)) != XB_GC_MARKED) return; /* young, or traced whole anyway */
  uint8_t *cards = xb_array_cards(a);
  if (!cards || index >= a->cap) {
    record(h, XB_GC_REMEMBERED);
    return;
  }
  cards[index >> XB_ARRAY_CARD_SHIFT] = 1;
  if (!(h->state & XB_GC_CARDED)) record(h, XB_GC_CARDED);
}

void xb_gc_write_items(xb_array *a, size_t from, size_t to) {
  xb_gc_hdr *h = xb_gc_header(a);
  if ((h->state & (XB_GC_MARKED | XB_GC_REMEMBERED)) != XB_GC_MARKED) return; /* young, or traced whole anyway */
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
  int young = trace(h);
  if (young && (h->state & XB_GC_AGED)) retain(h, XB_GC_REMEMBERED);
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
      if (heap.aged[i]->state & XB_GC_MARKED) heap.aged[i]->state = XB_GC_MARKED;
    heap.agedLen = 0;
    heap.oldBytes += heap.markedBytes;
    return;
  }
  heap.agedLen = 0;
  size_t promoted = 0;
  for (size_t i = 0; i < heap.survivorsLen; ++i) {
    xb_gc_hdr *h = heap.survivors[i];
    if (h->state & XB_GC_AGED) {
      h->state = XB_GC_MARKED;
      mark_lines(h, 1);
      promoted += h->size;
    } else {
      h->state = XB_GC_AGED;
      stack_push(&heap.aged, &heap.agedLen, &heap.agedCap, h);
    }
  }
  heap.survivorsLen = 0;
  heap.oldBytes += promoted;
  if (promoted > died * XB_GC_AGING_WASTE) heap.aging = 0;
  for (size_t i = 0; i < heap.retainedLen; ++i) record_locked(heap.retained[i], heap.retainedHow[i]);
  heap.retainedLen = 0;
}

static void mark_roots(void) {
  for (xb_ctx *ctx = heap.contexts; ctx; ctx = ctx->gcNext) {
    for (xb_frame *f = ctx->frames; f; f = f->prev)
      for (uint32_t i = 0; i < f->nslots; ++i) mark_item(&f->slots[i]);
    for (unsigned i = 0; i < ctx->rootLen; ++i) mark_item(ctx->roots[i]);
    mark_item(&ctx->error_block);
    mark_item(&ctx->break_value);
    mark_item(&ctx->language);
    xb_areas_mark(ctx, mark_item);
    for (uint32_t i = 0; i < ctx->memvarCap; ++i) mark_item(&ctx->memvars[i]);
    for (uint32_t i = 0; i < ctx->privateLen; ++i) mark_item(&ctx->privates[i].hidden);
    for (uint32_t i = 0; i < 2 * XB_MACRO_CACHE; ++i) mark_item(&ctx->macro_cache[i]);
  }
  for (const xb_root *r = heap.roots; r; r = r->next)
    for (size_t i = 0; i < r->count; ++i) mark_item(&r->items[i]);
  for (size_t i = heap.finalizedHead; i < heap.finalizedLen; ++i) mark_item(&heap.finalized[i]);
}

/* After marking: the finalizable objects it did not reach move to the queue
 * of finalized ones, which keeps them, and what they refer to, alive (marked
 * now, roots later) until xb_gc_next_finalized hands them out. */
static void find_finalized(void) {
  size_t kept = 0;
  for (size_t i = 0; i < heap.finalizableLen; ++i) {
    xb_gc_hdr *h = forwarded(heap.finalizable[i]);
    if (h->state & (XB_GC_SEEN | XB_GC_MARKED)) {
      heap.finalizable[kept++] = h;
      continue;
    }
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
  heap.finalizableLen = kept;
}

/* Clears the marks of every object and every line, before a full collection.
 * Block objects are found through the start bits (also dead ones whose lines
 * were not reused: clearing them changes nothing). */
static void unmark_all(void) {
  for (size_t k = 0; k < heap.blockCount; ++k) {
    xb_hblock *b = heap.blocks[k];
    if (b->cls) {
      size_t size = heap.classSize[b->cls - 1];
      for (char *p = block_data(b); p + size <= (char *)b + XB_GC_BLOCK_SIZE; p += size) ((xb_gc_hdr *)p)->state = 0;
      continue;
    }
    memset(b->lineOld, 0, sizeof b->lineOld); /* the marks of lines go stale with the next epoch */
    for (size_t l = XB_GC_FIRST_LINE; l < XB_GC_LINES; ++l)
      for (uint32_t bits = b->starts[l]; bits; bits &= bits - 1) {
        unsigned k8 = 0;
        while (!((bits >> k8) & 1u)) ++k8;
        ((xb_gc_hdr *)((char *)b + l * XB_GC_LINE_SIZE + k8 * XB_GC_ALIGN))->state = 0;
      }
  }
  for (xb_large *l = heap.large; l; l = l->link.next) l->hdr.state = 0;
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

/* After a collection: the free lines of the blocks that may have changed (all
 * of them after a full collection; else those that received objects or hold
 * aged ones), and the lists of free and recyclable blocks. A block keeps being
 * swept while aged objects (reached now, young still) may die there. */
static void sweep_blocks(int full) {
  heap.freeBlocks = NULL;
  heap.recyclable = NULL;
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
    if (b->freeLines == lines) {
      b->next = heap.freeBlocks;
      heap.freeBlocks = b;
    } else if (b->freeLines) {
      b->next = heap.recyclable;
      heap.recyclable = b;
    }
  }
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
  return k > 0;
}

static void collect(int full) {
  xb_mutex_lock(&heap.lock);
  for (xb_tlh *t = heap.threads; t; t = t->next) { /* the threads' holes end: blocks are swept anew */
    report_allocation(t);
    t->cursor = t->limit = t->overCursor = t->overLimit = NULL;
    t->block = t->overBlock = NULL;
    memset(t->cls, 0, sizeof t->cls);
  }
  heap.full = full;
  if (full) unmark_all();
  next_epoch();
  heap.markedBytes = 0;
  heap.markedObjects = 0;
  heap.evacuating = full && !heap.holds && stackBase && pick_defrag();
  if (heap.evacuating) { /* pins first: what the stack points to, and the objects C roots sit in */
    xb_platform_with_registers(pin_stack, NULL);
    for (xb_ctx *ctx = heap.contexts; ctx; ctx = ctx->gcNext)
      for (unsigned i = 0; i < ctx->rootLen; ++i) pin((uintptr_t)ctx->roots[i]);
  }
  mark_roots();
  for (size_t i = 0; i < heap.rememberedLen; ++i) {
    xb_gc_hdr *h = heap.remembered[i];
    uint8_t how = h->state & (XB_GC_REMEMBERED | XB_GC_CARDED);
    h->state = XB_GC_MARKED;
    int young = (how & XB_GC_REMEMBERED) ? trace(h) : trace_cards((xb_array *)(h + 1));
    if (young) retain(h, (how & XB_GC_REMEMBERED) ? XB_GC_REMEMBERED : XB_GC_CARDED);
  }
  heap.rememberedLen = 0;
  while (heap.stackLen) trace_queued(heap.stack[--heap.stackLen]);
  if (heap.finalizableLen) {
    find_finalized();
    while (heap.stackLen) trace_queued(heap.stack[--heap.stackLen]);
  }
  if (!full) settle_young();
  for (size_t i = 0; i < heap.pinnedLen; ++i) heap.pinned[i]->flags &= (uint16_t)~XB_GC_HDR_PINNED;
  heap.pinnedLen = 0;
  for (size_t i = 0; i < heap.defragLen; ++i) heap.defrag[i]->defrag = 0;
  heap.defragLen = 0;
  heap.evacuating = 0;
  heap.copyCursor = heap.copyLimit = NULL; /* the rest of the copy space's block is free lines */

  for (xb_large **pl = &heap.large; *pl;) {
    xb_large *l = *pl;
    if (l->hdr.state & (XB_GC_MARKED | XB_GC_AGED)) {
      pl = &l->link.next;
    } else {
      *pl = l->link.next;
      free(l);
    }
  }
  sweep_blocks(full);
  if (full) {
    if (heap.markedBytes < heap.oldBytes / 2) heap.aging = 1; /* most promoted bytes had died */
    heap.oldBytes = heap.markedBytes;
    heap.liveObjects = heap.markedObjects;
    heap.fullThreshold = heap.oldBytes * 2 > XB_GC_MIN_FULL ? heap.oldBytes * 2 : XB_GC_MIN_FULL;
  } else {
    if (heap.markedBytes > heap.allocated / XB_GC_SURVIVAL_HIGH) heap.youngBudget *= 2;
    else if (heap.markedBytes < heap.allocated / XB_GC_SURVIVAL_LOW) heap.youngBudget /= 2;
  }
  if (heap.youngBudget > heap.oldBytes) heap.youngBudget = heap.oldBytes;
  if (heap.youngBudget < XB_GC_YOUNG_MIN) heap.youngBudget = XB_GC_YOUNG_MIN;
  heap.allocatedSinceFull = full ? 0 : heap.allocatedSinceFull + heap.allocated;
  heap.allocated = 0;
  int fullNext = heap.oldBytes >= heap.fullThreshold;
  if (heap.finalizedLen > heap.finalizedHead)
    for (xb_ctx *ctx = heap.contexts; ctx; ctx = ctx->gcNext) ctx->pending |= XB_PENDING_FINALIZE;
  xb_mutex_unlock(&heap.lock);
  if (!full && fullNext) collect(1);
}

void xb_gc_thread_stack(const void *base) { stackBase = base; }

void xb_gc_hold(void) {
  xb_mutex_lock(&heap.lock);
  ++heap.holds;
  xb_mutex_unlock(&heap.lock);
}

void xb_gc_release(void) {
  xb_mutex_lock(&heap.lock);
  --heap.holds;
  xb_mutex_unlock(&heap.lock);
}

void xb_gc_collect(void) { collect(1); }
void xb_gc_collect_young(void) { collect(0); }

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
  size_t n = heap.finalizableLen + (heap.finalizedLen - heap.finalizedHead);
  xb_mutex_unlock(&heap.lock);
  return n;
}

size_t xb_gc_live_objects(void) {
  xb_mutex_lock(&heap.lock);
  size_t n = heap.liveObjects;
  xb_mutex_unlock(&heap.lock);
  return n;
}
