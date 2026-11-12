#include "rdd/pages.h"

#include "xbrt_int.h"

#include <stdlib.h>

void xb_pages_init(xb_pages *c, uint32_t max) {
  c->max = max;
  c->count = 0;
  c->newest = c->oldest = 0;
  c->bucketBits = 1;
  while ((1u << c->bucketBits) < 2 * max) ++c->bucketBits; /* about two buckets a slot: short chains */
  c->slot = calloc((size_t)max + 1, sizeof *c->slot);
  c->buckets = calloc((size_t)1 << c->bucketBits, sizeof *c->buckets);
  if (!c->slot || !c->buckets) xb_gc_fatal("out of memory");
}

void xb_pages_free(xb_pages *c) {
  free(c->slot);
  free(c->buckets);
  c->slot = NULL;
  c->buckets = NULL;
}

/* Offsets are multiples of the page size: the multiplicative hash's top bits mix them all. */
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

uint16_t xb_pages_spare(xb_pages *c) {
  if (c->oldest && !c->slot[c->oldest].offset) return c->oldest;
  if (c->count == c->max) return 0;
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
