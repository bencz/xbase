/* Tracing garbage collector: precise, non-moving, generational, with an Immix
 * heap layout.
 *
 * Heap. Objects up to XB_GC_MEDIUM_MAX bytes (header included) live in blocks
 * of XB_GC_BLOCK_SIZE bytes, aligned to their size, divided into lines of
 * XB_GC_LINE_SIZE bytes; the block's first lines hold its metadata: a mark per
 * line (the epoch of the last collection that reached an object there), a flag
 * per line holding an old object, and a bit per 8 bytes where an object
 * starts. Every thread allocates by bumping a cursor through a hole, a run of
 * free lines, with no lock; an object above one line that does not fit the
 * rest of the hole goes to an overflow run of the thread instead (holes large
 * enough for it, in recyclable or free blocks), so no hole is given up for it.
 * Larger objects are allocated individually and kept in a list. Blocks come
 * from chunks of XB_GC_CHUNK_BLOCKS blocks, aligned by hand.
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
 * Threads. Global structures are protected by one lock; the allocation fast
 * path takes none. A collection must run with every mutator thread stopped at
 * a safepoint (the XB_PENDING_SAFEPOINT handshake); until tasks run on several
 * threads, the allocating thread is the only mutator. */
#include "xbrt_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XB_GC_BLOCK_SIZE ((size_t)32 << 10)
#define XB_GC_LINE_SIZE ((size_t)128)
#define XB_GC_LINES (XB_GC_BLOCK_SIZE / XB_GC_LINE_SIZE)
#define XB_GC_MEDIUM_MAX ((size_t)8 << 10)       /* largest object in blocks, header included */
#define XB_GC_CHUNK_BLOCKS 32u                    /* blocks obtained from the system at a time */
/* Bytes allocated between young collections: they adapt to the survival
 * rate: they double while most young objects survive (collecting them early
 * only promotes them) and halve while few do. They never exceed the size of
 * the old generation, so memory stays proportional to the live data. */
#define XB_GC_YOUNG_MIN ((size_t)8 << 20)
#define XB_GC_SURVIVAL_HIGH 2u                    /* survivors above 1/2 of the allocation */
#define XB_GC_SURVIVAL_LOW 8u                     /* survivors below 1/8 */
/* Aging turns off when the bytes promoted at a young collection exceed this
 * many times the aged bytes that died. */
#define XB_GC_AGING_WASTE 4u
#define XB_GC_MIN_FULL ((size_t)32 << 20)        /* old bytes that trigger the first full collection */
#define XB_GC_ALIGN 8u

/* The metadata at the start of a block (its first XB_GC_FIRST_LINE lines). */
typedef struct xb_hblock {
  struct xb_hblock *next;            /* in the list of free or of recyclable blocks */
  uint32_t freeLines;               /* after its last sweep */
  uint8_t young;                    /* received objects, or holds aged ones, since its last sweep */
  uint8_t lineMark[XB_GC_LINES];    /* the epoch of the last collection that reached an object in the line */
  uint8_t lineOld[XB_GC_LINES];     /* an old object covers the line */
  uint16_t starts[XB_GC_LINES];     /* bit k of a line: an object starts at its byte 8k */
} xb_hblock;
#define XB_GC_FIRST_LINE ((sizeof(xb_hblock) + XB_GC_LINE_SIZE - 1) / XB_GC_LINE_SIZE)
_Static_assert(XB_GC_LINE_SIZE / XB_GC_ALIGN == 16, "one start bit per 8 bytes of a line");
_Static_assert(XB_GC_FIRST_LINE * 4 < XB_GC_LINES, "the metadata leaves most of a block");

typedef struct xb_large {
  union {                   /* 8 bytes on every target, so the object stays 8-aligned */
    struct xb_large *next;
    uint64_t align;
  } link;
  xb_gc_hdr hdr;            /* immediately followed by the object */
} xb_large;

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

void xb_gc_init(void) {
  if (heap.ready) return;
  if (!xb_mutex_init(&heap.lock)) xb_gc_fatal("cannot create the heap lock");
  heap.epoch = 1; /* never 0, which a line that no collection marked has */
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

/* Header and object, 8-byte aligned: what a block object takes. */
static size_t object_total(size_t size) {
  return sizeof(xb_gc_hdr) + ((size + XB_GC_ALIGN - 1) & ~(size_t)(XB_GC_ALIGN - 1));
}

/* Whether nothing live covers line `l` of `b`: no old object, and no object
 * the last collection reached. */
static int line_free(const xb_hblock *b, size_t l) { return !b->lineOld[l] && b->lineMark[l] != heap.epoch; }

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
  if (total <= XB_GC_MEDIUM_MAX) {
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

/* Marks what an item refers to; true when that stays young (see mark_ref). */
static int mark_item(const xb_item *it) {
  switch ((xb_type)it->type) {
    case XB_T_STRING:
      return !(it->v.s->flags & XB_STR_STATIC) && mark_ref(xb_gc_header(it->v.s));
    case XB_T_ARRAY:
    case XB_T_BLOCK:
    case XB_T_CELL:
    case XB_T_REF:
    case XB_T_OBJECT:
    case XB_T_HASH:
      return mark_ref(xb_gc_header(it->v.p));
    default:
      return 0;
  }
}

/* The items under the dirty cards of an old array, eight cards per step. A
 * card whose items still refer to young objects stays dirty; returns whether
 * any did. */
static int trace_cards(const xb_array *a) {
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

/* Marks what an object refers to; returns whether some of it stays young. */
static int trace(const xb_gc_hdr *h) {
  const void *obj = h + 1;
  int young = 0;
  switch ((xb_gc_kind)h->kind) {
    case XB_GC_ARRAY: {
      const xb_array *a = obj;
      if (a->items != a->inline_items) young |= mark_ref(xb_gc_header(a->items));
      for (size_t i = 0; i < a->len; ++i) young |= mark_item(&a->items[i]);
      uint8_t *cards = xb_array_cards(a); /* everything was scanned */
      if (cards) memset(cards, 0, xb_array_card_count(a->cap));
      break;
    }
    case XB_GC_BLOCK: {
      const xb_block *b = obj;
      for (uint32_t i = 0; i < b->ncells; ++i)
        if (b->cells[i]) young |= mark_ref(xb_gc_header(b->cells[i])); /* NULL until the creator fills it */
      break;
    }
    case XB_GC_CELL:
      young = mark_item(&((const xb_cell *)obj)->value);
      break;
    case XB_GC_REF:
      young = mark_item(&((const xb_ref *)obj)->coll);
      young |= mark_item(&((const xb_ref *)obj)->value);
      young |= mark_item(&((const xb_ref *)obj)->key);
      break;
    case XB_GC_HASH: {
      const xb_hash *hs = obj;
      if (hs->entries) young |= mark_ref(xb_gc_header(hs->entries));
      for (uint32_t i = 0; i < hs->used; ++i) {
        young |= mark_item(&hs->entries[i].key);
        young |= mark_item(&hs->entries[i].value);
      }
      break;
    }
    case XB_GC_OBJECT: {
      const xb_object *o = obj;
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
    for (const xb_frame *f = ctx->frames; f; f = f->prev)
      for (uint32_t i = 0; i < f->nslots; ++i) mark_item(&f->slots[i]);
    for (unsigned i = 0; i < ctx->rootLen; ++i) mark_item(ctx->roots[i]);
    mark_item(&ctx->error_block);
    mark_item(&ctx->break_value);
    for (uint32_t i = 0; i < ctx->memvarCap; ++i) mark_item(&ctx->memvars[i]);
    for (uint32_t i = 0; i < ctx->privateLen; ++i) mark_item(&ctx->privates[i].hidden);
  }
  for (const xb_root *r = heap.roots; r; r = r->next)
    for (size_t i = 0; i < r->count; ++i) mark_item(&r->items[i]);
}

/* Clears the marks of every object and every line, before a full collection.
 * Block objects are found through the start bits (also dead ones whose lines
 * were not reused: clearing them changes nothing). */
static void unmark_all(void) {
  for (size_t k = 0; k < heap.blockCount; ++k) {
    xb_hblock *b = heap.blocks[k];
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

/* After a collection: the free lines of the blocks that may have changed (all
 * of them after a full collection; else those that received objects or hold
 * aged ones), and the lists of free and recyclable blocks. A block keeps being
 * swept while aged objects (reached now, young still) may die there. */
static void sweep_blocks(int full) {
  heap.freeBlocks = NULL;
  heap.recyclable = NULL;
  const uint32_t lines = (uint32_t)(XB_GC_LINES - XB_GC_FIRST_LINE);
  for (size_t k = heap.blockCount; k-- > 0;) {
    xb_hblock *b = heap.blocks[k];
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

static void collect(int full) {
  xb_mutex_lock(&heap.lock);
  for (xb_tlh *t = heap.threads; t; t = t->next) { /* the threads' holes end: blocks are swept anew */
    report_allocation(t);
    t->cursor = t->limit = t->overCursor = t->overLimit = NULL;
    t->block = t->overBlock = NULL;
  }
  heap.full = full;
  if (full) unmark_all();
  next_epoch();
  heap.markedBytes = 0;
  heap.markedObjects = 0;
  mark_roots();
  for (size_t i = 0; i < heap.rememberedLen; ++i) {
    xb_gc_hdr *h = heap.remembered[i];
    uint8_t how = h->state & (XB_GC_REMEMBERED | XB_GC_CARDED);
    h->state = XB_GC_MARKED;
    int young = (how & XB_GC_REMEMBERED) ? trace(h) : trace_cards((const xb_array *)(h + 1));
    if (young) retain(h, (how & XB_GC_REMEMBERED) ? XB_GC_REMEMBERED : XB_GC_CARDED);
  }
  heap.rememberedLen = 0;
  while (heap.stackLen) trace_queued(heap.stack[--heap.stackLen]);
  if (!full) settle_young();

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
  heap.allocated = 0;
  int fullNext = heap.oldBytes >= heap.fullThreshold;
  xb_mutex_unlock(&heap.lock);
  if (!full && fullNext) collect(1);
}

void xb_gc_collect(void) { collect(1); }
void xb_gc_collect_young(void) { collect(0); }

size_t xb_gc_live_objects(void) {
  xb_mutex_lock(&heap.lock);
  size_t n = heap.liveObjects;
  xb_mutex_unlock(&heap.lock);
  return n;
}
