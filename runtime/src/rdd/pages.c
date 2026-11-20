#include "rdd/pages.h"

#include "xbrt_int.h"

#include "platform/atomic.h"

#include <stdlib.h>

/* What every cache's pages take, and how much of it allows a cache past its driver's minimum to grow. */
static size_t total;
static size_t budget = XB_PAGES_BUDGET;

void xb_pages_init(xb_pages *c, uint32_t max) {
  c->max = max;
  c->count = 0;
  c->newest = c->oldest = 0;
  c->bytes = 0;
  c->bucketBits = 1;
  while ((1u << c->bucketBits) < 2 * max) ++c->bucketBits; /* about two buckets a slot: short chains */
  c->slot = calloc((size_t)max + 1, sizeof *c->slot);
  c->buckets = calloc((size_t)1 << c->bucketBits, sizeof *c->buckets);
  if (!c->slot || !c->buckets) xb_gc_fatal("out of memory");
}

void xb_pages_free(xb_pages *c) {
  xb_atomic_addsz(&total, (size_t)0 - c->bytes, XB_RELAXED);
  c->bytes = 0;
  free(c->slot);
  free(c->buckets);
  c->slot = NULL;
  c->buckets = NULL;
}

/* Keys (offsets that are multiples of a page size, or consecutive numbers): the multiplicative hash's top bits mix
 * them all. */
static uint32_t bucket(const xb_pages *c, uint32_t offset) { return (offset * 2654435761u) >> (32 - c->bucketBits); }

uint16_t xb_pages_find(const xb_pages *c, uint32_t offset) {
  if (c->newest && c->slot[c->newest].offset == offset) return c->newest; /* navigation stays on a page */
  for (uint16_t s = c->buckets[bucket(c, offset)]; s; s = c->slot[s].next)
    if (c->slot[s].offset == offset) return s;
  return 0;
}

static void unlink_order(xb_pages *c, uint16_t s) {
  xb_page_slot *p = &c->slot[s];
  if (p->newer) c->slot[p->newer].older = p->older;
  else c->newest = p->older;
  if (p->older) c->slot[p->older].newer = p->newer;
  else c->oldest = p->newer;
  p->newer = p->older = 0;
}

static void link_newest(xb_pages *c, uint16_t s) {
  c->slot[s].older = c->newest;
  if (c->newest) c->slot[c->newest].newer = s;
  else c->oldest = s;
  c->newest = s;
}

static void link_oldest(xb_pages *c, uint16_t s) {
  c->slot[s].newer = c->oldest;
  if (c->oldest) c->slot[c->oldest].older = s;
  else c->newest = s;
  c->oldest = s;
}

void xb_pages_use(xb_pages *c, uint16_t s) {
  if (c->newest == s) return;
  unlink_order(c, s);
  link_newest(c, s);
}

/* Whether `bytes` more fit the budget: they are counted in the total then. */
static int fits(size_t bytes) {
  size_t now = xb_atomic_loadsz(&total, XB_RELAXED);
  do {
    size_t limit = xb_atomic_loadsz(&budget, XB_RELAXED);
    if (now > limit || bytes > limit - now) return 0;
  } while (!xb_atomic_cassz(&total, &now, now + bytes, XB_RELAXED));
  return 1;
}

uint16_t xb_pages_spare(xb_pages *c, int grow, size_t bytes) {
  if (c->oldest && !c->slot[c->oldest].offset) return c->oldest;
  if (grow == XB_PAGES_KEEP || c->count == c->max) return 0;
  if (grow == XB_PAGES_NEED) xb_atomic_addsz(&total, bytes, XB_RELAXED);
  else if (!fits(bytes)) return 0;
  c->bytes += bytes;
  uint16_t s = (uint16_t)++c->count;
  link_oldest(c, s);
  return s;
}

void xb_pages_place(xb_pages *c, uint16_t s, uint32_t offset) {
  xb_page_slot *p = &c->slot[s];
  if (p->offset) {
    uint16_t *link = &c->buckets[bucket(c, p->offset)];
    while (*link != s) link = &c->slot[*link].next;
    *link = p->next;
  }
  p->offset = offset;
  p->next = 0;
  if (offset) {
    uint16_t *head = &c->buckets[bucket(c, offset)];
    p->next = *head;
    *head = s;
  } else {
    unlink_order(c, s);
    link_oldest(c, s);
  }
}

void xb_pages_grew(xb_pages *c, size_t bytes) {
  xb_atomic_addsz(&total, bytes, XB_RELAXED);
  c->bytes += bytes;
}

/* __dbPageCache( [nBudget] ): { the bytes the pages of every table's and index's cache take, the budget before the
 * call }; a number of 0 or more is the new budget. */
void xbf___DBPAGECACHE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  size_t before = xb_atomic_loadsz(&budget, XB_RELAXED);
  if (argc && xb_is_number(&argv[0]) && xb_num_value(&argv[0]) >= 0) {
    int64_t n = xb_num_to_i64(&argv[0]);
    xb_atomic_storesz(&budget, (uint64_t)n < SIZE_MAX ? (size_t)n : SIZE_MAX, XB_RELAXED);
  }
  xb_array *a = xb_array_alloc(2);
  xb_set_int(&xb_array_items(a)[0], (int64_t)xb_atomic_loadsz(&total, XB_RELAXED));
  xb_set_int(&xb_array_items(a)[1], (int64_t)before);
  xb_set_array(ret, a);
}
