/* Memo files of the DBF driver's M fields: DBT (dBASE III, Clipper; rdd/dbt.c) and FPT (FoxPro; rdd/fpt.c), one
 * interface. A memo is found by the number of its first block, kept in the record (0: none). */
#ifndef XB_MEMO_H
#define XB_MEMO_H

#include "xbrt_int.h"

typedef struct xb_memo xb_memo;

typedef struct xb_memo_format {
  const char *ext;        /* ".dbt", ".fpt" */
  unsigned char version;  /* the version byte of a table with memo fields in this format */
  /* Creates an empty memo file (emptying one of the name nobody uses). */
  int (*create)(const char *path, int32_t *err);
  /* Opens it, `exclusive` or shared (the format's lock taken when it changes). */
  int (*open)(const char *path, int readonly, int exclusive, xb_memo **out, int32_t *err);
  int (*close)(xb_memo *m, int32_t *err);
  /* The memo that starts at `block` as a string in `r` (another kind of value the format holds: NIL). */
  int (*read)(xb_memo *m, uint32_t block, xb_item *r, int32_t *err);
  /* A record's memo becomes `n` bytes (0: none): the memo at `old` (0: none) is replaced, the new block in `block`. */
  int (*write)(xb_memo *m, uint32_t old, const char *p, size_t n, uint32_t *block, int32_t *err);
  /* Leaves the file with no memo. */
  int (*zap)(xb_memo *m, int32_t *err);
  /* PACK, in steps that each leave every record on a whole memo: copies of the memos kept go after every older one,
   * from block `from` (pack_begin) in the order given (pack_copy: the copy's block); the records take the copies'
   * blocks; the copies move down to the file's first memo block when they fit before themselves (pack_move: every
   * block then `shift` less, 0 when they stay), the records follow, and what is before the copies, or the old memos
   * when they stayed, is dropped (pack_end). */
  int (*pack_begin)(xb_memo *m, uint32_t *from, int32_t *err);
  int (*pack_copy)(xb_memo *m, uint32_t block, uint32_t *copy, int32_t *err);
  int (*pack_move)(xb_memo *m, uint32_t from, uint32_t *shift, int32_t *err);
  int (*pack_end)(xb_memo *m, uint32_t from, uint32_t shift, int32_t *err);
} xb_memo_format;

extern const xb_memo_format xb_memo_dbt;
extern const xb_memo_format xb_memo_fpt;

#endif
