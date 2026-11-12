/* Relations between work areas (SET RELATION; docs/rdd.md#relations) and their functions: dbSetRelation,
 * ordSetRelation, dbClearRelation, dbRelation, dbRSelect.
 *
 * A child moves lazily: a change of its parent's record only marks the move pending (for the child's children too),
 * and the child makes it before its next use, evaluating the expression with the parent's record as it is then. */
#include "rdd/order.h"

#include <stdlib.h>

void xb_area_sync_children(xb_ctx *ctx, xb_area *a) {
  for (uint32_t k = 0; k < a->nrelations; ++k) {
    const xb_relation *rel = a->relations[k];
    xb_area *child = xb_area_at(ctx, rel->child);
    if (!child) continue;
    child->relPending = rel;
    child->relParent = a->number;
    if (child->nrelations) xb_area_sync_children(ctx, child); /* no cycles: xb_area_relation_add refuses them */
  }
}

/* The record number a relation's value names in a child without an order: 0 (none) for a value that is not a
 * number or is out of range. */
static uint32_t record_of(const xb_item *v) {
  if (!xb_is_number(v)) return 0;
  int64_t n = xb_num_to_i64(v);
  return n >= 1 && n <= UINT32_MAX ? (uint32_t)n : 0;
}

/* The index of relation `id` among `parent`'s, -1 when it is no longer there (an expression removed it). */
static int64_t relation_index(const xb_area *parent, uint64_t id) {
  for (uint32_t k = 0; k < parent->nrelations; ++k)
    if (parent->relations[k]->id == id) return k;
  return -1;
}

/* Removes `parent`'s relation `k`; the one place a relation is freed, so no child's pending move names a freed one. */
static void remove_at(xb_ctx *ctx, xb_area *parent, uint32_t k) {
  xb_relation *rel = parent->relations[k];
  xb_area *child = xb_area_at(ctx, rel->child);
  if (child) {
    --child->parents;
    if (child->relPending == rel) child->relPending = NULL;
  }
  for (uint32_t i = k + 1; i < parent->nrelations; ++i) parent->relations[i - 1] = parent->relations[i];
  --parent->nrelations;
  free(rel);
}

/* Moves `a` where `parent`'s relation `id` puts it: the end when the parent is at its end; else a SEEK of the
 * expression's value (no soft seek) in the controlling order, within a scope of that value for a scoped relation;
 * without an order, the record of that number, Found() telling whether there is one. Nothing when the expression
 * removed the relation. */
static int follow(xb_ctx *ctx, xb_area *a, xb_area *parent, uint64_t id) {
  if (parent->eof) return xb_area_goto(ctx, a, 0);
  xb_item v;
  xb_set_nil(&v);
  xb_gc_root_push(ctx, &v);
  int ok = xb_area_eval(ctx, parent, &parent->relations[relation_index(parent, id)]->block, &v) && !a->closed &&
           !parent->closed;
  a->relPending = NULL; /* the moves of the parent the expression made are this one */
  int64_t k = ok ? relation_index(parent, id) : -1;
  if (k >= 0) {
    int scoped = parent->relations[k]->scoped;
    xb_order *o = xb_area_order(a);
    if (!o) {
      ok = xb_area_goto(ctx, a, record_of(&v));
      a->found = ok && !a->eof;
    } else {
      if (scoped) ok = xb_order_set_scope(ctx, a, o, 0, &v) && !a->closed && xb_order_set_scope(ctx, a, o, 1, &v);
      ok = ok && !a->closed && xb_area_seek(ctx, a, &v, 0, 0);
    }
  }
  xb_gc_root_pop(ctx, 1);
  return ok;
}

int xb_area_settle(xb_ctx *ctx, xb_area *a) {
  const xb_relation *rel = a->relPending;
  if (!rel || a->settling) return 1;
  uint64_t id = rel->id;
  a->relPending = NULL;
  xb_area *parent = xb_area_at(ctx, a->relParent);
  if (!parent) return 1;
  xb_area_pin(a);
  xb_area_pin(parent);
  a->settling = 1;
  int ok = xb_area_settle(ctx, parent) && !parent->closed && !a->closed;
  if (ok && relation_index(parent, id) >= 0) ok = follow(ctx, a, parent, id);
  a->settling = 0;
  xb_area_unpin(parent);
  xb_area_unpin(a);
  return ok;
}

int xb_area_settle_scoped(xb_ctx *ctx, xb_area *a) {
  return !a->relPending || !a->relPending->scoped || xb_area_settle(ctx, a);
}

int xb_area_drop_pending(xb_ctx *ctx, xb_area *a) {
  if (!xb_area_settle_scoped(ctx, a)) return 0;
  a->relPending = NULL;
  return 1;
}

/* Whether `a` is `target` or leads to it through its relations. */
static int leads_to(xb_ctx *ctx, const xb_area *a, const xb_area *target) {
  if (a == target) return 1;
  for (uint32_t k = 0; k < a->nrelations; ++k) {
    const xb_area *child = xb_area_at(ctx, a->relations[k]->child);
    if (child && leads_to(ctx, child, target)) return 1;
  }
  return 0;
}

int xb_area_relation_add(xb_ctx *ctx, xb_area *a, xb_area *child, const xb_item *block, const xb_item *text,
                         int scoped) {
  if (a->closing || child->closing || leads_to(ctx, child, a)) return 0;
  xb_relation *rel = malloc(sizeof *rel);
  xb_relation **grown = rel ? realloc(a->relations, (a->nrelations + 1) * sizeof *grown) : NULL;
  if (!grown) xb_gc_fatal("out of memory");
  rel->id = ++a->relationIds;
  rel->child = child->number;
  rel->block = *block;
  rel->text = *text;
  rel->scoped = scoped;
  a->relations = grown;
  a->relations[a->nrelations++] = rel;
  ++child->parents;
  child->relPending = rel;
  child->relParent = a->number;
  if (child->nrelations) xb_area_sync_children(ctx, child);
  return 1;
}

void xb_area_relations_clear(xb_ctx *ctx, xb_area *a) {
  xb_area_pin(a);
  while (a->nrelations && !a->closed) { /* the first one first, as they were set */
    const xb_relation *rel = a->relations[0];
    uint64_t id = rel->id;
    int scoped = rel->scoped;
    xb_area *child = xb_area_at(ctx, rel->child);
    if (child) { /* the move the relation left pending is made, a scoped one's scope goes */
      xb_area_pin(child);
      if (child->relPending == rel) xb_area_settle(ctx, child);
      xb_order *o = scoped && !child->closed ? xb_area_order(child) : NULL;
      if (o) {
        xb_item nil;
        xb_set_nil(&nil);
        xb_order_set_scope(ctx, child, o, 0, &nil);
        xb_order_set_scope(ctx, child, o, 1, &nil);
      }
      xb_area_unpin(child);
    }
    int64_t k = a->closed ? -1 : relation_index(a, id); /* the expressions may have removed it */
    if (k >= 0) remove_at(ctx, a, (uint32_t)k);
  }
  xb_area_unpin(a);
}

void xb_area_relations_detach(xb_ctx *ctx, xb_area *a) {
  for (uint32_t n = 1; n <= ctx->env->areaCap && a->parents; ++n) {
    xb_area *p = xb_area_at(ctx, n);
    for (uint32_t k = p ? p->nrelations : 0; k-- > 0;)
      if (p->relations[k]->child == a->number) remove_at(ctx, p, k);
  }
  a->relPending = NULL;
}

/* dbSetRelation( xArea, bExpr, [cExpr], [lScoped] ): a relation from the current area into the area xArea names (an
 * alias, which names no area: BASE/1002, which may be retried; or a number). The current area itself, or one that
 * leads back to it: DBCMD/1013; no area there (or one closing), or no block: DBCMD/1004. */
static void set_relation(xb_ctx *ctx, uint32_t argc, xb_item *argv, int scoped) {
  xb_area *a = xb_db_used(ctx, "DBSETRELATION");
  if (!a) return;
  uint32_t n = 0;
  if (xb_db_str_arg(argc, argv, 0)) {
    if (!xb_area_lookup(ctx, &argv[0], &n) || !(a = xb_area_current(ctx))) return; /* the error block may close it */
  } else {
    int64_t num = xb_db_num_arg(argc, argv, 0, 0);
    n = xb_area_number(num);
  }
  xb_area *child = xb_area_at(ctx, n);
  int noBlock = argc < 2 || argv[1].type != XB_T_BLOCK;
  int32_t subcode = child && leads_to(ctx, child, a) ? 1013 : !child || noBlock ? 1004 : 0;
  xb_item text;
  if (xb_db_str_arg(argc, argv, 2)) text = argv[2];
  else xb_set_str(&text, xb_str_empty());
  if (!subcode && !xb_area_relation_add(ctx, a, child, &argv[1], &text, scoped)) subcode = 1004; /* closing */
  if (subcode) xb_error_of(ctx, XB_DBCMD, EG_ARG, subcode, "DBSETRELATION", NULL, 0, 0, 0, NULL, 0, NULL);
}

void xbf_DBSETRELATION(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  set_relation(ctx, argc, argv, xb_db_logical_arg(argc, argv, 3));
}

/* ordSetRelation( xArea, bKey, [cKey] ): dbSetRelation() of a scoped relation. */
void xbf_ORDSETRELATION(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  set_relation(ctx, argc, argv, 1);
}

void xbf_DBCLEARRELATION(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_area *a = xb_area_current(ctx);
  if (a) xb_area_relations_clear(ctx, a);
}

/* The current area's relation `nRelation` (1-based; 0 or none: the first), NULL for none. */
static const xb_relation *relation_arg(xb_ctx *ctx, uint32_t argc, const xb_item *argv) {
  const xb_area *a = xb_area_current(ctx);
  int64_t n = xb_db_num_arg(argc, argv, 0, 0);
  if (n == 0) n = 1;
  return a && n >= 1 && n <= a->nrelations ? a->relations[n - 1] : NULL;
}

/* dbRelation( [nRelation] ): the relation's expression as text ("" for none). */
void xbf_DBRELATION(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_relation *rel = relation_arg(ctx, argc, argv);
  if (rel) *ret = rel->text;
  else xb_set_str(ret, xb_str_empty());
}

/* dbRSelect( [nRelation] ): the relation's child area (0 for none). */
void xbf_DBRSELECT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_relation *rel = relation_arg(ctx, argc, argv);
  xb_set_int(ret, rel ? rel->child : 0);
}
