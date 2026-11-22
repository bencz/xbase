/* The declarations of include/rtl.ch against the runtime: every NATIVE function that declares a RESULT is called with
 * arguments of its parameters' AS types (any value for a parameter without one), every count from its required
 * parameters to all of them and one past them for `...`, over a matrix of values of each type; then with any value for
 * every parameter. With arguments of their types, a TOTAL one raises no error; any call that raises none gives a value
 * of a RESULT type, within its RANGE. What the compiler takes from the declarations (the types and ranges of results,
 * calls that cannot fail) is then what the runtime does. */
#include "check.h"
#include "xbrt_int.h"

#include "core/symbols.h"

#include <stdint.h>
#include <string.h>

#ifndef XB_TEST_RTL_CH
#error "XB_TEST_RTL_CH names include/rtl.ch"
#endif

#define MAX_PARAMS 8
#define MAX_VALUES 64

typedef struct decl {
  char name[64];
  uint32_t nparams, required;
  int rest; /* `...`: the last of the nparams is one argument past the named ones */
  uint32_t types[MAX_PARAMS]; /* 0: any */
  uint32_t result;
  int ranged, total;
  int64_t lo, hi;
} decl;

static const struct {
  const char *name;
  uint32_t tags;
} type_names[] = {{"NIL", 1u << XB_T_NIL},       {"LOGICAL", 1u << XB_T_LOGICAL},
                  {"INTEGER", 1u << XB_T_INT},   {"DOUBLE", 1u << XB_T_DOUBLE},
                  {"NUMERIC", (1u << XB_T_INT) | (1u << XB_T_DOUBLE)},
                  {"STRING", 1u << XB_T_STRING}, {"DATE", 1u << XB_T_DATE},
                  {"ARRAY", 1u << XB_T_ARRAY},   {"HASH", 1u << XB_T_HASH},
                  {"BLOCK", 1u << XB_T_BLOCK},   {"OBJECT", 1u << XB_T_OBJECT},
                  {"SYMBOL", 1u << XB_T_SYMBOL}, {"ANY", 0}};

static int errors;

static const xb_class probe_class = {"PROBE", 0, NULL, NULL, 0, NULL, NULL}; /* an object of no variable or method */

/* The error block: counts the error and breaks out of the call (as a handler with BREAK would: an error without a
 * default or a substitute needs it), which the caller then clears. */
static void count_error(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, xb_block *self) {
  (void)argc;
  (void)argv;
  (void)self;
  ++errors;
  xb_set_nil(ret);
  const xb_symbol_entry *brk = xb_symbol_find("BREAK", 5);
  brk->ref.fn(ctx, ret, 0, NULL);
}

/* The words of `text` from `*p`: a name, a number, or one other character; 0 at the end. */
static size_t word(const char *text, size_t *p, char *out, size_t room) {
  while (text[*p] == ' ' || text[*p] == '\t' || text[*p] == '\r' || text[*p] == '\n' || text[*p] == ';') ++*p;
  size_t n = 0, start = *p;
  if (!text[*p]) return 0;
  if ((text[*p] >= 'A' && text[*p] <= 'Z') || (text[*p] >= 'a' && text[*p] <= 'z') || text[*p] == '_' ||
      (text[*p] >= '0' && text[*p] <= '9') || text[*p] == '-') {
    ++*p;
    while ((text[*p] >= 'A' && text[*p] <= 'Z') || (text[*p] >= 'a' && text[*p] <= 'z') || text[*p] == '_' ||
           (text[*p] >= '0' && text[*p] <= '9'))
      ++*p;
  } else {
    ++*p;
  }
  n = *p - start;
  if (n >= room) n = room - 1;
  memcpy(out, text + start, n);
  out[n] = 0;
  for (size_t k = 0; k < n; ++k)
    if (out[k] >= 'a' && out[k] <= 'z') out[k] = (char)(out[k] - 'a' + 'A');
  return n;
}

/* A type at `*p` (names joined by |, INTEGER RANGE lo TO hi). */
static uint32_t type_at(const char *text, size_t *p, decl *d, int result) {
  char w[64];
  uint32_t tags = 0;
  for (;;) {
    word(text, p, w, sizeof w);
    int known = 0;
    for (size_t k = 0; k < sizeof type_names / sizeof type_names[0]; ++k)
      if (!strcmp(w, type_names[k].name)) {
        tags |= type_names[k].tags;
        known = 1;
      }
    CHECK(known);
    size_t at = *p;
    word(text, p, w, sizeof w);
    if (!strcmp(w, "RANGE")) {
      word(text, p, w, sizeof w);
      d->lo = strtoll(w, NULL, 10);
      word(text, p, w, sizeof w);
      CHECK(!strcmp(w, "TO"));
      word(text, p, w, sizeof w);
      d->hi = strtoll(w, NULL, 10);
      if (result) d->ranged = 1;
      at = *p;
      word(text, p, w, sizeof w);
    }
    if (strcmp(w, "|")) {
      *p = at;
      return tags;
    }
  }
}

/* The declaration of the line `text` (an EXTERN FUNCTION, continuations joined): 0 when it is not a NATIVE one with a
 * RESULT. */
static int parse(const char *text, decl *d) {
  memset(d, 0, sizeof *d);
  size_t p = 0;
  char w[64];
  word(text, &p, w, sizeof w); /* EXTERN */
  word(text, &p, w, sizeof w); /* FUNCTION */
  word(text, &p, d->name, sizeof d->name);
  word(text, &p, w, sizeof w); /* ( */
  int optional = 0, native = 0, result = 0;
  for (;;) {
    word(text, &p, w, sizeof w);
    if (!strcmp(w, ")")) break;
    if (!strcmp(w, ",") || (d->rest && !strcmp(w, "."))) continue;
    int bracket = !strcmp(w, "[");
    if (bracket) { /* [name], [@name] */
      optional = 1;
      word(text, &p, w, sizeof w);
    }
    if (!strcmp(w, "@")) word(text, &p, w, sizeof w); /* a reference: it is called with values here */
    if (!strcmp(w, ".")) { /* `...`, `@...`: one argument more, optional */
      d->rest = 1;
      optional = 1;
    }
    if (bracket) word(text, &p, w, sizeof w); /* ] */
    if (d->nparams == MAX_PARAMS) return 0; /* none with a RESULT has as many */
    size_t at = p;
    word(text, &p, w, sizeof w);
    if (!strcmp(w, "AS")) d->types[d->nparams] = type_at(text, &p, d, 0);
    else p = at;
    if (!optional) d->required = d->nparams + 1;
    ++d->nparams;
  }
  while (word(text, &p, w, sizeof w)) {
    if (!strcmp(w, "NATIVE")) native = 1;
    if (!strcmp(w, "TOTAL")) d->total = 1;
    if (!strcmp(w, "RESULT")) {
      d->result = type_at(text, &p, d, 1);
      result = 1;
    }
  }
  return native && result;
}

/* The values of the matrix of type `tags` (any value: all of them) in `out`; their count. `block` is a codeblock. */
static size_t values(uint32_t tags, xb_item *out, const xb_item *block) {
  /* 65537 and 70000.25 past 16 bits, yet short as a width (Space(), PadL()): every call stays quick */
  static const int64_t ints[] = {0, 1, -1, 2, 255, 256, 65537, INT64_MAX, INT64_MIN};
  const double zero = 0.0; /* not a constant expression: NaN and the infinities computed */
  const double doubles[] = {0.0,     -zero,     1.5,        -1.5,          0.999999,  1e300,
                            -1e300,  2.5e-300,  70000.25,   zero / zero,   1 / zero, -1 / zero};
  static const char *strings[] = {"",         "a",           "  pad  ",   "Hello, World", "\xff\x80z",
                                  "20240229", "12/31/1999",  "abcdefghijklmnopqrstuvwxyz0123"};
  static const int64_t dates[] = {INT64_MIN, -1, 0, 1, 2451545, 2460371, 5373484, 100000000, INT64_MAX};
  size_t n = 0;
  int any = tags == 0;
  if (any || (tags & (1u << XB_T_NIL))) xb_set_nil(&out[n++]);
  if (any || (tags & (1u << XB_T_LOGICAL))) {
    xb_set_logical(&out[n++], 1);
    xb_set_logical(&out[n++], 0);
  }
  if (any || (tags & (1u << XB_T_INT)))
    for (size_t k = 0; k < sizeof ints / sizeof ints[0]; ++k) xb_set_int(&out[n++], ints[k]);
  if (any || (tags & (1u << XB_T_DOUBLE)))
    for (size_t k = 0; k < sizeof doubles / sizeof doubles[0]; ++k) xb_set_double(&out[n++], doubles[k], 2);
  if (any || (tags & (1u << XB_T_STRING)))
    for (size_t k = 0; k < sizeof strings / sizeof strings[0]; ++k)
      xb_set_str(&out[n++], xb_str_new(strings[k], strlen(strings[k])));
  if (any || (tags & (1u << XB_T_DATE)))
    for (size_t k = 0; k < sizeof dates / sizeof dates[0]; ++k) xb_set_date(&out[n++], dates[k]);
  if (any || (tags & (1u << XB_T_ARRAY))) {
    xb_set_array(&out[n++], xb_array_alloc(0));
    xb_set_array(&out[n++], xb_array_alloc(3));
  }
  if (any || (tags & (1u << XB_T_HASH))) xb_set_hash(&out[n++], xb_hash_alloc());
  if (any || (tags & (1u << XB_T_BLOCK))) out[n++] = *block;
  if (any || (tags & (1u << XB_T_OBJECT))) xb_set_ptr(&out[n++], XB_T_OBJECT, xb_object_new(&probe_class));
  if (any || (tags & (1u << XB_T_SYMBOL))) {
    const xb_symbol_entry *len = xb_symbol_find("LEN", 3);
    CHECK(len);
    xb_set_ptr(&out[n++], XB_T_SYMBOL, (void *)&len->ref);
  }
  CHECK(n <= MAX_VALUES);
  return n;
}

static int calls;

/* The values of an argument past the named ones (`...`): a few, small (Array()'s further dimensions). */
static size_t rest_values(xb_item *out) {
  xb_set_nil(&out[0]);
  xb_set_int(&out[1], 2);
  xb_set_str(&out[2], xb_str_new("x", 1));
  return 3;
}

/* Every combination of the matrix's values for the first `argc` parameters of `d`; `typed`: of their AS types, where a
 * TOTAL function raises no error. The arguments and the result are roots, as a compiled caller's frame slots are: a
 * collection while the function allocates moves what they refer to (Array( n, m ) keeps its rows in `ret`). */
static void call_all(xb_ctx *ctx, const decl *d, xb_func fn, uint32_t argc, xb_item (*pool)[MAX_VALUES],
                     const size_t *counts, int typed) {
  size_t index[MAX_PARAMS] = {0};
  xb_item argv[MAX_PARAMS], ret;
  for (uint32_t k = 0; k < MAX_PARAMS; ++k) {
    xb_set_nil(&argv[k]);
    xb_gc_root_push(ctx, &argv[k]);
  }
  xb_set_nil(&ret);
  xb_gc_root_push(ctx, &ret);
  for (;;) {
    for (uint32_t k = 0; k < argc; ++k) argv[k] = pool[k][index[k]];
    xb_set_nil(&ret);
    int before = errors;
    fn(ctx, &ret, argc, argv);
    ++calls;
    if (typed && d->total && errors != before) {
      fprintf(stderr, "%s: a TOTAL call raised an error (argument types:", d->name);
      for (uint32_t k = 0; k < argc; ++k) fprintf(stderr, " %u", argv[k].type);
      fprintf(stderr, ")\n");
      exit(1);
    }
    if (errors == before) {
      if (!(d->result & (1u << ret.type))) {
        fprintf(stderr, "%s: a result of type %u, not of its RESULT\n", d->name, ret.type);
        exit(1);
      }
      if (d->ranged && ret.type == XB_T_INT && (ret.v.i < d->lo || ret.v.i > d->hi)) {
        fprintf(stderr, "%s: %lld out of its RANGE\n", d->name, (long long)ret.v.i);
        exit(1);
      }
    }
    xb_pending_clear(ctx, ~0u);
    uint32_t k = 0;
    while (k < argc && ++index[k] == counts[k]) index[k++] = 0;
    if (k == argc) break;
  }
  xb_gc_root_pop(ctx, MAX_PARAMS + 1);
}

int main(void) {
  xb_platform_init();
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_ctx *ctx = xb_ctx_new();
  xb_symbols_seal();
  static xb_item handler;
  xb_set_nil(&handler);
  xb_gc_add_root(&handler, 1);
  xb_block_new(ctx, &handler, count_error, 0);
  ctx->env->error_block = handler;

  FILE *f = fopen(XB_TEST_RTL_CH, "rb");
  CHECK(f);
  static char text[1 << 20];
  size_t len = fread(text, 1, sizeof text - 1, f);
  fclose(f);
  text[len] = 0;
  static xb_item pool[MAX_PARAMS][MAX_VALUES];
  xb_gc_add_root(&pool[0][0], MAX_PARAMS * MAX_VALUES);
  int checked = 0;
  for (char *line = strstr(text, "\nEXTERN FUNCTION "); line; line = strstr(line + 1, "\nEXTERN FUNCTION ")) {
    char decl_text[1024];
    size_t n = 0;
    for (char *c = line + 1; *c && n + 1 < sizeof decl_text; ++c) { /* the line, and those its ';' continues */
      if (*c == '\n') {
        size_t back = n;
        while (back && (decl_text[back - 1] == ' ' || decl_text[back - 1] == '\r')) --back;
        if (!back || decl_text[back - 1] != ';') break;
      }
      decl_text[n++] = *c;
    }
    decl_text[n] = 0;
    decl d;
    if (!parse(decl_text, &d)) continue;
    const xb_symbol_entry *e = xb_symbol_find(d.name, strlen(d.name));
    CHECK(e);
    size_t counts[MAX_PARAMS];
    for (int typed = 1; typed >= 0; --typed) {
      for (uint32_t k = 0; k < d.nparams; ++k)
        counts[k] = d.rest && k + 1 == d.nparams ? rest_values(pool[k])
                                                 : values(typed ? d.types[k] : 0, pool[k], &handler);
      for (uint32_t argc = d.required; argc <= d.nparams; ++argc)
        call_all(ctx, &d, e->ref.fn, argc, pool, counts, typed);
    }
    ++checked;
  }
  CHECK(checked > 40 && calls > 1000);
  printf("declarations: %d functions, %d calls\n", checked, calls);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
  return 0;
}
