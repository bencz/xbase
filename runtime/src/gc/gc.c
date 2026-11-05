/* Tracing garbage collector: precise, non-moving mark and lazy sweep.
 *
 * Allocation. Objects up to XB_GC_SMALL_MAX bytes (header included) come from
 * size-class pages of XB_GC_PAGE_SIZE bytes. Every thread has a local heap
 * holding one page per class; allocating pops that page's free list with no
 * lock. Larger objects are allocated individually and kept in a list.
 *
 * Objects. An 8-byte xb_gc_hdr precedes every object, so object pointers are
 * what the rest of the runtime and compiled code use (an xb_str* is still the
 * string, a cell's value is at offset 0).
 *
 * Generations. Marks are sticky: an object marked by a collection stays
 * marked, and a marked object is old. A young collection marks from the roots
 * and from the remembered set, never traverses old objects, and frees only
 * young objects it did not reach. While aging is on, a young object it
 * reaches ages and one that had already aged is promoted, so objects that die
 * soon after a young collection (a table whose entries are replaced) die
 * young; an object that is old after a young collection and still points to
 * a young one stays in the remembered set (an array keeps the card dirty)
 * until that one is promoted or dies. Aging adapts: it turns off when most
 * aged objects survive to promotion (it only doubles their tracing) and back
 * on when a full collection finds that most promoted bytes had died. The write barrier
 * (xb_gc_write, xbrt.h) puts an old object that receives a store into the
 * remembered set, so the young objects it points to are found. A full
 * collection clears every mark first and traces the whole heap; it runs when
 * the old generation has doubled since the previous full one.
 *
 * Collection. Marking starts from the item frames and C roots of every
 * registered context (every task, running or suspended) and from the global
 * roots, and uses an explicit stack. Large objects are swept right away;
 * pages are swept lazily, when an allocation needs a page with free slots,
 * so the cost is spread over the following allocations. A young collection
 * only needs to sweep the pages that received objects since their last sweep:
 * old objects do not die in it. Collections are triggered by the bytes
 * allocated since the last one.
 *
 * Threads. Global structures are protected by one lock; the allocation fast
 * path takes none. A collection must run with every mutator thread stopped at
 * a safepoint (the XB_PENDING_SAFEPOINT handshake); until tasks run on
 * several threads, the allocating thread is the only mutator. */
#include "xbrt_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XB_GC_PAGE_SIZE ((size_t)64 << 10)
#define XB_GC_SMALL_MAX 2048u                    /* largest slot, header included */
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

/* Slot sizes (header included): fine steps for small objects, ~25% steps above. */
static const uint32_t class_sizes[] = {
    16,  24,  32,  40,  48,  56,  64,  80,  96,   112,  128,  160,  192,  224,
    256, 320, 384, 448, 512, 640, 768, 896, 1024, 1280, 1536, 1792, 2048,
};
#define XB_GC_CLASSES (sizeof class_sizes / sizeof class_sizes[0])

typedef struct xb_page {
  struct xb_page *next;     /* in its class list */
  void *freeList;           /* free slots (link stored in the payload) */
  uint32_t cls;
  uint32_t slotSize;
  uint32_t nslots;
  uint32_t live;            /* slots in use after the last sweep */
  uint8_t needsSweep;       /* a collection ran since its last sweep */
  uint8_t cached;           /* some thread heap allocates from it */
  uint8_t young;            /* may hold young objects: received objects, or kept aged ones, since its last sweep */
  char *data;               /* first slot, XB_GC_ALIGN aligned */
} xb_page;

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
  xb_page *current[XB_GC_CLASSES];
  size_t allocated;         /* bytes since it last reported to the global count */
} xb_tlh;

static struct {
  xb_mutex lock;
  int ready;
  uint8_t classOf[XB_GC_SMALL_MAX / XB_GC_ALIGN + 1]; /* (size + 7) / 8 -> class */
  xb_page *pages[XB_GC_CLASSES];   /* every page, by class */
  xb_page *cursor[XB_GC_CLASSES];  /* where the next search for a free slot starts */
  xb_page *emptyPages;             /* pages with no live object, ready for reuse */
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
  xb_mutex_init(&heap.lock);
  size_t c = 0;
  for (size_t units = 0; units <= XB_GC_SMALL_MAX / XB_GC_ALIGN; ++units) {
    while (class_sizes[c] < units * XB_GC_ALIGN) ++c;
    heap.classOf[units] = (uint8_t)c;
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

static void page_format(xb_page *p, uint32_t cls) {
  p->cls = cls;
  p->slotSize = class_sizes[cls];
  p->nslots = (uint32_t)((XB_GC_PAGE_SIZE - (size_t)(p->data - (char *)p)) / p->slotSize);
  p->live = 0;
  p->needsSweep = 0;
  p->young = 0;
  p->freeList = NULL;
  for (uint32_t i = p->nslots; i-- > 0;) {
    xb_gc_hdr *h = (xb_gc_hdr *)(p->data + (size_t)i * p->slotSize);
    h->kind = XB_GC_FREE;
    h->state = 0;
    *(void **)(h + 1) = p->freeList;
    p->freeList = h;
  }
}

static xb_page *page_new(uint32_t cls) {
  xb_page *p = heap.emptyPages;
  if (p) {
    heap.emptyPages = p->next;
  } else {
    p = xmalloc(XB_GC_PAGE_SIZE);
    size_t off = (sizeof(xb_page) + XB_GC_ALIGN - 1) & ~(size_t)(XB_GC_ALIGN - 1);
    p->data = (char *)p + off;
  }
  page_format(p, cls);
  p->cached = 0;
  p->next = heap.pages[cls];
  heap.pages[cls] = p;
  return p;
}

/* Rebuilds the free list from the marks of the last collection: old and aged
 * objects stay. */
static void page_sweep(xb_page *p) {
  void *freeList = NULL;
  uint32_t live = 0;
  uint8_t young = 0;
  for (uint32_t i = p->nslots; i-- > 0;) {
    xb_gc_hdr *h = (xb_gc_hdr *)(p->data + (size_t)i * p->slotSize);
    if (h->kind != XB_GC_FREE) {
      if (h->state & (XB_GC_MARKED | XB_GC_AGED)) {
        young |= (h->state & XB_GC_AGED) != 0;
        ++live;
        continue;
      }
      h->kind = XB_GC_FREE;
    }
    *(void **)(h + 1) = freeList;
    freeList = h;
  }
  p->freeList = freeList;
  p->live = live;
  p->needsSweep = 0;
  p->young = young; /* aged objects may die at the next young collection */
}

/* A page of class `cls` with a free slot, for the thread heap. Under the lock.
 * The search resumes where the previous one stopped (pages before it were
 * found full since the last collection), so refills stay cheap. */
static xb_page *page_for(uint32_t cls) {
  for (xb_page *p = heap.cursor[cls]; p; p = p->next) {
    if (p->cached) continue;
    if (p->needsSweep) page_sweep(p);
    if (p->freeList) {
      heap.cursor[cls] = p;
      return p;
    }
  }
  xb_page *p = page_new(cls);
  heap.cursor[cls] = NULL; /* new pages go to the front: the list behind them is full */
  return p;
}

static void *alloc_small(uint32_t cls) {
  xb_tlh *t = thread_heap();
  xb_page *p = t->current[cls];
  if (!p || !p->freeList) {
    xb_mutex_lock(&heap.lock);
    report_allocation(t);
    int collect = heap.allocated >= heap.youngBudget;
    xb_mutex_unlock(&heap.lock);
    if (collect) xb_gc_collect_young();
    xb_mutex_lock(&heap.lock);
    if (t->current[cls]) t->current[cls]->cached = 0;
    p = page_for(cls);
    p->cached = 1;
    p->young = 1;
    t->current[cls] = p;
    xb_mutex_unlock(&heap.lock);
  }
  xb_gc_hdr *h = p->freeList;
  p->freeList = *(void **)(h + 1);
  ++p->live;
  t->allocated += p->slotSize;
  return h;
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
  return &l->hdr;
}

void *xb_gc_alloc(xb_gc_kind kind, size_t size) {
  if (size > SIZE_MAX / 2) out_of_memory();
  size_t total = sizeof(xb_gc_hdr) + ((size + XB_GC_ALIGN - 1) & ~(size_t)(XB_GC_ALIGN - 1));
  xb_gc_hdr *h = total <= XB_GC_SMALL_MAX ? alloc_small(heap.classOf[total / XB_GC_ALIGN]) : alloc_large(size);
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
  if (ctx->rootLen == XB_GC_MAX_ROOTS) {
    fputs("xbrt: too many temporary GC roots\n", stderr);
    abort();
  }
  ctx->roots[ctx->rootLen++] = it;
}

void xb_gc_root_pop(xb_ctx *ctx, unsigned n) { ctx->rootLen -= n; }


/* Marks an object and queues it for tracing. A full collection marks it
 * old; a young one marks it seen, and stops at old objects. */
static void push(xb_gc_hdr *h) {
  if (heap.full || !heap.aging) {
    if (h->state & XB_GC_MARKED) return;
    h->state |= XB_GC_MARKED; /* old now (an aged object is settled at the end) */
  } else {
    if (h->state & (XB_GC_MARKED | XB_GC_SEEN)) return;
    h->state |= XB_GC_SEEN;
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
      break;
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
    for (const xb_seg *s = ctx->seg; s; s = s->prev)
      for (size_t i = 0; i < s->used; ++i) mark_item(&s->items[i]);
    for (unsigned i = 0; i < ctx->rootLen; ++i) mark_item(ctx->roots[i]);
  }
  for (const xb_root *r = heap.roots; r; r = r->next)
    for (size_t i = 0; i < r->count; ++i) mark_item(&r->items[i]);
}

/* Clears the marks of every object, before a full collection. */
static void unmark_all(void) {
  for (size_t c = 0; c < XB_GC_CLASSES; ++c)
    for (xb_page *p = heap.pages[c]; p; p = p->next)
      for (uint32_t k = 0; k < p->nslots; ++k) ((xb_gc_hdr *)(p->data + (size_t)k * p->slotSize))->state = 0;
  for (xb_large *l = heap.large; l; l = l->link.next) l->hdr.state = 0;
  heap.rememberedLen = 0;
  heap.agedLen = 0;
}

static void collect(int full) {
  xb_mutex_lock(&heap.lock);
  for (xb_tlh *t = heap.threads; t; t = t->next) {
    report_allocation(t);
    for (size_t c = 0; c < XB_GC_CLASSES; ++c) {
      if (t->current[c]) t->current[c]->cached = 0;
      t->current[c] = NULL;
    }
  }
  /* Unswept pages keep the marks of the previous collection: their unmarked
   * objects are dead, and stay unmarked since nothing reaches them. */
  heap.full = full;
  if (full) unmark_all();
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
  /* After a young collection pages are swept lazily, and only those that
   * received objects can have dead ones. A full collection, which already
   * touched every object, sweeps every page at once and gives the pages whose
   * objects all died back to the pool, for any size class. */
  for (size_t c = 0; c < XB_GC_CLASSES; ++c) {
    for (xb_page **pp = &heap.pages[c]; *pp;) {
      xb_page *p = *pp;
      if (!full) {
        if (p->young) p->needsSweep = 1;
        pp = &p->next;
        continue;
      }
      page_sweep(p);
      if (p->live == 0) {
        *pp = p->next;
        p->next = heap.emptyPages;
        heap.emptyPages = p;
      } else {
        pp = &p->next;
      }
    }
  }
  for (size_t c = 0; c < XB_GC_CLASSES; ++c) heap.cursor[c] = heap.pages[c];
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
