/* bench/xml_simples.prg in C: a catalog of 50,000 books { title, author, price, year } (titles and authors with & < >
 * and ' in them) made from a fixed seed, then timed: written as one document in a growing buffer (the text escaped by
 * four replacements in a row, each making a new string as StrTran() does), split into elements (each a new string),
 * and read back (each child's text found with strstr() between its tags, the id attribute, the entities decoded by
 * four replacements, the numbers read with strtod() as Val() reads them). The values read back add up as made. */
#include "bench.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *title;
  const char *author;
  int64_t price, year;
} Book;

typedef struct {
  char *data;
  size_t len, cap;
} Buf;

static int64_t seed = 5150;
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

static char *substr(const char *s, size_t n) {
  char *r = malloc(n + 1);
  memcpy(r, s, n);
  r[n] = '\0';
  return r;
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

/* Four replacements in a row, from[i] by to[i]. */
static char *strtran4(const char *c, const char *const *from, const char *const *to) {
  char *s = strtran(c, from[0], to[0]);
  for (int i = 1; i < 4; ++i) {
    char *u = strtran(s, from[i], to[i]);
    free(s);
    s = u;
  }
  return s;
}

static const char *const plain[] = {"&", "<", ">", "'"}, *const entity[] = {"&amp;", "&lt;", "&gt;", "&apos;"};
static const char *const entity_back[] = {"&apos;", "&gt;", "&lt;", "&amp;"};
static const char *const plain_back[] = {"'", ">", "<", "&"};

/* The text between <tag> and </tag> in c, in a new string. */
static char *inner(const char *c, const char *tag) {
  char open[16], close[16];
  snprintf(open, sizeof open, "<%s>", tag);
  snprintf(close, sizeof close, "</%s>", tag);
  const char *start = strstr(c, open) + strlen(open);
  return substr(start, (size_t)(strstr(c, close) - start));
}

/* The parts of c around each sep, each a new string. */
static char **split(const char *c, const char *sep, int64_t *count) {
  size_t slen = strlen(sep);
  int64_t n = 1, k = 0;
  for (const char *p = strstr(c, sep); p; p = strstr(p + slen, sep)) ++n;
  char **a = malloc((size_t)n * sizeof *a);
  for (const char *p = strstr(c, sep); p; p = strstr(c, sep)) {
    a[k++] = substr(c, (size_t)(p - c));
    c = p + slen;
  }
  a[k++] = substr(c, strlen(c));
  *count = k;
  return a;
}

int main(void) {
  static const char *const words[] = {"Rivers", "Salt & Pepper", "<Night>", "Stone", "Tales",
                                      "Tom & Jerry", "Glass", "Winter", "Red", "Old"};
  static const char *const authors[] = {"Ana Lima", "Bruno Costa", "Carla Dias", "Diego Reis", "Eva O'Neil"};
  int64_t nbooks = 50000, cents = 0, amp = 0, eva = 0, nparts;
  double ids = 0, years = 0;
  Book *books = malloc((size_t)nbooks * sizeof *books);
  char *c12345 = NULL, line[512];
  for (int64_t i = 0; i < nbooks; ++i) {
    int64_t n = rnd(2) + 2;
    Buf title = {0};
    put(&title, "");
    for (int64_t k = 1; k <= n; ++k) {
      if (k > 1) put(&title, " ");
      put(&title, words[rnd(10)]);
    }
    books[i].title = title.data;
    books[i].author = authors[rnd(5)];
    books[i].price = rnd(10000) + 100;
    books[i].year = 1950 + rnd(75);
  }
  long long t = bench_ms();
  Buf c = {0};
  put(&c, "<?xml version=\"1.0\"?>\n<catalog>\n");
  for (int64_t i = 1; i <= nbooks; ++i) {
    Book *b = &books[i - 1];
    char *title = strtran4(b->title, plain, entity), *author = strtran4(b->author, plain, entity);
    snprintf(line, sizeof line,
             "<book id=\"%lld\"><title>%s</title><author>%s</author><price>%.2f</price><year>%4lld</year></book>\n",
             (long long)i, title, author, (double)b->price / 100, (long long)b->year);
    put(&c, line);
    free(title);
    free(author);
  }
  put(&c, "</catalog>\n");
  int ok = c.len == 6276273;
  char **parts = split(c.data, "<book ", &nparts);
  for (int64_t i = 2; i <= nparts; ++i) {
    const char *book = parts[i - 1];
    ids += strtod(book + 4, NULL);
    char *raw = inner(book, "title"), *title = strtran4(raw, entity_back, plain_back);
    free(raw);
    if (strchr(title, '&')) ++amp;
    if (i == 12346)
      c12345 = title;
    else
      free(title);
    raw = inner(book, "author");
    char *author = strtran4(raw, entity_back, plain_back);
    if (strcmp(author, "Eva O'Neil") == 0) ++eva;
    free(raw);
    free(author);
    raw = inner(book, "price");
    cents += (int64_t)round(strtod(raw, NULL) * 100);
    free(raw);
    raw = inner(book, "year");
    years += strtod(raw, NULL);
    free(raw);
  }
  ok = ok && nparts == 50001 && ids == 50000.0 * 50001 / 2 && cents == 255248286 && amp == 21225 &&
       years == 99351507 && eva == 10152 && c12345 && strcmp(c12345, "Winter Glass Tom & Jerry") == 0;
  bench_report("xml_simples", t, ok);
  free(c12345);
  for (int64_t i = 0; i < nparts; ++i) free(parts[i]);
  free(parts);
  free(c.data);
  for (int64_t i = 0; i < nbooks; ++i) free(books[i].title);
  free(books);
  return 0;
}
