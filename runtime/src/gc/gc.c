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
 * Collection. Marking starts from the item frames and C roots of every
 * registered context (every task, running or suspended) and from the global
 * roots, and uses an explicit stack. Large objects are swept right away;
 * pages are swept lazily, when an allocation needs a page with free slots,
 * so the cost is spread over the following allocations. Collections are
 * triggered by the bytes allocated since the last one, scaled to the live
 * heap.
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
#define XB_GC_MIN_THRESHOLD ((size_t)8 << 20)    /* bytes between collections, at least */
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
  uint8_t needsSweep;       /* marked since its last sweep */
  uint8_t cached;           /* some thread heap allocates from it */
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
  size_t threshold;
  size_t liveBytes;                /* found by the last marking */
  size_t liveObjects;
  xb_gc_hdr **stack;               /* marking stack */
  size_t stackLen, stackCap;
} heap;

static XB_THREAD_LOCAL xb_tlh *tlh;

void xb_gc_fatal(const char *why) {
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
  heap.threshold = XB_GC_MIN_THRESHOLD;
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


static void page_format(xb_page *p, uint32_t cls) {
  p->cls = cls;
  p->slotSize = class_sizes[cls];
  p->nslots = (uint32_t)((XB_GC_PAGE_SIZE - (size_t)(p->data - (char *)p)) / p->slotSize);
  p->live = 0;
  p->needsSweep = 0;
  p->freeList = NULL;
  for (uint32_t i = p->nslots; i-- > 0;) {
    xb_gc_hdr *h = (xb_gc_hdr *)(p->data + (size_t)i * p->slotSize);
    h->kind = XB_GC_FREE;
    h->marked = 0;
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

/* Rebuilds the free list from the mark bits of the last collection. */
static void page_sweep(xb_page *p) {
  void *freeList = NULL;
  uint32_t live = 0;
  for (uint32_t i = p->nslots; i-- > 0;) {
    xb_gc_hdr *h = (xb_gc_hdr *)(p->data + (size_t)i * p->slotSize);
    if (h->kind != XB_GC_FREE) {
      if (h->marked) {
        h->marked = 0;
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
    int collect = heap.allocated >= heap.threshold;
    xb_mutex_unlock(&heap.lock);
    if (collect) xb_gc_collect();
    xb_mutex_lock(&heap.lock);
    if (t->current[cls]) t->current[cls]->cached = 0;
    p = page_for(cls);
    p->cached = 1;
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
  int collect = heap.allocated >= heap.threshold;
  xb_mutex_unlock(&heap.lock);
  if (collect) xb_gc_collect();
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
  h->marked = 0;
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


static void push(xb_gc_hdr *h) {
  if (h->marked) return;
  h->marked = 1;
  if (heap.stackLen == heap.stackCap) {
    size_t cap = heap.stackCap ? heap.stackCap * 2 : 4096;
    xb_gc_hdr **s = realloc(heap.stack, cap * sizeof *s);
    if (!s) out_of_memory();
    heap.stack = s;
    heap.stackCap = cap;
  }
  heap.stack[heap.stackLen++] = h;
}

static void mark_item(const xb_item *it) {
  switch ((xb_type)it->type) {
    case XB_T_STRING:
      if (!(it->v.s->flags & XB_STR_STATIC)) push(xb_gc_header(it->v.s));
      break;
    case XB_T_ARRAY:
    case XB_T_BLOCK:
    case XB_T_CELL:
    case XB_T_REF:
      push(xb_gc_header(it->v.p));
      break;
    default:
      break;
  }
}

static void trace(xb_gc_hdr *h) {
  heap.liveBytes += h->size;
  ++heap.liveObjects;
  const void *obj = h + 1;
  switch ((xb_gc_kind)h->kind) {
    case XB_GC_ARRAY: {
      const xb_array *a = obj;
      if (a->items != a->inline_items) push(xb_gc_header(a->items));
      for (size_t i = 0; i < a->len; ++i) mark_item(&a->items[i]);
      break;
    }
    case XB_GC_BLOCK: {
      const xb_block *b = obj;
      for (uint32_t i = 0; i < b->ncells; ++i)
        if (b->cells[i]) push(xb_gc_header(b->cells[i])); /* NULL until the creator fills it */
      break;
    }
    case XB_GC_CELL:
      mark_item(&((const xb_cell *)obj)->value);
      break;
    case XB_GC_REF:
      mark_item(&((const xb_ref *)obj)->coll);
      mark_item(&((const xb_ref *)obj)->value);
      break;
    default:
      break;
  }
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

void xb_gc_collect(void) {
  xb_mutex_lock(&heap.lock);
  for (xb_tlh *t = heap.threads; t; t = t->next) {
    report_allocation(t);
    for (size_t c = 0; c < XB_GC_CLASSES; ++c) {
      if (t->current[c]) t->current[c]->cached = 0;
      t->current[c] = NULL;
    }
  }
  /* Pages not swept since the previous collection still hold its marks. */
  for (size_t c = 0; c < XB_GC_CLASSES; ++c)
    for (xb_page *p = heap.pages[c]; p; p = p->next)
      if (p->needsSweep) page_sweep(p);

  heap.liveBytes = 0;
  heap.liveObjects = 0;
  mark_roots();
  while (heap.stackLen) trace(heap.stack[--heap.stackLen]);

  for (xb_large **pl = &heap.large; *pl;) {
    xb_large *l = *pl;
    if (l->hdr.marked) {
      l->hdr.marked = 0;
      pl = &l->link.next;
    } else {
      *pl = l->link.next;
      free(l);
    }
  }
  /* Pages are swept lazily; a page whose objects all died goes back to the pool. */
  for (size_t c = 0; c < XB_GC_CLASSES; ++c) {
    for (xb_page **pp = &heap.pages[c]; *pp;) {
      xb_page *p = *pp;
      p->needsSweep = 1;
      if (p->live == 0) { /* nothing was allocated in it since its last sweep */
        *pp = p->next;
        p->next = heap.emptyPages;
        heap.emptyPages = p;
      } else {
        pp = &p->next;
      }
    }
  }
  for (size_t c = 0; c < XB_GC_CLASSES; ++c) heap.cursor[c] = heap.pages[c];
  heap.allocated = 0;
  heap.threshold = heap.liveBytes > XB_GC_MIN_THRESHOLD ? heap.liveBytes : XB_GC_MIN_THRESHOLD;
  xb_mutex_unlock(&heap.lock);
}

size_t xb_gc_live_objects(void) {
  xb_mutex_lock(&heap.lock);
  size_t n = heap.liveObjects;
  xb_mutex_unlock(&heap.lock);
  return n;
}
