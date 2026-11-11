/* Order files of the DBF driver: NTX (Clipper's, one order per file; rdd/ntx.c) and, planned, CDX (FoxPro's, orders
 * as the tags of one file), one interface. A file is a bag; each order in it a tag, which keeps a cursor on one
 * entry that navigation moves without searching again. Every operation works in the order's own order (a descending
 * order's first entry is its greatest key), whatever the file stores. */
#ifndef XB_INDEX_H
#define XB_INDEX_H

#include "xbrt_int.h"

#define XB_INDEX_KEY_MAX 256  /* bytes of a key, and of the key and condition expressions' text */

typedef struct xb_bag xb_bag;
typedef struct xb_tag xb_tag;

/* What an order is: its expressions' text, its key's type (C N D L), length and decimals, its flags. */
typedef struct xb_index_info {
  char key[XB_INDEX_KEY_MAX + 1];  /* the key expression's text */
  char cond[XB_INDEX_KEY_MAX + 1]; /* the FOR condition's text, "" for none */
  char type;                       /* 0 until the key was evaluated */
  uint32_t keyLen, keyDec;
  int unique, descend;
} xb_index_info;

/* A place among the entries: before the first whose key's first `len` bytes come at or after `key` (`past`: after
 * them), or, with `whole`, before the entry (key, recno) (keyLen bytes compared). */
typedef struct xb_index_bound {
  const unsigned char *key;
  size_t len;
  uint32_t recno;
  int whole, past;
} xb_index_bound;

typedef struct xb_index_format {
  const char *ext;    /* ".ntx" */
  uint32_t keyMax;    /* the longest key */
  int tiesByRecord;   /* a bulk build orders equal keys by record number, not in the order they were collected */

  /* Keys. The length and decimals of a key of value `v` (its type known); a value's key (`*used`: the bytes a SEEK
   * compares, fewer for a shorter string), 0 when its type is not the order's; a key's value again; keys compared
   * in the order's order (the first `len` bytes). */
  void (*key_shape)(const xb_item *v, char type, uint32_t *len, uint32_t *dec);
  int (*value_key)(const xb_index_info *info, const xb_item *v, unsigned char *out, size_t *used);
  void (*key_value)(const xb_index_info *info, const unsigned char *key, xb_item *r);
  int (*compare)(const xb_index_info *info, const unsigned char *a, const unsigned char *b, size_t len);
  /* The order of a bulk build's entries: whether `info`'s file keeps them by key the other way round (an NTX
   * descending order) rather than ascending. */
  int (*stored_descending)(const xb_index_info *info);

  /* Files. Opening gives the bag and its tags (tag_at: in the bag's order, with their names; NULL: the file's name).
   * Building makes tag `name` from `count` entries sorted as stored_descending says (unique already applied): in
   * `*bag` when it is open, else in the file at `path`, opened (`fresh`: made anew, else the tag is added or
   * replaced); the tag in `*tag`. */
  int (*open)(const char *path, int readonly, int exclusive, xb_bag **out, int32_t *err);
  uint32_t (*tag_count)(const xb_bag *b);
  xb_tag *(*tag_at)(xb_bag *b, uint32_t i, xb_index_info *info, const char **name);
  int (*build)(xb_bag **bag, const char *path, const char *name, int fresh, const xb_index_info *info,
               const unsigned char *keys, const uint32_t *recnos, uint32_t count, int readonly, int exclusive,
               xb_tag **tag, int32_t *err);
  /* The key type, once the order's key was evaluated on opening (CDX trims keys by it). */
  void (*set_type)(xb_tag *t, char type);
  /* Holds the bag's lock (shared: `write` or read) across several operations of its tags, which then take it no
   * more themselves, until let go (what changed is written then); nested holds count and keep the first's kind (a
   * read hold allows no change). 0 after an error. */
  int (*hold)(xb_bag *b, int write, int32_t *err);
  int (*let_go)(xb_bag *b, int32_t *err);
  /* Writes the pages and the header that changed; flush and close the bag. */
  int (*flush)(xb_bag *b, int32_t *err);
  int (*close)(xb_bag *b, int32_t *err);

  /* The cursor of a tag. Each returns 1 on an entry (its record number in `recno`), 0 when there is none there (an
   * empty order, past either end), -1 after a file error (in `err`). */
  int (*first)(xb_tag *t, uint32_t *recno, int32_t *err);
  int (*last)(xb_tag *t, uint32_t *recno, int32_t *err);
  int (*next)(xb_tag *t, uint32_t *recno, int32_t *err);
  int (*prev)(xb_tag *t, uint32_t *recno, int32_t *err);
  /* The first entry whose key's first `len` bytes come at or after `key` (`last`: the last one whose first `len`
   * bytes come at or before it). */
  int (*seek)(xb_tag *t, const unsigned char *key, size_t len, int last, uint32_t *recno, int32_t *err);
  /* The entry (key, recno), or the first after where it would be. */
  int (*find)(xb_tag *t, const unsigned char *key, uint32_t recno, uint32_t *found, int32_t *err);
  /* The entries not before `lo` and before `hi` (NULL: no bound that side); not before `lo` and before the cursor's
   * entry; the cursor on entry `k` (from 0) of those not before `lo`. */
  int (*count)(xb_tag *t, const xb_index_bound *lo, const xb_index_bound *hi, uint64_t *n, int32_t *err);
  int (*count_before)(xb_tag *t, const xb_index_bound *lo, uint64_t *n, int32_t *err);
  int (*select)(xb_tag *t, const xb_index_bound *lo, uint64_t k, uint32_t *recno, int32_t *err);
  /* The key of the entry the cursor is on (keyLen bytes; NULL when it is on none), until the cursor moves; whether
   * the cursor is on record `recno`'s entry with nothing changed since it got there. */
  const unsigned char *(*key)(xb_tag *t);
  int (*at)(const xb_tag *t, uint32_t recno);
  /* A record's entry changes: the one of key `old` goes, one of `key` comes (NULL: none), in one operation; the
   * cursor is then on none. Adding an entry a unique order has the key of is nothing. */
  int (*replace)(xb_tag *t, const unsigned char *old, const unsigned char *key, uint32_t recno, int32_t *err);
} xb_index_format;

extern const xb_index_format xb_index_ntx;

#endif
