/* Codeblocks and cells.
 *
 * A LOCAL (or parameter) used by a codeblock lives in a cell, so the block
 * and the function share it and it survives the function's return. The
 * function's frame slot then holds an XB_T_CELL item pointing at the cell;
 * compiled code reaches the value through it. */
#include "xbrt_int.h"

void xb_cell_box(xb_ctx *ctx, xb_item *slot) {
  XB_UNUSED(ctx);
  xb_cell *c = xb_gc_alloc(XB_GC_CELL, sizeof(xb_cell)); /* slot is still a frame root */
  c->value = *slot;
  xb_set_nil(slot);
  slot->type = XB_T_CELL;
  slot->v.p = c;
}

xb_block *xb_block_new(xb_ctx *ctx, xb_item *r, xb_block_fn fn, uint32_t ncells) {
  XB_UNUSED(ctx);
  xb_block *b = xb_gc_alloc(XB_GC_BLOCK, sizeof(xb_block) + ncells * sizeof(xb_cell *));
  b->fn = fn;
  b->ncells = ncells;
  for (uint32_t i = 0; i < ncells; ++i) b->cells[i] = NULL; /* filled by the caller */
  xb_set_nil(r);
  r->type = XB_T_BLOCK;
  r->v.p = b;
  return b;
}

void xb_block_call(xb_ctx *ctx, xb_item *ret, const xb_item *block, uint32_t argc, xb_item *argv) {
  xb_set_nil(ret);
  if (block->type != XB_T_BLOCK) {
    xb_rt_error(ctx, "BASE", 1004, "No exported method", "EVAL");
    return;
  }
  xb_block *b = block->v.p;
  b->fn(ctx, ret, argc, argv, b);
}
