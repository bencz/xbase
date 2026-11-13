/* The pages of a file kept in memory, for the drivers (rdd/ntx.c and rdd/cdx.c: an index file's pages; rdd/dbf.c: a
 * table's records, by pages of whole records): slots, made as pages come up to a maximum, found by the page's key (an
 * offset, or a number) through a hash table, and kept in the order of their last use, so the slot a new page takes
 * is a free one, else a new one, else the one used longest ago that the driver can give up. The driver keeps each
 * slot's page (its bytes, whether it changed) by the slot's number, from 1. */
#ifndef XB_RDD_PAGES_H
#define XB_RDD_PAGES_H

#include <stdint.h>

typedef struct xb_page_slot {
  uint32_t offset;           /* the page's key: its offset in the file, or its number from 1; 0: a free slot */
  uint16_t next;             /* the next slot of its hash bucket (0: none) */
  uint16_t newer, older;     /* its neighbours in the order of use (0: none) */
} xb_page_slot;

typedef struct xb_pages {
  xb_page_slot *slot;        /* [1 .. count] */
  uint16_t *buckets;         /* the first slot of each hash bucket (0: none) */
  uint32_t bucketBits;
  uint32_t count, max;       /* slots made, and the most there may be */
  uint16_t newest, oldest;   /* the ends of the order of use; free slots are the oldest */
} xb_pages;

/* At most `max` slots (below 65536). */
void xb_pages_init(xb_pages *c, uint32_t max);
void xb_pages_free(xb_pages *c);

/* The slot holding the page at `offset` (not 0), 0 when none does. */
uint16_t xb_pages_find(const xb_pages *c, uint32_t offset);

/* The slot was just used: it becomes the newest. */
void xb_pages_use(xb_pages *c, uint16_t s);

/* A slot that a new page can take without giving one up: the free one used longest ago, else (with `grow`, the driver's
 * budget allowing) one made now (its number is `count`, the driver makes its page), else 0: the driver gives up the
 * oldest page it can (xb_pages_oldest, then xb_pages_newer), writing it first if it changed. */
uint16_t xb_pages_spare(xb_pages *c, int grow);
static inline uint16_t xb_pages_oldest(const xb_pages *c) { return c->oldest; }
static inline uint16_t xb_pages_newer(const xb_pages *c, uint16_t s) { return c->slot[s].newer; }

/* Gives the slot the page at `offset`, or frees it (0: it becomes the oldest, the first a new page takes). */
void xb_pages_place(xb_pages *c, uint16_t s, uint32_t offset);

#endif
