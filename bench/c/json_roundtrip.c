/* bench/json_roundtrip.prg in C: 20,000 records as hash tables that keep their keys in insertion order (an id, a name
 * with quotes, backslashes, tabs and line feeds, an array of tags, a logical, a number, a null, a nested hash) made
 * from a fixed seed, then timed: serialized to text, parsed back into hash tables and arrays by a recursive descent
 * parser, and serialized again. The two texts must be equal, and the values read back add up as made. The text is
 * appended to one growing buffer; each string is quoted by four replacements in a row, each making a new string as
 * StrTran() does. */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { T_NIL, T_NUM, T_LOG, T_STR, T_ARR, T_HASH } Type;

typedef struct Value Value;

typedef struct {
  char *key;
  Value *value;
} Entry;

struct Value {
  Type type;
  int64_t num; /* a number, or a logical's 0 or 1 */
  char *str;
  Value **items; /* an array's elements */
  Entry *entries; /* a hash's entries, in insertion order */
  int64_t count, cap;
  int64_t *index; /* a hash's slots: entry number plus 1, 0 for a free one */
  int64_t slots;
};

typedef struct {
  char *data;
  size_t len, cap;
} Buf;

static int64_t seed = 1618;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static void put(Buf *b, const char *s, size_t n) {
  if (b->len + n + 1 > b->cap) {
    while (b->len + n + 1 > b->cap) b->cap = b->cap ? b->cap * 2 : 64;
    b->data = realloc(b->data, b->cap);
  }
  memcpy(b->data + b->len, s, n);
  b->len += n;
  b->data[b->len] = '\0';
}

static void puts_(Buf *b, const char *s) { put(b, s, strlen(s)); }

static char *substr(const char *s, size_t n) {
  char *r = malloc(n + 1);
  memcpy(r, s, n);
  r[n] = '\0';
  return r;
}

static Value *new_value(Type type) {
  Value *v = calloc(1, sizeof *v);
  v->type = type;
  return v;
}

static Value *new_num(int64_t n) {
  Value *v = new_value(T_NUM);
  v->num = n;
  return v;
}

static Value *new_str(char *s) {
  Value *v = new_value(T_STR);
  v->str = s;
  return v;
}

static void array_add(Value *a, Value *x) {
  if (a->count == a->cap) {
    a->cap = a->cap ? a->cap * 2 : 4;
    a->items = realloc(a->items, (size_t)a->cap * sizeof *a->items);
  }
  a->items[a->count++] = x;
}

static uint64_t hash_str(const char *s) {
  uint64_t h = 14695981039346656037ULL;
  for (; *s; ++s) h = (h ^ (unsigned char)*s) * 1099511628211ULL;
  return h;
}

/* The slot of key in h's index: one that holds it, or the free one where it goes. */
static int64_t *slot_of(const Value *h, const char *key) {
  uint64_t i = hash_str(key) & (uint64_t)(h->slots - 1);
  while (h->index[i] && strcmp(h->entries[h->index[i] - 1].key, key) != 0) i = (i + 1) & (uint64_t)(h->slots - 1);
  return &h->index[i];
}

static void free_value(Value *v);

/* h[key] := x; the hash takes key and x. */
static void hash_set(Value *h, char *key, Value *x) {
  if ((h->count + 1) * 4 > h->slots * 3) {
    free(h->index);
    h->slots = h->slots ? h->slots * 2 : 8;
    h->index = calloc((size_t)h->slots, sizeof *h->index);
    for (int64_t e = 0; e < h->count; ++e) *slot_of(h, h->entries[e].key) = e + 1;
  }
  int64_t *s = slot_of(h, key);
  if (*s) {
    Entry *e = &h->entries[*s - 1];
    free(key);
    free_value(e->value);
    e->value = x;
    return;
  }
  if (h->count == h->cap) {
    h->cap = h->cap ? h->cap * 2 : 8;
    h->entries = realloc(h->entries, (size_t)h->cap * sizeof *h->entries);
  }
  h->entries[h->count] = (Entry){key, x};
  *s = ++h->count;
}

static Value *hash_get(const Value *h, const char *key) {
  int64_t *s = slot_of(h, key);
  return *s ? h->entries[*s - 1].value : NULL;
}

static void free_value(Value *v) {
  if (!v) return;
  for (int64_t i = 0; v->type == T_ARR && i < v->count; ++i) free_value(v->items[i]);
  for (int64_t i = 0; v->type == T_HASH && i < v->count; ++i) {
    free(v->entries[i].key);
    free_value(v->entries[i].value);
  }
  free(v->str);
  free(v->items);
  free(v->entries);
  free(v->index);
  free(v);
}

/* c with every w replaced by r, in a new string. */
static char *strtran(const char *c, const char *w, const char *r) {
  size_t wlen = strlen(w), rlen = strlen(r), count = 0, n = 0, clen = strlen(c);
  for (const char *p = strstr(c, w); p; p = strstr(p + wlen, w)) ++count;
  char *out = malloc(clen + count * rlen - count * wlen + 1);
  const char *from = c;
  for (const char *p = strstr(c, w); p; p = strstr(p + wlen, w)) {
    memcpy(out + n, from, (size_t)(p - from));
    n += (size_t)(p - from);
    memcpy(out + n, r, rlen);
    n += rlen;
    from = p + wlen;
  }
  strcpy(out + n, from);
  return out;
}

static void quote(Buf *b, const char *c) {
  static const char *const from[] = {"\\", "\"", "\n", "\t"}, *const to[] = {"\\\\", "\\\"", "\\n", "\\t"};
  char *s = strtran(c, from[0], to[0]);
  for (int i = 1; i < 4; ++i) {
    char *u = strtran(s, from[i], to[i]);
    free(s);
    s = u;
  }
  put(b, "\"", 1);
  puts_(b, s);
  put(b, "\"", 1);
  free(s);
}

static void to_json(Buf *b, const Value *x) {
  char num[24];
  switch (x ? x->type : T_NIL) {
  case T_HASH:
    put(b, "{", 1);
    for (int64_t i = 0; i < x->count; ++i) {
      if (i) put(b, ",", 1);
      quote(b, x->entries[i].key);
      put(b, ":", 1);
      to_json(b, x->entries[i].value);
    }
    put(b, "}", 1);
    break;
  case T_ARR:
    put(b, "[", 1);
    for (int64_t i = 0; i < x->count; ++i) {
      if (i) put(b, ",", 1);
      to_json(b, x->items[i]);
    }
    put(b, "]", 1);
    break;
  case T_STR:
    quote(b, x->str);
    break;
  case T_NUM:
    snprintf(num, sizeof num, "%lld", (long long)x->num);
    puts_(b, num);
    break;
  case T_LOG:
    puts_(b, x->num ? "true" : "false");
    break;
  default:
    puts_(b, "null");
  }
}

/* The string that starts at the quote at *pos, which it leaves after the closing quote. */
static char *parse_string(const char *c, size_t *pos) {
  Buf out = {0};
  size_t start = ++*pos;
  put(&out, "", 0);
  while (c[*pos] != '"') {
    if (c[*pos] == '\\') {
      put(&out, c + start, *pos - start);
      char ch = c[*pos + 1];
      put(&out, ch == 'n' ? "\n" : ch == 't' ? "\t" : &ch, 1);
      *pos += 2;
      start = *pos;
    } else
      ++*pos;
  }
  put(&out, c + start, *pos - start);
  ++*pos;
  return out.data;
}

/* The value that starts at *pos, which it leaves after the value; NULL for null. */
static Value *parse(const char *c, size_t *pos) {
  char ch = c[*pos];
  Value *x = NULL;
  if (ch == '{') {
    x = new_value(T_HASH);
    if (c[++*pos] == '}') {
      ++*pos;
      return x;
    }
    for (;;) {
      char *k = parse_string(c, pos);
      ++*pos;
      hash_set(x, k, parse(c, pos));
      if (c[(*pos)++] == '}') break;
    }
  } else if (ch == '[') {
    x = new_value(T_ARR);
    if (c[++*pos] == ']') {
      ++*pos;
      return x;
    }
    for (;;) {
      array_add(x, parse(c, pos));
      if (c[(*pos)++] == ']') break;
    }
  } else if (ch == '"')
    x = new_str(parse_string(c, pos));
  else if (ch == 't') {
    x = new_value(T_LOG);
    x->num = 1;
    *pos += 4;
  } else if (ch == 'f') {
    x = new_value(T_LOG);
    *pos += 5;
  } else if (ch == 'n')
    *pos += 4;
  else {
    size_t k = *pos;
    while (c[*pos] && strchr("-0123456789", c[*pos])) ++*pos;
    char *s = substr(c + k, *pos - k);
    x = new_num(strtoll(s, NULL, 10));
    free(s);
  }
  return x;
}

static char *dup(const char *s) { return substr(s, strlen(s)); }

/* A record, drawn from the seed in the order the checks' constants were computed. */
static Value *make_record(int64_t i) {
  static const char letters[] = "abcdefghijk ABC\"\\\n\txyz";
  char name[16], text[24];
  int64_t n = rnd(11) + 5, k;
  for (k = 0; k < n; ++k) name[k] = letters[rnd(22)];
  name[k] = '\0';
  n = rnd(4);
  int active = rnd(2) == 1;
  int64_t qty = rnd(2001) - 1000;
  Value *parent = NULL;
  if (rnd(3) != 0) parent = new_num(rnd(i) + 1);
  snprintf(text, sizeof text, "City %lld", (long long)rnd(50));
  Value *address = new_value(T_HASH), *tags = new_value(T_ARR), *h = new_value(T_HASH), *log = new_value(T_LOG);
  hash_set(address, dup("city"), new_str(dup(text)));
  hash_set(address, dup("zip"), new_num(rnd(90000) + 10000));
  for (k = 1; k <= n; ++k) {
    snprintf(text, sizeof text, "tag%lld", (long long)rnd(100));
    array_add(tags, new_str(dup(text)));
  }
  log->num = active;
  hash_set(h, dup("id"), new_num(i));
  hash_set(h, dup("name"), new_str(dup(name)));
  hash_set(h, dup("tags"), tags);
  hash_set(h, dup("active"), log);
  hash_set(h, dup("qty"), new_num(qty));
  hash_set(h, dup("parent"), parent);
  hash_set(h, dup("address"), address);
  return h;
}

int main(void) {
  Value *data = new_value(T_ARR);
  int64_t qty = 0, active = 0, tags = 0, nulls = 0, zip = 0;
  for (int64_t i = 1; i <= 20000; ++i) array_add(data, make_record(i));
  long long t = bench_ms();
  Buf c1 = {0}, c2 = {0};
  size_t pos = 0;
  to_json(&c1, data);
  Value *back = parse(c1.data, &pos);
  to_json(&c2, back);
  for (int64_t i = 0; i < back->count; ++i) {
    Value *h = back->items[i];
    qty += hash_get(h, "qty")->num;
    active += hash_get(h, "active")->num;
    tags += hash_get(h, "tags")->count;
    nulls += hash_get(h, "parent") == NULL;
    zip += hash_get(hash_get(h, "address"), "zip")->num;
  }
  int ok = c1.len == c2.len && memcmp(c1.data, c2.data, c1.len) == 0 && c1.len == 2697804 && pos == c1.len &&
           back->count == 20000 && qty == -88759 && active == 9960 && tags == 29729 && nulls == 6525 &&
           zip == 1098378340 && strcmp(hash_get(back->items[0], "name")->str, "ykAeACczkccifkx") == 0 &&
           strcmp(hash_get(back->items[1], "name")->str, "Cdkk\"\\Ae\nx\nz") == 0 &&
           strncmp(c1.data, "[{\"id\":1,\"name\":\"ykAeACczkccifkx\"", 33) == 0;
  bench_report("json_roundtrip", t, ok);
  free(c1.data);
  free(c2.data);
  free_value(back);
  free_value(data);
  return 0;
}
