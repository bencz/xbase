/* Codeblocks and cells.
 *
 * A variable a codeblock captures or that is passed by reference moves into a
 * cell when that first happens, so the block or the callee and the function
 * share it and it survives the function's return. Its frame slot (or STATIC
 * storage) then holds an XB_T_CELL item pointing at the cell; compiled code
 * reaches the value through it. */
#include "xbrt_int.h"

xb_cell *xb_cell_move(xb_item *var) {
  if (var->type == XB_T_CELL) return var->v.p; /* shared already (a parameter: its caller's variable) */
  xb_cell *c = xb_gc_alloc(XB_GC_CELL, sizeof(xb_cell)); /* `var` is a root: a frame slot or STATIC storage */
  c->value = *var;
  xb_set_ptr(var, XB_T_CELL, c);
  return c;
}

xb_cell *xb_cell_of(xb_ctx *ctx, xb_item *var) {
  if (!xb_is_loop_ref(var)) return xb_cell_move(var);
  xb_item element; /* the current element, in a cell of its own: the variable stays the loop's */
  xb_ref_copy(&element, var);
  xb_gc_root_push(ctx, &element);
  xb_cell *c = xb_gc_alloc(XB_GC_CELL, sizeof(xb_cell));
  xb_gc_root_pop(ctx, 1);
  c->value = element;
  return c;
}

xb_block *xb_block_new(xb_ctx *ctx, xb_item *r, xb_block_fn fn, uint32_t ncells) {
  XB_UNUSED(ctx);
  xb_block *b = xb_gc_alloc(XB_GC_BLOCK, sizeof(xb_block) + ncells * sizeof(xb_cell *));
  b->fn = fn;
  b->ncells = ncells;
  for (uint32_t i = 0; i < ncells; ++i) b->cells[i] = NULL; /* filled by the caller */
  xb_set_ptr(r, XB_T_BLOCK, b);
  return b;
}

void xb_block_call(xb_ctx *ctx, xb_item *ret, const xb_item *block, uint32_t argc, xb_item *argv) {
  xb_set_nil(ret);
  if (block->type != XB_T_BLOCK) {
    xb_error_in(ctx, XB_BLOCK_PREFIX "EVAL", ret, EG_NOMETHOD, 1004, "EVAL", XB_ERR_SUBSTITUTE, 1, block);
    return;
  }
  xb_block *b = block->v.p;
  b->fn(ctx, ret, argc, argv, b);
}
