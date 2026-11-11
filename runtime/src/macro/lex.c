/* The macro compiler's tokens: the compiler's for expressions (see its
 * preprocessor lexer), from a string. */
#include "macro/macro.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int is_ident_start(char c) { return is_alpha(c) || c == '_'; }
static int is_ident_char(char c) { return is_ident_start(c) || is_digit(c); }
static int is_hex(char c) { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static int is_blank(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

void xb_mlex_init(mlexer *lx, const char *src, size_t len) {
  lx->pos = src;
  lx->end = src + len;
  memset(&lx->tok, 0, sizeof lx->tok);
  lx->tok.kind = MT_END;
  xb_mlex_next(lx);
}

static int at(const mlexer *lx, size_t k, char c) { return lx->pos + k < lx->end && lx->pos[k] == c; }

/* A number: decimal digits with an optional fraction, `.5`, or 0x hex
 * digits; the value and the display width as the compiler gives them. */
static void lex_number(mlexer *lx, mtoken *t) {
  const char *p = lx->pos;
  char buf[64];
  if (at(lx, 0, '0') && (at(lx, 1, 'x') || at(lx, 1, 'X')) && lx->pos + 2 < lx->end && is_hex(lx->pos[2])) {
    p += 2;
    uint64_t v = 0;
    int over = 0;
    for (; p < lx->end && is_hex(*p); ++p) {
      unsigned d = is_digit(*p) ? (unsigned)(*p - '0') : (unsigned)((*p | 0x20) - 'a' + 10);
      if (v > ((uint64_t)INT64_MAX - d) / 16) over = 1;
      v = v * 16 + d;
    }
    t->kind = over ? MT_BAD : MT_NUMBER;
    t->isInt = 1;
    t->ival = (int64_t)v;
    lx->pos = p;
    return;
  }
  while (p < lx->end && is_digit(*p)) ++p;
  size_t intDigits = (size_t)(p - lx->pos);
  int hasDot = 0;
  uint16_t dec = 0;
  if (p + 1 < lx->end && *p == '.' && is_digit(p[1])) {
    hasDot = 1;
    const char *d = ++p;
    while (p < lx->end && is_digit(*p)) ++p;
    dec = (uint16_t)(p - d);
  }
  size_t n = (size_t)(p - lx->pos);
  t->kind = MT_NUMBER;
  t->dec = dec;
  char *text = n < sizeof buf ? buf : malloc(n + 1); /* strtoll and strtod need a NUL */
  if (!text) xb_gc_fatal("out of memory");
  memcpy(text, lx->pos, n);
  text[n] = 0;
  int isDouble = hasDot;
  if (!hasDot) {
    errno = 0;
    long long v = strtoll(text, NULL, 10);
    if (errno != ERANGE) {
      t->isInt = 1;
      t->ival = v;
    } else {
      isDouble = 1; /* too large for 64 bits: a double */
    }
  }
  if (isDouble) t->dval = strtod(text, NULL);
  if (text != buf) free(text);
  t->width = xb_num_literal_width(isDouble, t->ival, t->dval, intDigits, dec);
  lx->pos = p;
}

/* `[` opens a string unless it follows something that can be indexed, and
 * only when a `]` closes it on the same line. */
static int bracket_string_allowed(const mlexer *lx) {
  switch (lx->prev.kind) {
  case MT_IDENT: case MT_RPAREN: case MT_RBRACKET: case MT_RBRACE: case MT_STRING: case MT_NUMBER: case MT_TRUE:
  case MT_FALSE:
    return 0;
  default:
    for (const char *q = lx->pos + 1; q < lx->end && *q != '\n'; ++q)
      if (*q == ']') return 1;
    return 0;
  }
}

/* .T. .F. .Y. .N. .AND. .OR. .NOT., in any case. */
static int lex_dot_word(mlexer *lx, mtoken *t) {
  const char *p = lx->pos + 1;
  while (p < lx->end && is_alpha(*p)) ++p;
  if (p >= lx->end || *p != '.') return 0;
  char w[4];
  size_t n = (size_t)(p - lx->pos - 1);
  if (n == 0 || n > 3) return 0;
  for (size_t i = 0; i < n; ++i) w[i] = xb_ascii_upper(lx->pos[1 + i]);
  w[n] = 0;
  if (!strcmp(w, "T") || !strcmp(w, "Y")) t->kind = MT_TRUE;
  else if (!strcmp(w, "F") || !strcmp(w, "N")) t->kind = MT_FALSE;
  else if (!strcmp(w, "AND")) t->kind = MT_AND;
  else if (!strcmp(w, "OR")) t->kind = MT_OR;
  else if (!strcmp(w, "NOT")) t->kind = MT_NOT;
  else return 0;
  lx->pos = p + 1;
  return 1;
}

static const struct {
  const char *s;
  mtok k;
} operators[] = {
  {"^^=", MT_XOREQ}, {"<<=", MT_SHLEQ}, {">>=", MT_SHREQ}, {"^^", MT_BITXOR}, {"<<", MT_SHL}, {">>", MT_SHR},
  {"&=", MT_ANDEQ},  {"|=", MT_OREQ},   {"**=", MT_POWEQ}, {":=", MT_ASSIGN}, {"+=", MT_PLUSEQ}, {"-=", MT_MINUSEQ},
  {"*=", MT_MULEQ},  {"/=", MT_DIVEQ},  {"%=", MT_MODEQ},  {"^=", MT_POWEQ},  {"==", MT_EXACTEQ}, {"=>", MT_FATARROW},
  {"!=", MT_NE},     {"<>", MT_NE},     {"<=", MT_LE},     {">=", MT_GE},     {"++", MT_INC},     {"--", MT_DEC},
  {"->", MT_ARROW},  {"**", MT_POWER},  {"+", MT_PLUS},    {"-", MT_MINUS},   {"*", MT_STAR},     {"/", MT_SLASH},
  {"%", MT_PERCENT}, {"^", MT_POWER},   {"=", MT_EQ},      {"<", MT_LT},      {">", MT_GT},       {"!", MT_NOT},
  {"#", MT_NE},      {"$", MT_DOLLAR},  {"(", MT_LPAREN},  {")", MT_RPAREN},  {"[", MT_LBRACKET}, {"]", MT_RBRACKET},
  {"{", MT_LBRACE},  {"}", MT_RBRACE},  {",", MT_COMMA},   {":", MT_COLON},   {"|", MT_PIPE},     {"@", MT_AT},
  {"&", MT_AMP},     {".", MT_DOT},     {"~", MT_TILDE},
};

void xb_mlex_next(mlexer *lx) {
  lx->prev = lx->tok;
  mtoken *t = &lx->tok;
  memset(t, 0, sizeof *t);
  const char *before = lx->pos;
  while (lx->pos < lx->end && is_blank(*lx->pos)) ++lx->pos;
  t->spaceBefore = lx->pos != before;
  t->text = lx->pos;
  if (lx->pos >= lx->end) {
    t->kind = MT_END;
    return;
  }
  char c = *lx->pos;
  if (is_digit(c) || (c == '.' && lx->pos + 1 < lx->end && is_digit(lx->pos[1]))) {
    lex_number(lx, t);
  } else if (is_ident_start(c)) {
    while (lx->pos < lx->end && is_ident_char(*lx->pos)) ++lx->pos;
    t->kind = MT_IDENT;
  } else if (c == '"' || c == '\'' || (c == '[' && bracket_string_allowed(lx))) {
    const char *close = memchr(lx->pos + 1, c == '[' ? ']' : c, (size_t)(lx->end - lx->pos - 1));
    t->kind = close ? MT_STRING : MT_BAD;
    lx->pos = close ? close + 1 : lx->end;
  } else if (!(c == '.' && lex_dot_word(lx, t))) {
    t->kind = MT_BAD;
    size_t left = (size_t)(lx->end - lx->pos);
    for (size_t i = 0; i < sizeof operators / sizeof operators[0]; ++i) {
      const char *s = operators[i].s;
      if (s[0] != c) continue; /* the longest operators come first */
      size_t n = s[1] ? (s[2] ? 3 : 2) : 1;
      if (left >= n && (n == 1 || (lx->pos[1] == s[1] && (n == 2 || lx->pos[2] == s[2])))) {
        t->kind = operators[i].k;
        lx->pos += n;
        break;
      }
    }
    if (t->kind == MT_BAD) ++lx->pos;
  }
  t->len = (size_t)(lx->pos - t->text);
}
