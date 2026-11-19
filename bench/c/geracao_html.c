/* bench/geracao_html.prg in C: a table of 100,000 items { name of two words, quantity, price in cents } made from a
 * fixed seed, then one HTML page built by appending to a growing buffer, each row with its id, a zebra class, the name
 * escaped (four replacements in a row, each making a new string as StrTran() does), and the price and total formatted
 * with snprintf() at the .prg's widths. The page's length, a row and its end are checked. */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *data;
  size_t len, cap;
} Buf;

typedef struct {
  char *name;
  int64_t qty, price;
} Item;

static int64_t seed = 4711;
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

static char *escape(const char *c) {
  static const char *const from[] = {"&", "<", ">", "\""}, *const to[] = {"&amp;", "&lt;", "&gt;", "&quot;"};
  char *s = strtran(c, from[0], to[0]);
  for (int i = 1; i < 4; ++i) {
    char *u = strtran(s, from[i], to[i]);
    free(s);
    s = u;
  }
  return s;
}

/* Str( x, width, dec ) with LTrim(). */
static void put_num(Buf *b, double x, int dec) {
  char s[40];
  snprintf(s, sizeof s, "%.*f", dec, x);
  puts_(b, s);
}

int main(void) {
  static const char *const words[] = {"Alpha",   "Beta & Sons", "<Gamma>", "Delta", "Echo \"Q\"",
                                      "Foxtrot", "Golf",        "H&M"};
  int64_t n = 100000;
  size_t pos77 = 0;
  Item *items = malloc((size_t)n * sizeof *items);
  for (int64_t i = 0; i < n; ++i) {
    const char *w1 = words[rnd(8)], *w2 = words[rnd(8)];
    items[i].name = malloc(strlen(w1) + strlen(w2) + 2);
    sprintf(items[i].name, "%s %s", w1, w2);
    items[i].qty = rnd(100) + 1;
    items[i].price = rnd(1000000) + 1;
  }
  long long t = bench_ms();
  Buf c = {0};
  char id[24];
  puts_(&c, "<html><head><title>Items</title></head><body>\n<table>\n"
            "<tr><th>Id</th><th>Name</th><th>Qty</th><th>Price</th><th>Total</th></tr>\n");
  for (int64_t i = 1; i <= n; ++i) {
    Item *r = &items[i - 1];
    snprintf(id, sizeof id, "%lld", (long long)i);
    if (i == 77777) pos77 = c.len + 1;
    char *name = escape(r->name);
    puts_(&c, "<tr id=\"r");
    puts_(&c, id);
    puts_(&c, "\" class=\"");
    puts_(&c, i % 2 == 1 ? "odd" : "even");
    puts_(&c, "\"><td>");
    puts_(&c, id);
    puts_(&c, "</td><td>");
    puts_(&c, name);
    puts_(&c, "</td><td class=\"n\">");
    put_num(&c, (double)r->qty, 0);
    puts_(&c, "</td><td class=\"n\">");
    put_num(&c, (double)r->price / 100, 2);
    puts_(&c, "</td><td class=\"n\">");
    put_num(&c, (double)(r->qty * r->price) / 100, 2);
    puts_(&c, "</td></tr>\n");
    free(name);
  }
  puts_(&c, "</table>\n</body></html>\n");
  const char *row77 = "<tr id=\"r77777\" class=\"odd\"><td>77777</td><td>&lt;Gamma&gt; Delta</td>"
                      "<td class=\"n\">56</td><td class=\"n\">3992.87</td><td class=\"n\">223600.72</td></tr>\n";
  const char *tail = "<tr id=\"r100000\" class=\"even\"><td>100000</td><td>Foxtrot Alpha</td><td class=\"n\">57</td>"
                     "<td class=\"n\">4506.67</td><td class=\"n\">256880.19</td></tr>\n</table>\n</body></html>\n";
  size_t tlen = strlen(tail);
  int ok = c.len == 15121698 && pos77 == 11756338 && strncmp(c.data + pos77 - 1, row77, strlen(row77)) == 0 &&
           memcmp(c.data + c.len - tlen, tail, tlen) == 0;
  bench_report("geracao_html", t, ok);
  free(c.data);
  for (int64_t i = 0; i < n; ++i) free(items[i].name);
  free(items);
  return 0;
}
