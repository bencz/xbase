/* NTX index files (Clipper's, rdd/ntx.c): one order per file, a B-tree of 1024-byte pages whose entries are a key
 * (bytes compared as they are; reversed for a descending order, record numbers ascending among equal keys) and a
 * record number. Each open file has a cursor on one entry, which navigation moves without searching again. */
#ifndef XB_NTX_H
#define XB_NTX_H

#include "xbrt_int.h"

#define XB_NTX_EXT ".ntx"
#define XB_NTX_KEY_MAX 256 /* bytes of a key, and of the key and condition expressions' text */

typedef struct xb_ntx xb_ntx;

/* What the header describes besides the tree. */
typedef struct xb_ntx_info {
  char key[XB_NTX_KEY_MAX + 1];  /* the key expression's text */
  char cond[XB_NTX_KEY_MAX + 1]; /* the FOR condition's text, "" for none */
  uint32_t keyLen, keyDec;
  int unique, descend;
} xb_ntx_info;

/* Writes a new file from `count` entries in the order's order (`keys`: count * keyLen bytes), overwriting a file of
 * the name, as the reference's bulk build lays it out. */
int xb_ntx_create(const char *path, const xb_ntx_info *info, const unsigned char *keys, const uint32_t *recnos,
                  uint32_t count, int32_t *err);
/* Opens a file, `exclusive` or shared with other users (each operation then under the index lock, its pages read
 * again when another user changed it); `info` gets its header. A header that is not an NTX one is XB_FERR_FAILURE. */
int xb_ntx_open(const char *path, int readonly, int exclusive, xb_ntx **out, xb_ntx_info *info, int32_t *err);
/* Writes the pages and the header that changed; then closes and frees. */
int xb_ntx_flush(xb_ntx *x, int32_t *err);
int xb_ntx_close(xb_ntx *x, int32_t *err);

/* A place among the entries: before the first whose key's first `len` bytes come at or after `key` (`past`: after
 * them), or, with `whole`, before the entry (key, recno) (keyLen bytes compared). */
typedef struct xb_ntx_bound {
  const unsigned char *key;
  size_t len;
  uint32_t recno;
  int whole, past;
} xb_ntx_bound;

/* The cursor. Each returns 1 on an entry (its record number in `recno`), 0 when there is none there (an empty
 * index, past either end), -1 after a file error (in `err`). */
int xb_ntx_first(xb_ntx *x, uint32_t *recno, int32_t *err);
int xb_ntx_last(xb_ntx *x, uint32_t *recno, int32_t *err);
int xb_ntx_next(xb_ntx *x, uint32_t *recno, int32_t *err);
int xb_ntx_prev(xb_ntx *x, uint32_t *recno, int32_t *err);
/* The first entry whose key's first `len` bytes come at or after `key` (`last`: the last one whose first `len` bytes
 * come at or before it). */
int xb_ntx_seek(xb_ntx *x, const unsigned char *key, size_t len, int last, uint32_t *recno, int32_t *err);
/* The entry (key, recno), or the first after where it would be. */
int xb_ntx_find(xb_ntx *x, const unsigned char *key, uint32_t recno, uint32_t *found, int32_t *err);
/* The number of entries not before `lo` and before `hi` (NULL: no bound that side), from the pages' counts: only the
 * pages that hold the bounds are searched. */
int xb_ntx_count(xb_ntx *x, const xb_ntx_bound *lo, const xb_ntx_bound *hi, uint64_t *n, int32_t *err);
/* The number of entries not before `lo` and before the cursor's entry (0 when the cursor is on none). */
int xb_ntx_count_before(xb_ntx *x, const xb_ntx_bound *lo, uint64_t *n, int32_t *err);
/* Places the cursor on entry `k` (from 0) of those not before `lo` (NULL: all): 1, 0 when there are fewer. */
int xb_ntx_select(xb_ntx *x, const xb_ntx_bound *lo, uint64_t k, uint32_t *recno, int32_t *err);
/* The key of the entry the cursor is on (keyLen bytes; NULL when it is on none), until the cursor moves. */
const unsigned char *xb_ntx_key(xb_ntx *x);
/* Whether the cursor is on the entry of record `recno` (an order has one at most), with nothing changed in the index
 * since it got there. */
int xb_ntx_at(const xb_ntx *x, uint32_t recno);

/* A record's entry changes: the one of key `old` goes, one of `key` comes (NULL: none), in one operation (shared: no
 * other program sees the record missing between them); the cursor is then on none. Adding an entry a unique order
 * has the key of is nothing. */
int xb_ntx_replace(xb_ntx *x, const unsigned char *old, const unsigned char *key, uint32_t recno, int32_t *err);

/* Compares keys in the order's order (the first `len` bytes). */
int xb_ntx_compare(const xb_ntx_info *info, const unsigned char *a, const unsigned char *b, size_t len);

#endif
