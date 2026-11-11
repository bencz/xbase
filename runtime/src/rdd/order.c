/* Orders of a work area (rdd/order.h). Keys are made as Clipper's NTX orders make them: a string's bytes (padded
 * with blanks or cut to the key's length), a number's Str( n, len, dec ) with its blanks and sign as '0' and, for a
 * negative number, each digit d as 0x5C - d (so that negative numbers come first, the largest first), a date's
 * DToS() (blanks for the empty date), a logical's T or F. */
#include "rdd/order.h"

#include "core/file.h"
#include "core/numeric.h"

#include <stdlib.h>
#include <string.h>

#define NUMBER_FLIP 0x5C /* a negative number's digit d is stored as 0x5C - d */

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

/* A number's key: Str() in `len` bytes with `dec` decimals, then as the top of the file says. */
static void number_key(const xb_item *v, uint32_t len, uint32_t dec, unsigned char *out) {
  char *s = (char *)out;
  xb_num_to_field_buf(v, (int)len, (int)dec, s);
  int neg = memchr(s, '-', len) != NULL;
  for (uint32_t i = 0; i < len; ++i) {
    if (s[i] == ' ' || s[i] == '-') s[i] = '0';
    if (neg && s[i] >= '0' && s[i] <= '9') s[i] = (char)(NUMBER_FLIP - s[i]);
  }
}

int xb_order_value_key(const xb_order *o, const xb_item *v, unsigned char *out, size_t *used) {
  uint32_t len = o->info.keyLen;
  if (type_of(v) != o->type) return 0;
  *used = len;
  switch (o->type) {
  case 'C': {
    size_t n = v->v.s->len < len ? v->v.s->len : len;
    memcpy(out, v->v.s->data, n);
    memset(out + n, ' ', len - n);
    *used = n;
    return 1;
  }
  case 'N': number_key(v, len, o->info.keyDec, out); return 1;
  case 'D': {
    char digits[XB_DATE_DIGITS_SIZE];
    xb_date_to_digits(v->v.i, digits);
    memcpy(out, digits, XB_DATE_DIGITS_SIZE - 1);
    return 1;
  }
  default: out[0] = v->v.l ? 'T' : 'F'; return 1;
  }
}

/* A key of a type an order cannot have. */
static void key_type_error(xb_ctx *ctx, xb_area *a) {
  xb_error_of(ctx, a->rdd->name, EG_DATATYPE, 1026, "", NULL, 0, 0, 0, NULL, 0, NULL);
}

/* Evaluates `block` with area `a` current: 1 with its value in `r` (rooted by the caller), 0 after an error. While
 * it runs, the area's orders may not change (`keying`; ordcmd.c refuses it). */
static int eval_in(xb_ctx *ctx, xb_area *a, const xb_item *block, xb_item *r) {
  uint32_t before = ctx->areaCurrent;
  ctx->areaCurrent = a->number;
  xb_item b = *block;
  xb_gc_root_push(ctx, &b);
  ++a->keying;
  xb_block_call(ctx, r, &b, 0, r);
  --a->keying;
  xb_gc_root_pop(ctx, 1);
  if (ctx->areaCurrent == a->number) ctx->areaCurrent = before;
  return !(ctx->pending && xb_pending(ctx));
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
  xb_error_of(ctx, a->rdd->name, gencode, subcode, "", o->bag, err, 0, 0, NULL, 0, NULL);
}

/* An order's name: its file's name without directories and extension, upper case, at most XB_ORDER_NAME_MAX
 * characters (malloc'ed). */
static char *name_of(const char *path) {
  size_t start, ext, len = strlen(path);
  xb_file_name_parts(path, len, &start, &ext);
  if (ext - start > XB_ORDER_NAME_MAX) ext = start + XB_ORDER_NAME_MAX;
  char *n = malloc(ext - start + 1);
  if (!n) xb_gc_fatal("out of memory");
  for (size_t i = start; i < ext; ++i) n[i - start] = xb_ascii_upper(path[i]);
  n[ext - start] = '\0';
  return n;
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

static xb_order *order_new(const char *path) {
  xb_order *o = calloc(1, sizeof *o);
  if (!o) xb_gc_fatal("out of memory");
  o->bag = malloc(strlen(path) + 1);
  if (!o->bag) xb_gc_fatal("out of memory");
  strcpy(o->bag, path);
  o->name = name_of(path);
  xb_set_nil(&o->key);
  xb_set_nil(&o->cond);
  o->field = -1;
  return o;
}

static void order_free(xb_order *o) {
  free(o->bag);
  free(o->name);
  free(o);
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

/* Whether entry a comes before entry b: by key, then (collected in record order) by record number. */
static int sorts_before(const sorting *a, const sorting *b, const collected *c, const xb_ntx_info *info) {
  if (a->prefix != b->prefix) return a->prefix < b->prefix;
  uint32_t len = info->keyLen;
  if (len > 8) {
    int d = xb_ntx_compare(info, c->keys + (size_t)a->at * len + 8, c->keys + (size_t)b->at * len + 8, len - 8);
    if (d) return d < 0;
  }
  return a->at < b->at;
}

#define SORT_RUN 16 /* entries sorted by insertion before the merges */

/* Sorts the entries: runs by insertion, then merges of runs twice as long each pass. */
static void sort_entries(sorting *v, uint32_t n, const collected *c, const xb_ntx_info *info) {
  for (uint32_t lo = 0; lo < n; lo += SORT_RUN) {
    uint32_t hi = lo + SORT_RUN < n ? lo + SORT_RUN : n;
    for (uint32_t i = lo + 1; i < hi; ++i) {
      sorting e = v[i];
      uint32_t j = i;
      for (; j > lo && sorts_before(&e, &v[j - 1], c, info); --j) v[j] = v[j - 1];
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
      while (i < mid && j < hi) dst[k++] = sorts_before(&src[j], &src[i], c, info) ? src[j++] : src[i++];
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

/* Writes the collected entries, sorted (unique: the first of equal keys), as the order's file. */
static int write_sorted(const char *path, const xb_ntx_info *info, collected *c, int32_t *err) {
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
  sort_entries(order, c->n, c, info);
  uint32_t n = 0;
  for (uint32_t i = 0; i < c->n; ++i) {
    const unsigned char *k = c->keys + (size_t)order[i].at * len;
    if (info->unique && n > 0 && memcmp(keys + (size_t)(n - 1) * len, k, len) == 0) continue;
    memcpy(keys + (size_t)n * len, k, len);
    recnos[n++] = c->recnos[order[i].at];
  }
  int ok = xb_ntx_create(path, info, keys, recnos, n, err);
  free(order);
  free(keys);
  free(recnos);
  return ok;
}

/* Collects the key of every record (in record order; every record, deleted or filtered or not) that meets the
 * order's condition; the key's type and length come from the first record's (the blank record's when there is
 * none) when `o->type` is 0. 0 after an error (raised), or when the area closed. */
static int collect(xb_ctx *ctx, xb_area *a, xb_order *o, collected *c) {
  uint32_t last = a->rdd->lastrec(a);
  unsigned char key[XB_NTX_KEY_MAX];
  for (uint32_t r = last ? 1 : 0; r <= last; ++r) {
    if (!xb_area_goto(ctx, a, r) || a->closed) return 0;
    if (!o->type) { /* the first record: the key's type and length */
      xb_item v;
      xb_set_nil(&v);
      xb_gc_root_push(ctx, &v);
      int ok = eval_in(ctx, a, &o->key, &v) && !a->closed;
      o->type = ok ? type_of(&v) : 0;
      if (ok && !o->type) key_type_error(ctx, a);
      if (o->type == 'C') o->info.keyLen = v.v.s->len < XB_NTX_KEY_MAX ? (uint32_t)v.v.s->len : XB_NTX_KEY_MAX;
      if (o->type == 'N') {
        int width = v.width ? v.width : v.type == XB_T_INT ? xb_num_int_width(v.v.i) : xb_num_dbl_width(v.v.d);
        o->info.keyDec = v.dec;
        o->info.keyLen = (uint32_t)width + (v.dec ? v.dec + 1u : 0u);
      }
      if (o->type == 'D') o->info.keyLen = 8;
      if (o->type == 'L') o->info.keyLen = 1;
      xb_gc_root_pop(ctx, 1);
      if (!o->type) return 0;
      if (o->info.keyLen == 0 || o->info.keyLen > XB_NTX_KEY_MAX) {
        xb_error_of(ctx, a->rdd->name, EG_DATAWIDTH, 1021, "", o->bag, 0, 0, 0, NULL, 0, NULL);
        return 0;
      }
      o->field = key_field(a, o->info.key, o->info.keyLen);
    }
    if (r == 0) break; /* no record: the blank one gave the type */
    if (!c->keys) {
      if ((uint64_t)last * (2u * o->info.keyLen + sizeof(uint32_t) * 2 + sizeof(sorting)) > SIZE_MAX)
        xb_gc_fatal("out of memory"); /* the keys, sorted and not, and their records: more than an address reaches */
      c->cap = last;
      c->keys = malloc((size_t)c->cap * o->info.keyLen);
      c->recnos = malloc((size_t)c->cap * sizeof *c->recnos);
      if (!c->keys || !c->recnos) xb_gc_fatal("out of memory");
    }
    int in;
    if (!record_key(ctx, a, o, key, &in)) return 0;
    if (!in) continue;
    memcpy(c->keys + (size_t)c->n * o->info.keyLen, key, o->info.keyLen);
    c->recnos[c->n++] = r;
  }
  return 1;
}

/* Builds the order's file from the records; the order is opened after. */
static int build(xb_ctx *ctx, xb_area *a, xb_order *o) {
  collected c = {NULL, NULL, 0, 0};
  xb_area_pin(a);
  int ok = collect(ctx, a, o, &c) && !a->closed;
  int32_t err = 0;
  if (ok && !write_sorted(o->bag, &o->info, &c, &err)) {
    xb_error_of(ctx, a->rdd->name, EG_CREATE, 1004, "", o->bag, err, 0, 0, NULL, 0, NULL);
    ok = 0;
  }
  xb_area_unpin(a);
  free(c.keys);
  free(c.recnos);
  return ok;
}

/* Opens the order's file; a file that cannot be is an Open error, which may be retried. */
static int open_file(xb_ctx *ctx, xb_area *a, xb_order *o) {
  xb_ntx_info info;
  for (uint16_t tries = 1;; ++tries) {
    int32_t err = 0;
    if (xb_ntx_open(o->bag, a->readonly, &o->ntx, &info, &err)) break;
    if (xb_error_of(ctx, a->rdd->name, EG_OPEN, 1003, "", o->bag, err, XB_ERR_RETRY | XB_ERR_DEFAULT, 0, NULL, tries,
                    NULL) != 2)
      return 0;
  }
  o->info = info;
  return 1;
}

xb_order *xb_order_create(xb_ctx *ctx, xb_area *a, const char *path, const xb_str *keyText, const xb_item *keyBlock,
                          int unique) {
  xb_order *o = order_new(path);
  size_t n = keyText->len < XB_NTX_KEY_MAX ? keyText->len : XB_NTX_KEY_MAX;
  memcpy(o->info.key, keyText->data, n);
  o->info.unique = unique;
  o->info.descend = a->pendingDescend;
  if (a->pendingCond.type == XB_T_BLOCK) {
    o->cond = a->pendingCond;
    const xb_str *t = a->pendingCondText.type == XB_T_STRING ? a->pendingCondText.v.s : NULL;
    size_t m = t ? (t->len < XB_NTX_KEY_MAX ? t->len : XB_NTX_KEY_MAX) : 0;
    if (t) memcpy(o->info.cond, t->data, m);
  }
  if (keyBlock && keyBlock->type == XB_T_BLOCK) o->key = *keyBlock;
  else block_of(ctx, o->info.key, &o->key);
  xb_gc_root_push(ctx, &o->key);
  xb_gc_root_push(ctx, &o->cond);
  int ok = o->key.type == XB_T_BLOCK && build(ctx, a, o) && open_file(ctx, a, o);
  xb_gc_root_pop(ctx, 2);
  if (!ok) {
    if (o->ntx) {
      int32_t ignored;
      xb_ntx_close(o->ntx, &ignored);
    }
    order_free(o);
    return NULL;
  }
  return o;
}

xb_order *xb_order_open(xb_ctx *ctx, xb_area *a, const char *path) {
  xb_order *o = order_new(path);
  if (!open_file(ctx, a, o)) {
    order_free(o);
    return NULL;
  }
  xb_gc_root_push(ctx, &o->key);
  xb_gc_root_push(ctx, &o->cond);
  block_of(ctx, o->info.key, &o->key);
  if (o->info.cond[0]) block_of(ctx, o->info.cond, &o->cond);
  xb_item v; /* the key's type: the current record's key */
  xb_set_nil(&v);
  xb_gc_root_push(ctx, &v);
  int ok = o->key.type == XB_T_BLOCK && (!o->info.cond[0] || o->cond.type == XB_T_BLOCK) &&
           eval_in(ctx, a, &o->key, &v);
  o->type = ok ? type_of(&v) : 0;
  xb_gc_root_pop(ctx, 3);
  if (!o->type) {
    if (ok) xb_error_of(ctx, a->rdd->name, EG_CORRUPTION, 1012, "", o->bag, 0, 0, 0, NULL, 0, NULL);
    int32_t ignored;
    xb_ntx_close(o->ntx, &ignored);
    order_free(o);
    return NULL;
  }
  o->field = key_field(a, o->info.key, o->info.keyLen);
  return o;
}

/* Builds an order's file again from the records; 0 (the order with no file) after the error. */
static int rebuild(xb_ctx *ctx, xb_area *a, xb_order *o) {
  int32_t err = 0;
  int ok = xb_ntx_close(o->ntx, &err);
  o->ntx = NULL;
  if (!ok) order_error(ctx, a, o, EG_WRITE, 1011, err);
  return ok && build(ctx, a, o) && open_file(ctx, a, o);
}

int xb_area_orders_rebuild(xb_ctx *ctx, xb_area *a) {
  xb_area_pin(a);
  int ok = 1;
  for (uint32_t k = 0; k < a->norders && !a->closed;) {
    xb_order *o = a->orders[k];
    if (rebuild(ctx, a, o) || a->closed) {
      ++k;
      continue;
    }
    ok = 0; /* an order whose file could not be made again is closed; the ones after it stay as they were */
    memmove(&a->orders[k], &a->orders[k + 1], (a->norders - k - 1) * sizeof *a->orders);
    --a->norders;
    if (a->order == k + 1) a->order = 0;
    else if (a->order > k + 1) --a->order;
    xb_order_close(ctx, a, o);
    break;
  }
  ok = ok && !a->closed;
  xb_area_unpin(a);
  return ok;
}

int xb_order_close(xb_ctx *ctx, xb_area *a, xb_order *o) {
  int32_t err = 0;
  int ok = !o->ntx || xb_ntx_close(o->ntx, &err);
  if (!ok) order_error(ctx, a, o, EG_WRITE, 1011, err);
  order_free(o);
  return ok;
}

/* Puts the cursor on the current record's entry, or on the first after where it would be (its record in `found`):
 * 1, 0 when there is none after it, -1 after an error (raised). */
static int sync(xb_ctx *ctx, xb_area *a, xb_order *o, uint32_t *found) {
  *found = a->recno;
  if (xb_ntx_at(o->ntx, a->recno)) return 1;
  unsigned char key[XB_NTX_KEY_MAX];
  int in;
  if (!record_key(ctx, a, o, key, &in)) return -1;
  int32_t err = 0;
  int r = xb_ntx_find(o->ntx, key, a->recno, found, &err);
  if (r < 0) order_error(ctx, a, o, EG_READ, 1010, err);
  return r;
}

int xb_order_edge(xb_ctx *ctx, xb_area *a, xb_order *o, int dir, uint32_t *recno) {
  int32_t err = 0;
  int r = dir > 0 ? xb_ntx_first(o->ntx, recno, &err) : xb_ntx_last(o->ntx, recno, &err);
  if (r < 0) order_error(ctx, a, o, EG_READ, 1010, err);
  return r;
}

int xb_order_step(xb_ctx *ctx, xb_area *a, xb_order *o, int dir, uint32_t *recno) {
  int32_t err = 0;
  if (a->eof) return dir < 0 ? xb_order_edge(ctx, a, o, -1, recno) : 0;
  uint32_t found;
  int r = sync(ctx, a, o, &found);
  if (r < 0) return -1;
  if (r == 0) return dir < 0 ? xb_order_edge(ctx, a, o, -1, recno) : 0; /* after every entry */
  if (found != a->recno && dir > 0) { /* the record has no entry: the cursor is on the one after it */
    *recno = found;
    return 1;
  }
  r = dir > 0 ? xb_ntx_next(o->ntx, recno, &err) : xb_ntx_prev(o->ntx, recno, &err);
  if (r < 0) order_error(ctx, a, o, EG_READ, 1010, err);
  return r;
}

int xb_order_seek(xb_ctx *ctx, xb_area *a, xb_order *o, const unsigned char *key, size_t used, int last,
                  uint32_t *recno, int *match) {
  int32_t err = 0;
  int r = xb_ntx_seek(o->ntx, key, used, last, recno, &err);
  if (r < 0) order_error(ctx, a, o, EG_READ, 1010, err);
  *match = r > 0 && xb_order_cursor_matches(o, key, used);
  return r;
}

int xb_order_cursor_matches(xb_order *o, const unsigned char *key, size_t used) {
  const unsigned char *k = xb_ntx_key(o->ntx);
  return k && memcmp(k, key, used) == 0;
}

void xb_order_mark(xb_order *o, int (*mark)(const xb_item *)) {
  mark(&o->key);
  mark(&o->cond);
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

int xb_area_cold(xb_ctx *ctx, xb_area *a) {
  if (!a->hot) return 1;
  a->hot = 0;
  if (a->hotRecno != a->recno) return 1; /* the record of the change is not current: nothing to compare */
  xb_area_pin(a);
  int ok = 1;
  unsigned char key[XB_NTX_KEY_MAX];
  for (uint32_t k = 0; ok && k < a->norders && !a->closed; ++k) {
    xb_order *o = a->orders[k];
    int in;
    if (!record_key(ctx, a, o, key, &in) || a->closed) {
      ok = 0;
      break;
    }
    int same = o->oldIn == in && (!in || memcmp(o->old, key, o->info.keyLen) == 0);
    if (same) continue;
    int32_t err = 0;
    if ((o->oldIn && !xb_ntx_delete(o->ntx, o->old, a->recno, &err)) ||
        (in && !xb_ntx_insert(o->ntx, key, a->recno, &err))) {
      order_error(ctx, a, o, EG_WRITE, 1011, err);
      ok = 0;
    }
  }
  ok = ok && !a->closed;
  xb_area_unpin(a);
  return ok;
}

int xb_area_orders_flush(xb_ctx *ctx, xb_area *a) {
  for (uint32_t k = 0; k < a->norders; ++k) {
    int32_t err = 0;
    if (!xb_ntx_flush(a->orders[k]->ntx, &err)) {
      order_error(ctx, a, a->orders[k], EG_WRITE, 1011, err);
      return 0;
    }
  }
  return 1;
}
