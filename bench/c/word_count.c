/* bench/word_count.prg in C: 200,000 lines of 1 to 30 words (about 1,300,000 words, of a 5,000-word vocabulary in mixed
 * case and a skewed distribution) cut at blanks into new strings (the rest of the line taken into a new one after each
 * word, as SubStr() does), lower-cased and counted in a hash table of string keys; the most frequent word found. */
#include "bench.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *key; /* NULL for a free slot */
  int64_t value;
} Slot;

typedef struct {
  Slot *slots;
  int64_t count, cap;
} Hash;

typedef struct {
  char *data;
  size_t len, cap;
} Buf;

static int64_t seed = 4242;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static void put(Buf *b, const char *s) {
  size_t n = strlen(s);
  if (b->len + n + 1 > b->cap) {
    while (b->len + n + 1 > b->cap) b->cap = b->cap ? b->cap * 2 : 64;
    b->data = realloc(b->data, b->cap);
  }
  memcpy(b->data + b->len, s, n + 1);
  b->len += n;
}

static uint64_t hash_str(const char *s) {
  uint64_t h = 14695981039346656037ULL;
  for (; *s; ++s) h = (h ^ (unsigned char)*s) * 1099511628211ULL;
  return h;
}

static Slot *find(const Hash *h, const char *key) {
  uint64_t i = hash_str(key) & (uint64_t)(h->cap - 1);
  while (h->slots[i].key && strcmp(h->slots[i].key, key) != 0) i = (i + 1) & (uint64_t)(h->cap - 1);
  return &h->slots[i];
}

/* h[key] := value; the table takes key. */
static void set(Hash *h, char *key, int64_t value) {
  if ((h->count + 1) * 4 > h->cap * 3) {
    Slot *old = h->slots;
    int64_t n = h->cap;
    h->cap *= 2;
    h->slots = calloc((size_t)h->cap, sizeof *h->slots);
    for (int64_t i = 0; i < n; ++i)
      if (old[i].key) *find(h, old[i].key) = old[i];
    free(old);
  }
  Slot *s = find(h, key);
  if (s->key)
    free(key);
  else {
    s->key = key;
    ++h->count;
  }
  s->value = value;
}

static char *copy(const char *s, size_t n) {
  char *r = malloc(n + 1);
  memcpy(r, s, n);
  r[n] = '\0';
  return r;
}

static char *lower_copy(const char *s, size_t n) {
  char *r = malloc(n + 1);
  for (size_t i = 0; i < n; ++i) r[i] = (char)tolower((unsigned char)s[i]);
  r[n] = '\0';
  return r;
}

int main(void) {
  int64_t nvocab = 5000, nlines = 200000, words = 0, top = 0, check = 0;
  char(*vocab)[12] = malloc((size_t)nvocab * sizeof *vocab), **lines = malloc((size_t)nlines * sizeof *lines);
  const char *top_word = "";
  for (int64_t i = 0; i < nvocab; ++i) {
    size_t k = 0;
    for (int64_t j = 1; j <= 2 + rnd(9); ++j) vocab[i][k++] = "eEtaAoinNsrhldcumfpgwybvkxjqz"[rnd(rnd(29) + 1)];
    vocab[i][k] = '\0';
  }
  for (int64_t i = 0; i < nlines; ++i) {
    Buf c = {0};
    put(&c, "");
    for (int64_t j = 1; j <= 1 + rnd(30); ++j) {
      put(&c, vocab[rnd(rnd(5000) + 1)]);
      put(&c, " ");
    }
    lines[i] = c.data;
  }
  long long t = bench_ms();
  Hash h = {calloc(8, sizeof(Slot)), 0, 8};
  for (int64_t i = 0; i < nlines; ++i) {
    char *c = copy(lines[i], strlen(lines[i])), *p;
    while ((p = strchr(c, ' ')) != NULL) {
      char *w = lower_copy(c, (size_t)(p - c)), *rest = copy(p + 1, strlen(p + 1)); /* SubStr( c, n + 1 ) */
      free(c);
      c = rest;
      ++words;
      Slot *s = find(&h, w);
      if (s->key) {
        ++s->value;
        free(w);
      } else
        set(&h, w, 1);
    }
    free(c);
  }
  for (int64_t i = 0; i < h.cap; ++i)
    if (h.slots[i].key) {
      int64_t n = h.slots[i].value;
      check += n * n;
      if (n > top) top = n;
    }
  for (int64_t i = 0; i < nvocab; ++i) {
    char *w = lower_copy(vocab[i], strlen(vocab[i]));
    if (find(&h, w)->value == top) {
      top_word = find(&h, w)->key;
      free(w);
      break;
    }
    free(w);
  }
  int ok = words == 1312183 && h.count == 4328 && top == 7336 && strcmp(top_word, "ee") == 0 && check == 971695061;
  bench_report("word_count", t, ok);
  for (int64_t i = 0; i < h.cap; ++i) free(h.slots[i].key);
  free(h.slots);
  for (int64_t i = 0; i < nlines; ++i) free(lines[i]);
  free(lines);
  free(vocab);
  return 0;
}
