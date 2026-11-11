/* DBT memo files (dBASE III, as Clipper writes them), the memos of the DBF driver's M fields (rdd/dbt.c). */
#ifndef XB_DBT_H
#define XB_DBT_H

#include "xbrt_int.h"

#define XB_DBT_EXT ".dbt"

typedef struct xb_dbt {
  xb_file f;
  uint32_t next; /* the first free block, kept in the file's header */
} xb_dbt;

/* Creates an empty memo file (overwriting one of the name). */
int xb_dbt_create(const char *path, int32_t *err);
int xb_dbt_open(xb_dbt *m, const char *path, int readonly, int32_t *err);
int xb_dbt_close(xb_dbt *m, int32_t *err);

/* The memo that starts at `block` (up to its end mark, or the end of the file) as a string in `r`. */
int xb_dbt_read(xb_dbt *m, uint32_t block, xb_item *r, int32_t *err);
/* Writes `n` bytes as a new memo after the last one: the block it starts at in `block`. */
int xb_dbt_append(xb_dbt *m, const char *p, size_t n, uint32_t *block, int32_t *err);
/* Leaves the file with no memo. */
int xb_dbt_zap(xb_dbt *m, int32_t *err);
/* PACK, after it wrote the memos it keeps after every older one (from block `from` on) and gave the records their
 * blocks: copies them to the first block, when they fit in the blocks before `from` (`moved` tells; otherwise they
 * stay where they are, and the records with them). The older ones are still there until xb_dbt_cut. */
int xb_dbt_move_down(xb_dbt *m, uint32_t from, int *moved, int32_t *err);
/* Drops the older memos once the records give the moved ones' new blocks (b - from + 1 for a memo written at b). */
int xb_dbt_cut(xb_dbt *m, uint32_t from, int32_t *err);

#endif
