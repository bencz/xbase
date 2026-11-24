/* Codeblocks and cells.
 *
 * A variable a codeblock captures or that is passed by reference moves into a
 * cell when that first happens, so the block or the callee and the function
 * share it and it survives the function's return. Its frame slot (or STATIC
 * storage) then holds an XB_T_CELL item pointing at the cell; compiled code
 * reaches the value through it. */
#include "xbrt_int.h"

#include "core/shared.h"

/* A STATIC's own item once the program's storage is shared, moved into a cell under the stripe of its address: the
 * cell is made and shared first (nothing else reaches it yet), then takes the value and replaces it, unless another
 * thread moved it meanwhile. */
static xb_cell *static_cell(xb_ctx *ctx, xb_item *var) {
  xb_item cell;
  xb_seq_get(var, &cell, var);
  if (cell.type == XB_T_CELL) return cell.v.p;
  xb_set_ptr(&cell, XB_T_CELL, xb_gc_alloc(XB_GC_CELL, sizeof(xb_cell)));
  xb_set_nil(&((xb_cell *)cell.v.p)->value);
  xb_share(&cell);
  xb_gc_root_push(ctx, &cell);
  xb_seq_lock(var);
  if (var->type == XB_T_CELL) {
    cell = *var;
  } else {
    ((xb_cell *)cell.v.p)->value = *var; /* shared already, as all the storage holds */
    xb_gc_write(cell.v.p);                /* the cell may have aged while the lock was waited for */
    xb_seq_store(var, &cell);
  }
  xb_seq_unlock(var);
  xb_gc_root_pop(ctx, 1);
  return cell.v.p;
}

xb_cell *xb_cell_move(xb_ctx *ctx, xb_item *var, uint32_t is_static) {
  if (is_static && xb_storage_is_shared()) return static_cell(ctx, var);
  if (var->type == XB_T_CELL) return var->v.p; /* shared already (a parameter: its caller's variable) */
  xb_cell *c = xb_gc_alloc(XB_GC_CELL, sizeof(xb_cell)); /* `var` is a root: a frame slot or STATIC storage */
  if (is_static && xb_storage_is_shared()) return static_cell(ctx, var); /* shared while it allocated: c is garbage */
  c->value = *var;
  xb_set_ptr(var, XB_T_CELL, c);
  return c;
}

xb_cell *xb_cell_of(xb_ctx *ctx, xb_item *var, uint32_t is_static) {
  xb_item element; /* the variable's own item, read whole when it is shared storage */
  if (is_static && xb_storage_is_shared()) xb_seq_get(var, &element, var);
  else element = *var;
  if (!xb_is_loop_ref(&element)) return xb_cell_move(ctx, var, is_static);
  /* the current element, in a cell of its own: the variable stays the loop's (rooted: the loop's reference, then the
   * copy) */
  xb_gc_root_push(ctx, &element);
  xb_ref_copy(&element, &element);
  xb_cell *c = xb_gc_alloc(XB_GC_CELL, sizeof(xb_cell));
  xb_gc_root_pop(ctx, 1);
  c->value = element;
  return c;
}

xb_cell *xb_cell_new(xb_ctx *ctx, const xb_item *value) {
  xb_item v = *value; /* rooted while the cell is allocated: `value` may be a scratch item no collection sees */
  xb_gc_root_push(ctx, &v);
  xb_cell *c = xb_gc_alloc(XB_GC_CELL, sizeof(xb_cell));
  xb_gc_root_pop(ctx, 1);
  c->value = v;
  return c;
}

xb_block *xb_block_new(xb_ctx *ctx, xb_item *r, xb_block_fn fn, xb_direct direct, uint32_t nparams,
                       uint32_t ncells) {
  XB_UNUSED(ctx);
  xb_block *b = xb_gc_alloc(XB_GC_BLOCK, sizeof(xb_block) + ncells * sizeof(xb_cell *));
  b->fn = fn;
  b->direct = direct;
  b->nparams = nparams;
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

void xb_cell_get_shared(const xb_cell *c, xb_item *dst) {
  xb_seq_get(c, dst, &c->value); /* retried until whole: `dst` ends consistent */
}

void xb_cell_put_shared(xb_cell *c, const xb_item *v) {
  xb_share(v);
  xb_seq_lock(c);
  xb_seq_store(&c->value, v);
  xb_gc_write(c);
  xb_seq_unlock(c);
}
