/* Orders of a work area (rdd/order.h): keys made, kept up to date and searched in the order's file (rdd/index.h). */
#include "rdd/order.h"

#include "core/file.h"
#include "core/numeric.h"

#include <stdlib.h>
#include <string.h>

static char type_of(const xb_item *v) {
  switch (v->type) {
  case XB_T_STRING: return 'C';
  case XB_T_INT:
  case XB_T_DOUBLE: return 'N';
  case XB_T_DATE: return 'D';
  case XB_T_LOGICAL: return 'L';
  default: return 0;
  }
}

int xb_order_value_key(const xb_order *o, const xb_item *v, unsigned char *out, size_t *used) {
  return o->format->value_key(&o->info, v, out, used);
}

/* A key of a type an order cannot have. */
static void key_type_error(xb_ctx *ctx, xb_area *a) {
  xb_error_of(ctx, a->rdd->name, EG_DATATYPE, 1026, "", NULL, 0, 0, 0, NULL, 0, NULL);
}

/* xb_area_eval of a key, condition or scope block: while it runs, the area's orders may not change (`keying`;
 * ordcmd.c refuses it). */
static int eval_in(xb_ctx *ctx, xb_area *a, const xb_item *block, xb_item *r) {
  ++a->keying;
  int ok = xb_area_eval(ctx, a, block, r);
  --a->keying;
  return ok;
}

/* The key of the current record: 1 with it in `out` and in `*in` whether the record has an entry (the condition's),
 * 0 after an error (raised), or when the evaluation closed the area. */
static int record_key(xb_ctx *ctx, xb_area *a, xb_order *o, unsigned char *out, int *in) {
  if (o->field >= 0) { /* a character field's bytes */
    if (!xb_area_load(ctx, a) || a->closed) return 0;
    memcpy(out, a->rdd->field_bytes(a, (uint32_t)o->field), o->info.keyLen);
    *in = 1;
    if (o->cond.type != XB_T_BLOCK) return 1;
  }
  xb_item r;
  xb_set_nil(&r);
  xb_gc_root_push(ctx, &r);
  int ok = 1, met = 1;
  if (o->cond.type == XB_T_BLOCK) {
    ok = eval_in(ctx, a, &o->cond, &r) && !a->closed; /* closed: `o` and `in` may be gone */
    met = ok && r.type == XB_T_LOGICAL && r.v.l;
  }
  if (ok && o->field < 0) {
    ok = eval_in(ctx, a, &o->key, &r) && !a->closed;
    size_t used;
    if (ok && !xb_order_value_key(o, &r, out, &used)) {
      key_type_error(ctx, a);
      ok = 0;
    }
  }
  xb_gc_root_pop(ctx, 1);
  if (ok) *in = met;
  return ok;
}

static void order_error(xb_ctx *ctx, xb_area *a, const xb_order *o, int32_t gencode, int32_t subcode, int32_t err) {
  xb_error_of(ctx, a->rdd->name, gencode, subcode, "", o->bag->path, err, 0, 0, NULL, 0, NULL);
}

/* An order's name from `len` bytes of text: upper case, its first XB_INDEX_NAME_MAX characters, without trailing
 * blanks (malloc'ed). */
static size_t name_into(const char *text, size_t len, char out[XB_INDEX_NAME_MAX + 1]) {
  if (len > XB_INDEX_NAME_MAX) len = XB_INDEX_NAME_MAX;
  while (len && text[len - 1] == ' ') --len;
  for (size_t i = 0; i < len; ++i) out[i] = xb_ascii_upper(text[i]);
  out[len] = '\0';
  return len;
}

static char *order_name(const char *text, size_t len) {
  char n[XB_INDEX_NAME_MAX + 1];
  size_t got = name_into(text, len, n);
  char *copy = malloc(got + 1);
  if (!copy) xb_gc_fatal("out of memory");
  memcpy(copy, n, got + 1);
  return copy;
}

uint32_t xb_area_order_named(const xb_area *a, const char *name, size_t len, const char *path) {
  char n[XB_INDEX_NAME_MAX + 1];
  name_into(name, len, n);
  for (uint32_t i = 0; i < a->norders; ++i)
    if (strcmp(a->orders[i]->name, n) == 0 && (!path || strcmp(a->orders[i]->bag->path, path) == 0)) return i + 1;
  return 0;
}

/* The name of a file's order: the file's name without directories and extension. */
static char *file_order_name(const char *path) {
  size_t start, ext;
  xb_file_name_parts(path, strlen(path), &start, &ext);
  return order_name(path + start, ext - start);
}

/* A codeblock from an expression's text: NIL after the error (raised). */
static void block_of(xb_ctx *ctx, const char *text, xb_item *r) {
  size_t n = strlen(text);
  char *src = malloc(n + 8);
  if (!src) xb_gc_fatal("out of memory");
  memcpy(src, "{|| ", 4);
  memcpy(src + 4, text, n);
  memcpy(src + 4 + n, " }", 3);
  xb_item t;
  xb_set_str(&t, xb_str_new(src, n + 6));
  free(src);
  xb_gc_root_push(ctx, &t);
  xb_macro_get(ctx, r, &t);
  xb_gc_root_pop(ctx, 1);
  if (r->type != XB_T_BLOCK) xb_set_nil(r);
}

/* The field a key expression is when it is a character field's name alone (-1 otherwise). */
static int64_t key_field(xb_area *a, const char *text, uint32_t keyLen) {
  size_t start = 0, end = strlen(text);
  while (start < end && text[start] == ' ') ++start;
  while (end > start && text[end - 1] == ' ') --end;
  for (size_t i = start; i < end; ++i) {
    char c = xb_ascii_upper(text[i]);
    if (!((c >= 'A' && c <= 'Z') || c == '_' || (i > start && c >= '0' && c <= '9'))) return -1;
  }
  int64_t k = end > start ? xb_area_field_index(a, text + start, end - start) : -1;
  if (k < 0 || a->fields[k].type != 'C' || a->fields[k].len != keyLen) return -1;
  return k;
}

/* An order of bag `bag` (counted there already), named `name` (order_name(), which it keeps). */
static xb_order *order_new(xb_order_bag *bag, char *name) {
  xb_order *o = calloc(1, sizeof *o);
  if (!o) xb_gc_fatal("out of memory");
  o->bag = bag;
  o->format = bag->format;
  o->name = name;
  xb_set_nil(&o->key);
  xb_set_nil(&o->cond);
  o->field = -1;
  return o;
}

static int bag_release(xb_order_bag *b, int32_t *err);

/* Frees the order and gives up its place in its bag. 0 after the bag's close error (in `err`). */
static int order_free(xb_order *o, int32_t *err) {
  int ok = bag_release(o->bag, err);
  free(o->name);
  free(o);
  return ok;
}

/* The key entries a build collects: keyLen bytes each, the record numbers beside them. */
typedef struct collected {
  unsigned char *keys;
  uint32_t *recnos;
  uint32_t n, cap;
} collected;

/* A sort entry: the key's first 8 bytes as a number (in the order's order), its place in the collection. */
typedef struct sorting {
  uint64_t prefix;
  uint32_t at;
} sorting;

/* What a sort compares: the collection, the order's format and its stored order. */
typedef struct sorting_by {
  const collected *c;
  const xb_index_format *format;
  const xb_index_info *info; /* descend: as the file stores entries */
} sorting_by;

/* Whether entry a comes before entry b: by key, then by record number or, as NTX does, in the order collected (a
 * build that walks another order keeps its order among equal keys, and UNIQUE its first). */
static int sorts_before(const sorting *a, const sorting *b, const sorting_by *by) {
  if (a->prefix != b->prefix) return a->prefix < b->prefix;
  uint32_t len = by->info->keyLen;
  const collected *c = by->c;
  if (len > 8) {
    int d = by->format->compare(by->info, c->keys + (size_t)a->at * len + 8, c->keys + (size_t)b->at * len + 8,
                                len - 8);
    if (d) return d < 0;
  }
  return by->format->tiesByRecord ? c->recnos[a->at] < c->recnos[b->at] : a->at < b->at;
}

#define SORT_RUN 16 /* entries sorted by insertion before the merges */

/* Sorts the entries: runs by insertion, then merges of runs twice as long each pass. */
static void sort_entries(sorting *v, uint32_t n, const sorting_by *by) {
  for (uint32_t lo = 0; lo < n; lo += SORT_RUN) {
    uint32_t hi = lo + SORT_RUN < n ? lo + SORT_RUN : n;
    for (uint32_t i = lo + 1; i < hi; ++i) {
      sorting e = v[i];
      uint32_t j = i;
      for (; j > lo && sorts_before(&e, &v[j - 1], by); --j) v[j] = v[j - 1];
      v[j] = e;
    }
  }
  if (n <= SORT_RUN) return;
  sorting *tmp = malloc((size_t)n * sizeof *tmp), *src = v, *dst = tmp;
  if (!tmp) xb_gc_fatal("out of memory");
  for (uint32_t width = SORT_RUN; width < n; width *= 2) {
    for (uint32_t lo = 0; lo < n; lo += 2 * width) {
      uint32_t mid = lo + width < n ? lo + width : n, hi = lo + 2 * width < n ? lo + 2 * width : n;
      uint32_t i = lo, j = mid, k = lo;
      while (i < mid && j < hi) dst[k++] = sorts_before(&src[j], &src[i], by) ? src[j++] : src[i++];
      while (i < mid) dst[k++] = src[i++];
      while (j < hi) dst[k++] = src[j++];
    }
    sorting *t = src;
    src = dst;
    dst = t;
    if (width > UINT32_MAX / 2) break;
  }
  if (src != v) memcpy(v, src, (size_t)n * sizeof *v);
  free(tmp);
}

/* Writes the collected entries, sorted as the file stores them (unique: the first of equal keys), as the order's tag
 * (`fresh`: in a file made anew). */
static int write_sorted(xb_area *a, xb_order *o, collected *c, int fresh, int32_t *err) {
  xb_index_info stored = o->info;
  stored.descend = o->format->stored_descending(&o->info);
  const xb_index_info *info = &stored;
  sorting_by by = {c, o->format, info};
  uint32_t len = info->keyLen;
  sorting *order = malloc((size_t)(c->n ? c->n : 1) * sizeof *order);
  unsigned char *keys = malloc((size_t)(c->n ? c->n : 1) * len);
  uint32_t *recnos = malloc((size_t)(c->n ? c->n : 1) * sizeof *recnos);
  if (!order || !keys || !recnos) xb_gc_fatal("out of memory");
  for (uint32_t i = 0; i < c->n; ++i) {
    const unsigned char *k = c->keys + (size_t)i * len;
    uint64_t p = 0;
    for (uint32_t j = 0; j < 8; ++j) p = p << 8 | (j < len ? k[j] : 0);
    order[i].prefix = info->descend ? ~p : p;
    order[i].at = i;
  }
  sort_entries(order, c->n, &by);
  uint32_t n = 0;
  for (uint32_t i = 0; i < c->n; ++i) {
    const unsigned char *k = c->keys + (size_t)order[i].at * len;
    if (info->unique && n > 0 && memcmp(keys + (size_t)(n - 1) * len, k, len) == 0) continue;
    memcpy(keys + (size_t)n * len, k, len);
    recnos[n++] = c->recnos[order[i].at];
  }
  int ok = o->format->build(&o->bag->bag, o->bag->path, o->name, fresh, &o->info, keys, recnos, n,
                            a->rdd->lastrec(a), a->readonly, !a->shared, &o->tag, err);
  free(order);
  free(keys);
  free(recnos);
  return ok;
}

/* The key's type and length from the current record's key (the blank record's when there is none), and whether it
 * is a character field's bytes. 0 after an error (raised). */
static int key_shape(xb_ctx *ctx, xb_area *a, xb_order *o) {
  xb_item v;
  xb_set_nil(&v);
  xb_gc_root_push(ctx, &v);
  int ok = eval_in(ctx, a, &o->key, &v) && !a->closed;
  o->info.type = ok ? type_of(&v) : 0;
  if (ok && !o->info.type) key_type_error(ctx, a);
  if (o->info.type) o->format->key_shape(&v, o->info.type, &o->info.keyLen, &o->info.keyDec);
  if (o->info.type == 'C' && o->info.keyLen > o->format->keyMax) o->info.keyLen = o->format->keyMax;
  xb_gc_root_pop(ctx, 1);
  if (!o->info.type) return 0;
  if (o->info.keyLen == 0 || o->info.keyLen > o->format->keyMax) {
    xb_error_of(ctx, a->rdd->name, EG_DATAWIDTH, 1021, "", o->bag->path, 0, 0, 0, NULL, 0, NULL);
    return 0;
  }
  o->field = key_field(a, o->info.key, o->info.keyLen);
  return 1;
}

/* Adds the current record's entry when it has one; 0 after an error (raised). */
static int collect_record(xb_ctx *ctx, xb_area *a, xb_order *o, collected *c) {
  unsigned char key[XB_INDEX_KEY_MAX];
  int in;
  if (!record_key(ctx, a, o, key, &in)) return 0;
  if (!in) return 1;
  if (!c->keys || c->n == c->cap) { /* room for lastrec entries at first, more when the blocks append records */
    uint32_t cap = c->cap;
    if (c->keys || !cap) cap = cap < 1024 ? 1024 : cap <= UINT32_MAX / 2 ? cap * 2 : UINT32_MAX;
    if (c->n == cap || (uint64_t)cap * (2u * o->info.keyLen + sizeof(uint32_t) * 2 + sizeof(sorting)) > SIZE_MAX)
      xb_gc_fatal("out of memory"); /* the keys, sorted and not, and their records: more than an address reaches */
    unsigned char *keys = realloc(c->keys, (size_t)cap * o->info.keyLen);
    if (keys) c->keys = keys;
    uint32_t *recnos = realloc(c->recnos, (size_t)cap * sizeof *c->recnos);
    if (recnos) c->recnos = recnos;
    if (!keys || !recnos) xb_gc_fatal("out of memory");
    c->cap = cap;
  }
  memcpy(c->keys + (size_t)c->n * o->info.keyLen, key, o->info.keyLen);
  c->recnos[c->n++] = a->recno;
  return 1;
}

/* Whether a WHILE or EVAL block lets the build go on: a logical true. -1 after an error (raised). */
static int block_true(xb_ctx *ctx, xb_area *a, const xb_item *block) {
  xb_item r;
  xb_set_nil(&r);
  xb_gc_root_push(ctx, &r);
  int ok = eval_in(ctx, a, block, &r) && !a->closed;
  xb_gc_root_pop(ctx, 1);
  return ok ? r.type == XB_T_LOGICAL && r.v.l : -1;
}

/* A record number from a number argument: 0 (no record) outside 1 .. 2^32-1. */
static uint32_t recno_of(int64_t n) { return n > 0 && n <= UINT32_MAX ? (uint32_t)n : 0; }

/* The records of a new order, as ordCondSet() asked (xb_order_conditions): RECORD the one record; NEXT, REST and
 * WHILE from the start record (the current one by default); USECURRENT from the controlling order's first record in
 * its scope; else every record in the record order. Those that start somewhere walk the controlling order (the
 * record order without one). The walk is raw: the filter and SET DELETED do not apply. WHILE ends it at a record it
 * rejects; the EVAL block runs after the first record and then after every `every` more (not after NEXT's last one),
 * and ends the build, keeping what it has, when it returns anything but true. Records the blocks append are not
 * taken; in record order, the blocks moving the record do not change which records are taken. 0 after an error
 * (raised). */
static int collect(xb_ctx *ctx, xb_area *a, xb_order *o, const xb_order_conditions *cond, collected *c) {
  uint32_t last = c->cap = a->rdd->lastrec(a);
  int hasWhile = cond->whileBlock.type == XB_T_BLOCK;
  int walk = cond->record > 0 || cond->next > 0 || cond->rest || hasWhile || cond->current;
  int64_t next = cond->record > 0 ? 1 : cond->next > 0 ? cond->next : 0;
  int ok;
  if (cond->record > 0) ok = xb_area_goto(ctx, a, recno_of(cond->record));
  else if (cond->next > 0 || cond->rest || hasWhile)
    ok = cond->start <= 0 || xb_area_goto(ctx, a, recno_of(cond->start));
  else if (cond->current) ok = xb_area_edge_raw(ctx, a, 1) >= 0;
  else ok = xb_area_goto(ctx, a, 1); /* the blank record when there is none: it gives the key's type */
  if (!ok || a->closed || (!o->info.type && !key_shape(ctx, a, o))) return 0; /* a rebuild keeps the file's shape */
  uint32_t taken = 1; /* in record order, the records go on from the last taken, wherever the blocks move */
  for (int64_t step = cond->every; !a->eof;) {
    if (a->recno > last) { /* appended by the blocks while the build ran: not taken */
      if (!walk) break;
      if (xb_area_step_raw(ctx, a, 1) < 0 || a->closed) return 0;
      continue;
    }
    if (hasWhile) {
      int go = block_true(ctx, a, &cond->whileBlock);
      if (go <= 0) return go == 0;
    }
    if (!collect_record(ctx, a, o, c)) return 0;
    if (next > 0 && --next == 0) break;
    if (cond->evalBlock.type == XB_T_BLOCK) {
      if (step >= cond->every) {
        step = 0;
        int go = block_true(ctx, a, &cond->evalBlock);
        if (go <= 0) return go == 0;
      }
      ++step;
    }
    ok = walk ? xb_area_step_raw(ctx, a, 1) >= 0 : xb_area_goto(ctx, a, ++taken);
    if (!ok || a->closed) return 0;
  }
  return 1;
}

/* Builds the order's file from the records `cond` selects (collect()). */
static int build(xb_ctx *ctx, xb_area *a, xb_order *o, const xb_order_conditions *cond, int fresh) {
  collected c = {NULL, NULL, 0, 0};
  o->info.partial = cond->next > 0 || cond->record > 0 || cond->rest || cond->current;
  xb_area_pin(a);
  int ok = collect(ctx, a, o, cond, &c) && !a->closed;
  int32_t err = 0;
  if (ok && !write_sorted(a, o, &c, fresh, &err)) {
    xb_error_of(ctx, a->rdd->name, EG_CREATE, 1004, "", o->bag->path, err, 0, 0, NULL, 0, NULL);
    ok = 0;
  }
  xb_area_unpin(a);
  free(c.keys);
  free(c.recnos);
  return ok;
}

/* The area's bag of file `path`, NULL when none of its orders is in it. */
static xb_order_bag *bag_of(const xb_area *a, const char *path) {
  for (uint32_t i = 0; i < a->norders; ++i)
    if (strcmp(a->orders[i]->bag->path, path) == 0) return a->orders[i]->bag;
  return NULL;
}

/* Whether `path` names the table's production order file. */
static int production_path(const xb_area *a, const char *path) {
  return a->production && strcmp(a->production, path) == 0; /* as bag_of() tells files apart */
}

static xb_order_bag *bag_new(const xb_area *a, const char *path) {
  xb_order_bag *b = calloc(1, sizeof *b);
  if (!b) xb_gc_fatal("out of memory");
  b->path = malloc(strlen(path) + 1);
  if (!b->path) xb_gc_fatal("out of memory");
  strcpy(b->path, path);
  b->format = a->rdd->index;
  b->production = production_path(a, path);
  return b;
}

/* One order less in the bag: the file closes with the last. 0 after a close error (in `err`). */
static int bag_release(xb_order_bag *b, int32_t *err) {
  if (--b->orders) return 1;
  int ok = !b->bag || b->format->close(b->bag, err);
  free(b->path);
  free(b);
  return ok;
}

/* The key's type of an order of a file: its key's value on the current record; its key, condition and field. 0 after
 * the error (raised: Corruption detected when the key's type is none a key has). */
static int order_ready(xb_ctx *ctx, xb_area *a, xb_order *o) {
  xb_gc_root_push(ctx, &o->key);
  xb_gc_root_push(ctx, &o->cond);
  block_of(ctx, o->info.key, &o->key);
  if (o->info.cond[0]) block_of(ctx, o->info.cond, &o->cond);
  xb_item v;
  xb_set_nil(&v);
  xb_gc_root_push(ctx, &v);
  int ok = o->key.type == XB_T_BLOCK && (!o->info.cond[0] || o->cond.type == XB_T_BLOCK) &&
           eval_in(ctx, a, &o->key, &v);
  o->info.type = ok ? type_of(&v) : 0;
  xb_gc_root_pop(ctx, 3);
  if (!o->info.type) {
    if (ok) xb_error_of(ctx, a->rdd->name, EG_CORRUPTION, 1012, "", o->bag->path, 0, 0, 0, NULL, 0, NULL);
    return 0;
  }
  o->format->set_type(o->tag, o->info.type);
  o->field = key_field(a, o->info.key, o->info.keyLen);
  return 1;
}

xb_order *xb_order_create(xb_ctx *ctx, xb_area *a, const char *path, const char *name, int fresh,
                          const xb_str *keyText, const xb_item *keyBlock, int unique) {
  xb_order_bag *bag = bag_of(a, path);
  if (!bag) bag = bag_new(a, path);
  ++bag->orders;
  xb_order *o = order_new(bag, name ? order_name(name, strlen(name)) : file_order_name(path));
  size_t n = keyText->len < XB_INDEX_KEY_MAX ? keyText->len : XB_INDEX_KEY_MAX;
  memcpy(o->info.key, keyText->data, n);
  o->info.unique = unique;
  o->info.descend = a->pending.descend;
  if (a->pending.cond.type == XB_T_BLOCK) {
    o->cond = a->pending.cond;
    const xb_str *t = a->pending.condText.type == XB_T_STRING ? a->pending.condText.v.s : NULL;
    size_t m = t ? (t->len < XB_INDEX_KEY_MAX ? t->len : XB_INDEX_KEY_MAX) : 0;
    if (t) memcpy(o->info.cond, t->data, m);
  }
  if (keyBlock && keyBlock->type == XB_T_BLOCK) o->key = *keyBlock;
  else block_of(ctx, o->info.key, &o->key);
  xb_gc_root_push(ctx, &o->key);
  xb_gc_root_push(ctx, &o->cond);
  int ok = o->key.type == XB_T_BLOCK && build(ctx, a, o, &a->pending, fresh);
  xb_gc_root_pop(ctx, 2);
  if (!ok) {
    int32_t ignored;
    order_free(o, &ignored);
    return NULL;
  }
  return o;
}

uint32_t xb_order_open(xb_ctx *ctx, xb_area *a, const char *path, xb_order ***out) {
  const xb_index_format *format = a->rdd->index;
  xb_bag *file = NULL;
  for (uint16_t tries = 1;; ++tries) {
    int32_t err = 0;
    if (format->open(path, a->readonly, !a->shared, &file, &err)) break;
    if (xb_error_of(ctx, a->rdd->name, EG_OPEN, 1003, "", path, err, XB_ERR_RETRY | XB_ERR_DEFAULT, 0, NULL, tries,
                    NULL) != 2)
      return 0;
  }
  xb_order_bag *bag = bag_new(a, path);
  bag->bag = file;
  uint32_t n = format->tag_count(file), made = 0;
  if (!n) { /* a file of no order */
    int32_t err = 0;
    format->close(file, &err);
    free(bag->path);
    free(bag);
    return 0;
  }
  xb_order **list = malloc((n ? n : 1) * sizeof *list);
  if (!list) xb_gc_fatal("out of memory");
  int ok = 1;
  for (uint32_t i = 0; ok && i < n && !a->closed; ++i) {
    const char *name;
    xb_index_info info;
    xb_tag *tag = format->tag_at(file, i, &info, &name);
    ++bag->orders;
    xb_order *o = order_new(bag, name ? order_name(name, strlen(name)) : file_order_name(path));
    o->tag = tag;
    o->info = info;
    list[made++] = o;
    ok = order_ready(ctx, a, o);
  }
  if (!ok || a->closed) {
    int32_t ignored;
    for (uint32_t i = 0; i < made; ++i) order_free(list[i], &ignored);
    free(list);
    return 0;
  }
  *out = list;
  return made;
}

/* Builds an order again from the records: an untagged format's file made anew; a tagged one's file made anew with
 * its first order (`first`), the others added after it in their order. 0 (the order with no file) after the
 * error. */
static int rebuild(xb_ctx *ctx, xb_area *a, xb_order *o, const xb_order_conditions *cond, int first) {
  int32_t err = 0;
  if (o->format->tagged) return build(ctx, a, o, cond, first);
  int ok = !o->bag->bag || o->format->close(o->bag->bag, &err);
  o->bag->bag = NULL;
  o->tag = NULL;
  if (!ok) order_error(ctx, a, o, EG_WRITE, 1011, err);
  return ok && build(ctx, a, o, cond, 1);
}

/* Closes the area's orders of bag `b` from position `from` (from 0) on (out of the list first: handlers may close the
 * area). */
static void close_bag_orders(xb_ctx *ctx, xb_area *a, xb_order_bag *b, uint32_t from) {
  xb_order **gone = malloc((a->norders ? a->norders : 1) * sizeof *gone);
  if (!gone) xb_gc_fatal("out of memory");
  uint32_t kept = 0, n = 0;
  for (uint32_t i = 0; i < a->norders; ++i) {
    if (i < from || a->orders[i]->bag != b) {
      if (a->order == i + 1) a->order = kept + 1;
      a->orders[kept++] = a->orders[i];
    } else {
      if (a->order == i + 1) a->order = 0;
      gone[n++] = a->orders[i];
    }
  }
  a->norders = kept;
  for (uint32_t i = 0; i < n; ++i) xb_order_close(ctx, a, gone[i]);
  free(gone);
}

int xb_area_orders_rebuild(xb_ctx *ctx, xb_area *a, const xb_item *evalBlock, int64_t every) {
  xb_order_conditions cond;
  xb_order_conditions_clear(&cond);
  if (evalBlock) cond.evalBlock = *evalBlock;
  cond.every = every;
  xb_gc_root_push(ctx, &cond.evalBlock);
  xb_area_pin(a);
  int ok = 1;
  for (uint32_t k = 0; k < a->norders && !a->closed; ++k) {
    /* While its file is made again the order is out of the area's list (blocks that run meanwhile see it closed),
     * its items rooted here. */
    xb_order *o = a->orders[k];
    int first = 1;
    for (uint32_t j = 0; j < k && first; ++j) first = a->orders[j]->bag != o->bag;
    uint32_t focus = a->order;
    memmove(&a->orders[k], &a->orders[k + 1], (a->norders - k - 1) * sizeof *a->orders);
    --a->norders;
    a->order = focus == k + 1 ? 0 : focus > k + 1 ? focus - 1 : focus;
    xb_gc_root_push(ctx, &o->key);
    xb_gc_root_push(ctx, &o->cond);
    xb_gc_root_push(ctx, &o->scope[0].value);
    xb_gc_root_push(ctx, &o->scope[1].value);
    int built = rebuild(ctx, a, o, &cond, first);
    xb_gc_root_pop(ctx, 4);
    if (!built || a->closed) { /* not made again: closed; the ones after it stay as they were but in its file */
      ok = built;
      if (o->format->tagged && !a->closed) close_bag_orders(ctx, a, o->bag, k); /* not made again: their tags went */
      xb_order_close(ctx, a, o);
      break;
    }
    memmove(&a->orders[k + 1], &a->orders[k], (a->norders - k) * sizeof *a->orders);
    a->orders[k] = o;
    ++a->norders;
    a->order = focus;
  }
  ok = ok && !a->closed;
  xb_area_unpin(a);
  xb_gc_root_pop(ctx, 1);
  return ok;
}

int xb_order_close(xb_ctx *ctx, xb_area *a, xb_order *o) {
  int32_t err = 0;
  char *path = malloc(strlen(o->bag->path) + 1); /* for the error: the bag may go with the order */
  if (!path) xb_gc_fatal("out of memory");
  strcpy(path, o->bag->path);
  int ok = order_free(o, &err);
  if (!ok) xb_error_of(ctx, a->rdd->name, EG_WRITE, 1011, "", path, err, 0, 0, NULL, 0, NULL);
  free(path);
  return ok;
}

int xb_order_destroy(xb_ctx *ctx, xb_area *a, xb_order *o) {
  int32_t err = 0;
  xb_order_bag *b = o->bag;
  if (!o->format->remove(b->bag, o->tag, &err)) {
    order_error(ctx, a, o, EG_WRITE, 1011, err);
    xb_order_close(ctx, a, o);
    return 0;
  }
  int erase = !o->format->tagged || !o->format->tag_count(b->bag), production = b->production;
  char *path = malloc(strlen(b->path) + 1);
  if (!path) xb_gc_fatal("out of memory");
  strcpy(path, b->path);
  int ok = xb_order_close(ctx, a, o);
  if (ok && erase) {
    xb_platform_file_erase(path, &err); /* closed: a file another user still has open stays */
    if (production && !a->closed && !a->rdd->set_production(a, 0, &err)) {
      xb_area_error(ctx, a, EG_WRITE, 1011, "", err);
      ok = 0;
    }
  }
  free(path);
  return ok;
}

int xb_area_orders_clear(xb_ctx *ctx, xb_area *a, const xb_order_bag *keep) {
  int ok = xb_area_cold(ctx, a);
  if (a->closed) return 0;
  xb_order **gone = malloc((a->norders ? a->norders : 1) * sizeof *gone);
  if (!gone) xb_gc_fatal("out of memory");
  uint32_t kept = 0, n = 0;
  for (uint32_t i = 0; i < a->norders; ++i) {
    xb_order *o = a->orders[i];
    if (o->bag->production || o->bag == keep) a->orders[kept++] = o;
    else gone[n++] = o;
  }
  a->norders = kept;
  a->order = 0;
  for (uint32_t i = 0; i < n; ++i) ok = xb_order_close(ctx, a, gone[i]) && ok; /* out of the list first: */
  free(gone);                                                                  /* handlers may close the area */
  return ok;
}

uint32_t xb_area_add_order(xb_area *a, xb_order *o) {
  uint32_t at = o->bag->production ? 0 : a->norders;
  for (uint32_t i = 0; i < a->norders; ++i)
    if (a->orders[i]->bag == o->bag) at = i + 1;
  xb_order **grown = realloc(a->orders, (a->norders + 1) * sizeof *grown);
  if (!grown) xb_gc_fatal("out of memory");
  a->orders = grown;
  memmove(&a->orders[at + 1], &a->orders[at], (a->norders - at) * sizeof *a->orders);
  a->orders[at] = o;
  ++a->norders;
  if (a->order > at) ++a->order;
  return at + 1;
}

int xb_area_production_open(const xb_area *a) {
  for (uint32_t i = 0; i < a->norders; ++i)
    if (a->orders[i]->bag->production) return 1;
  return 0;
}

void xb_area_open_production(xb_ctx *ctx, xb_area *a) {
  xb_file_id id;
  int32_t err = 0;
  if (!a->production || !xb_platform_path_id(a->production, &id, &err)) return;
  xb_order **list = NULL;
  uint32_t n = xb_order_open(ctx, a, a->production, &list);
  for (uint32_t i = 0; i < n; ++i) {
    if (a->closed) xb_order_close(ctx, a, list[i]);
    else xb_area_add_order(a, list[i]);
  }
  free(list);
}

/* Puts the cursor on the current record's entry, or on the first after where it would be (its record in `found`):
 * 1, 0 when there is none after it, -1 after an error (raised). */
static int sync(xb_ctx *ctx, xb_area *a, xb_order *o, uint32_t *found) {
  *found = a->recno;
  int at = o->format->at(o->tag, a->recno);
  if (at && !a->shared) return 1;
  if (a->shared) a->rdd->refresh(a); /* the key as the record is now */
  unsigned char key[XB_INDEX_KEY_MAX];
  int in;
  if (!record_key(ctx, a, o, key, &in)) return -1;
  const unsigned char *k = at ? o->format->key(o->tag) : NULL; /* shared: another program may have changed the key */
  if (k && memcmp(k, key, o->info.keyLen) == 0) return 1;
  int32_t err = 0;
  int r = o->format->find(o->tag, key, a->recno, found, &err);
  if (r < 0) order_error(ctx, a, o, EG_READ, 1010, err);
  return r;
}

/* Where the entry the cursor is on stands against the order's scope: -1 before its top, 1 after its bottom, 0 in it.
 * A bound compares as many bytes as it has, in the order's order. */
static int scope_place(xb_order *o) {
  const unsigned char *k = o->format->key(o->tag);
  if (!k) return 0;
  if (o->scope[0].set && o->format->compare(&o->info, k, o->scope[0].key, o->scope[0].used) < 0) return -1;
  if (o->scope[1].set && o->format->compare(&o->info, k, o->scope[1].key, o->scope[1].used) > 0) return 1;
  return 0;
}

/* A bound's key from a value (a codeblock's: from what it returns); a value of another type than the key's leaves
 * the bound off. 0 after an error in the block (raised). */
static int scope_key(xb_ctx *ctx, xb_area *a, xb_order *o, xb_order_scope *sc) {
  xb_item r = sc->value;
  xb_gc_root_push(ctx, &r);
  int ok = r.type != XB_T_BLOCK || (eval_in(ctx, a, &sc->value, &r) && !a->closed);
  if (ok) sc->set = r.type != XB_T_NIL && xb_order_value_key(o, &r, sc->key, &sc->used); /* else `o` may be gone */
  xb_gc_root_pop(ctx, 1);
  return ok;
}

int xb_order_set_scope(xb_ctx *ctx, xb_area *a, xb_order *o, int bound, const xb_item *v) {
  xb_order_scope *sc = &o->scope[bound];
  xb_item before = sc->value;
  xb_gc_root_push(ctx, &before);
  sc->value = *v;
  int ok = scope_key(ctx, a, o, sc);
  xb_gc_root_pop(ctx, 1);
  if (!ok) {
    if (!a->closed) sc->value = before; /* the block failed: the bound stays as it was */
    return 0;
  }
  if (!sc->set && v->type != XB_T_BLOCK) xb_set_nil(&sc->value); /* not a value of the key's type: no bound */
  return 1;
}

/* Bounds given as codeblocks are evaluated again before each navigation. 0 after an error (raised). */
static int refresh_scope(xb_ctx *ctx, xb_area *a, xb_order *o) {
  for (int k = 0; k < 2; ++k)
    if (o->scope[k].value.type == XB_T_BLOCK && !scope_key(ctx, a, o, &o->scope[k])) return 0;
  return 1;
}

/* A bound for the format's searches from a scope bound: before its first match (`past`: after its last). */
static const xb_index_bound *scope_bound(const xb_order_scope *sc, int past, xb_index_bound *b) {
  if (!sc->set) return NULL;
  b->key = sc->key;
  b->len = sc->used;
  b->recno = 0;
  b->whole = 0;
  b->past = past;
  return b;
}

/* xb_order_edge with the bounds as they are. */
static int edge(xb_ctx *ctx, xb_area *a, xb_order *o, int dir, uint32_t *recno) {
  int32_t err = 0;
  const xb_order_scope *sc = &o->scope[dir > 0 ? 0 : 1];
  int r = sc->set ? o->format->seek(o->tag, sc->key, sc->used, dir < 0, recno, &err)
          : dir > 0 ? o->format->first(o->tag, recno, &err)
                    : o->format->last(o->tag, recno, &err);
  if (r < 0) order_error(ctx, a, o, EG_READ, 1010, err);
  return r > 0 && scope_place(o) ? 0 : r;
}

int xb_order_edge(xb_ctx *ctx, xb_area *a, xb_order *o, int dir, uint32_t *recno) {
  return refresh_scope(ctx, a, o) ? edge(ctx, a, o, dir, recno) : -1;
}

/* A step that came to an entry outside the scope: from before it, its first (last) entry; past it, none. */
static int in_scope(xb_ctx *ctx, xb_area *a, xb_order *o, int dir, uint32_t *recno) {
  int place = scope_place(o);
  if (!place) return 1;
  return place == -dir ? edge(ctx, a, o, dir, recno) : 0;
}

int xb_order_step(xb_ctx *ctx, xb_area *a, xb_order *o, int dir, uint32_t *recno) {
  if (!refresh_scope(ctx, a, o)) return -1;
  int32_t err = 0;
  if (a->eof) return dir < 0 ? edge(ctx, a, o, -1, recno) : 0;
  uint32_t found;
  int r = sync(ctx, a, o, &found);
  if (r < 0) return -1;
  if (r == 0) return dir < 0 ? edge(ctx, a, o, -1, recno) : 0; /* after every entry */
  if (found != a->recno && dir > 0) { /* the record has no entry: the cursor is on the one after it */
    *recno = found;
    return in_scope(ctx, a, o, dir, recno);
  }
  r = dir > 0 ? o->format->next(o->tag, recno, &err) : o->format->prev(o->tag, recno, &err);
  if (r < 0) order_error(ctx, a, o, EG_READ, 1010, err);
  return r > 0 ? in_scope(ctx, a, o, dir, recno) : r;
}

int xb_order_step_raw(xb_ctx *ctx, xb_area *a, xb_order *o, int dir, uint32_t *recno) {
  if (!a->eof && !o->format->at(o->tag, a->recno)) {
    uint32_t found;
    int r = sync(ctx, a, o, &found);
    if (r <= 0 || found != a->recno) return r < 0 ? -1 : 0;
  }
  return xb_order_step(ctx, a, o, dir, recno);
}

int xb_order_seek(xb_ctx *ctx, xb_area *a, xb_order *o, const unsigned char *key, size_t used, int last,
                  uint32_t *recno, int *match) {
  *match = 0;
  if (!refresh_scope(ctx, a, o)) return -1;
  int32_t err = 0;
  int r = o->format->seek(o->tag, key, used, last, recno, &err);
  if (r < 0) order_error(ctx, a, o, EG_READ, 1010, err);
  int place = r > 0 ? scope_place(o) : 0;
  if (place) return place < 0 ? edge(ctx, a, o, 1, recno) : 0; /* before the scope: its first entry; past it: none */
  *match = r > 0 && xb_order_cursor_matches(o, key, used);
  return r;
}

int xb_order_cursor_matches(xb_order *o, const unsigned char *key, size_t used) {
  const unsigned char *k = o->format->key(o->tag);
  return k && memcmp(k, key, used) == 0;
}

void xb_order_mark(xb_order *o, int (*mark)(xb_item *)) {
  mark(&o->key);
  mark(&o->cond);
  mark(&o->scope[0].value);
  mark(&o->scope[1].value);
}

int xb_order_count(xb_ctx *ctx, xb_area *a, xb_order *o, uint64_t *n) {
  *n = 0;
  if (!refresh_scope(ctx, a, o)) return 0;
  xb_index_bound top, bottom;
  int32_t err = 0;
  if (o->format->count(o->tag, scope_bound(&o->scope[0], 0, &top), scope_bound(&o->scope[1], 1, &bottom), n, &err))
    return 1;
  order_error(ctx, a, o, EG_READ, 1010, err);
  return 0;
}

int xb_order_position(xb_ctx *ctx, xb_area *a, xb_order *o, uint64_t *n) {
  *n = 0;
  if (a->eof) return 1;
  if (!refresh_scope(ctx, a, o)) return 0;
  uint32_t found;
  int r = sync(ctx, a, o, &found);
  if (r <= 0 || found != a->recno || scope_place(o)) return r >= 0; /* no entry, or outside the scope */
  xb_index_bound top;
  int32_t err = 0;
  if (!o->format->count_before(o->tag, scope_bound(&o->scope[0], 0, &top), n, &err)) {
    order_error(ctx, a, o, EG_READ, 1010, err);
    return 0;
  }
  ++*n; /* the entries before it, and it */
  return 1;
}

int xb_order_nth(xb_ctx *ctx, xb_area *a, xb_order *o, uint64_t n, uint32_t *recno) {
  *recno = 0;
  if (!n) return 1;
  if (!refresh_scope(ctx, a, o)) return 0;
  xb_index_bound top;
  int32_t err = 0;
  int r = o->format->select(o->tag, scope_bound(&o->scope[0], 0, &top), n - 1, recno, &err);
  if (r < 0) order_error(ctx, a, o, EG_READ, 1010, err);
  if (r <= 0 || scope_place(o)) *recno = 0; /* past the scope's bottom */
  return r >= 0;
}

int xb_order_skip_unique(xb_ctx *ctx, xb_area *a, xb_order *o, int dir) {
  if (!refresh_scope(ctx, a, o)) return 0;
  int32_t err = 0;
  uint32_t recno = 0, found = 0;
  int r = a->eof ? 0 : sync(ctx, a, o, &found), out = 0;
  if (r < 0) return 0;
  if (r > 0 && found == a->recno) { /* on the record's entry: past the entries of its key */
    unsigned char key[XB_INDEX_KEY_MAX];
    const unsigned char *k = o->format->key(o->tag);
    if (k) memcpy(key, k, o->info.keyLen);
    while (k) {
      r = dir > 0 ? o->format->next(o->tag, &recno, &err) : o->format->prev(o->tag, &recno, &err);
      if (r > 0 && scope_place(o)) r = 0;
      k = r > 0 ? o->format->key(o->tag) : NULL;
      if (k && memcmp(k, key, o->info.keyLen) != 0) break;
    }
    if (r > 0 && !k) { /* the page under the cursor could not be read */
      r = -1;
      err = XB_FERR_FAILURE;
    }
    if (r < 0) {
      order_error(ctx, a, o, EG_READ, 1010, err);
      return 0;
    }
    out = r == 0;
  } else if (dir < 0 && a->eof) { /* back from the end: the last entry */
    r = edge(ctx, a, o, -1, &recno);
  } else {
    out = 1;
  }
  if (out) r = dir > 0 ? 0 : edge(ctx, a, o, 1, &recno); /* none: the end, or the first entry with BOF */
  if (r < 0 || a->closed || !xb_area_arrive(ctx, a, r > 0 ? recno : 0, dir > 0 || out ? 1 : -1)) return 0;
  if (dir < 0 && out && r > 0) a->bof = 1;
  if (dir > 0) a->bof = 0;
  return 1;
}

int xb_order_record_key(xb_ctx *ctx, xb_area *a, xb_order *o, unsigned char *key, int *in) {
  return record_key(ctx, a, o, key, in);
}

void xb_order_key_value(const xb_order *o, const unsigned char *key, uint16_t dec, xb_item *r) {
  o->format->key_value(&o->info, key, dec, r);
}

/* The orders' entries for a record that changes: the keys before (`fresh`: a new record, which has none), then,
 * once it is written, the keys after; an entry moves only when its key (or the condition) changed. */
int xb_area_hot(xb_ctx *ctx, xb_area *a, int fresh) {
  if (!a->norders || a->hot) return 1;
  xb_area_pin(a);
  int ok = 1;
  for (uint32_t k = 0; ok && k < a->norders && !a->closed; ++k) {
    xb_order *o = a->orders[k];
    o->oldIn = 0;
    if (!fresh) ok = record_key(ctx, a, o, o->old, &o->oldIn);
  }
  ok = ok && !a->closed;
  if (ok) {
    a->hot = 1;
    a->hotRecno = a->recno;
  }
  xb_area_unpin(a);
  return ok;
}

/* Whether order `o`'s entry for the record changed (its key, or whether it has one). */
static int entry_changed(const xb_order *o) {
  return o->oldIn != o->nowIn || (o->nowIn && memcmp(o->old, o->now, o->info.keyLen) != 0);
}

/* The orders of bag `b`, from the k-th of the area on: their entries move, under one hold of the bag's lock. */
static int apply_bag(xb_ctx *ctx, xb_area *a, uint32_t k, uint32_t evaluated, xb_order_bag *b) {
  int32_t err = 0;
  if (!b->format->hold(b->bag, 1, &err)) {
    order_error(ctx, a, a->orders[k], EG_WRITE, 1011, err);
    return 0;
  }
  xb_order *failed = NULL; /* the error is raised once the lock is let go: its handler may close the area */
  int32_t failure = 0;
  for (uint32_t j = k; !failed && j < evaluated; ++j) {
    xb_order *o = a->orders[j];
    if (o->bag != b || !entry_changed(o)) continue;
    if (!o->format->replace(o->tag, o->oldIn ? o->old : NULL, o->nowIn ? o->now : NULL, a->recno, &err)) {
      failed = o;
      failure = err;
    }
  }
  if (!b->format->let_go(b->bag, &err) && !failed) {
    failed = a->orders[k];
    failure = err;
  }
  if (failed) order_error(ctx, a, failed, EG_WRITE, 1011, failure);
  return !failed;
}

/* The keys after the change first (blocks run: no lock is held), then each bag's entries that changed, the bags in
 * the order of their first orders in the area's list. */
int xb_area_cold(xb_ctx *ctx, xb_area *a) {
  if (!a->hot) return 1;
  a->hot = 0;
  if (a->hotRecno != a->recno) return 1; /* the record of the change is not current: nothing to compare */
  xb_area_pin(a);
  uint32_t evaluated = 0; /* the orders whose new key is known: their entries move even when a later key failed */
  while (evaluated < a->norders && !a->closed && record_key(ctx, a, a->orders[evaluated], a->orders[evaluated]->now,
                                                            &a->orders[evaluated]->nowIn))
    ++evaluated;
  int ok = evaluated == a->norders;
  for (uint32_t k = 0; !a->closed && k < evaluated; ++k) {
    xb_order_bag *b = a->orders[k]->bag;
    int first = 1, changed = 0;
    for (uint32_t j = 0; j < k && first; ++j) first = a->orders[j]->bag != b;
    for (uint32_t j = k; first && j < evaluated && !changed; ++j) changed = a->orders[j]->bag == b &&
                                                                            entry_changed(a->orders[j]);
    if (changed && !apply_bag(ctx, a, k, evaluated, b)) { /* its handler may have changed the orders: no more */
      ok = 0;
      break;
    }
  }
  ok = ok && !a->closed;
  xb_area_unpin(a);
  return ok;
}

int xb_area_orders_flush(xb_ctx *ctx, xb_area *a) {
  for (uint32_t k = 0; k < a->norders; ++k) {
    int32_t err = 0;
    const xb_order *o = a->orders[k];
    if (o->bag->bag && !o->format->flush(o->bag->bag, &err)) {
      order_error(ctx, a, a->orders[k], EG_WRITE, 1011, err);
      return 0;
    }
  }
  return 1;
}
