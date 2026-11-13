/* Transform( xValue, cPicture ): a value as text by a picture, Clipper's
 * rules.
 *
 * A picture is optional functions, `@` and letters up to a blank, then a
 * template. Functions: B left-justified, C " CR" after a positive number,
 * X " DB" after a negative one, ( a negative number in parentheses, ) the
 * same next to its digits, D a date's or the SET DATE picture as the
 * template, E the British date (and `,` and `.` exchanged in a number), L or
 * 0 leading zeros, R template characters that are not placeholders are
 * inserted, Z blanks for a zero, ! upper case, Sn the first n characters, T
 * (times, none yet). Template placeholders: for strings `!` upper case, `#`
 * `9` `A` `L` `N` `X` the character, `Y` Y or N; for numbers `9` `#` digits,
 * `$` `*` digits with that instead of leading blanks, `.` the point, `,` a
 * thousands separator (only between digits); for logicals `Y` Y/N, `L` `#`
 * T/F. Anything else is copied (for a string only with R; without it, it
 * takes a character's place). */
#include "xbrt_int.h"

#include <stdlib.h>
#include <string.h>

enum {
  P_LEFT = 1u << 0,         /* @B */
  P_CREDIT = 1u << 1,       /* @C */
  P_DEBIT = 1u << 2,        /* @X */
  P_ZEROS = 1u << 3,        /* @L, @0 */
  P_PARENS = 1u << 4,       /* @( */
  P_REMAIN = 1u << 5,       /* @R */
  P_UPPER = 1u << 6,        /* @! */
  P_DATE = 1u << 7,         /* @D */
  P_BRITISH = 1u << 8,      /* @E: British dates, `,` for the point */
  P_BLANK = 1u << 9,        /* @Z */
  P_PARENS_TIGHT = 1u << 10 /* @) */
};

/* The result being built. */
typedef struct out {
  char *p;
  size_t len, cap;
} out;

static void put(out *o, char c) {
  if (o->len == o->cap) {
    o->cap = o->cap ? o->cap * 2 : 32;
    char *grown = realloc(o->p, o->cap);
    if (!grown) xb_gc_fatal("out of memory");
    o->p = grown;
  }
  o->p[o->len++] = c;
}

/* SET DATE's picture with its digits shown as `c` ("99/99/99"). */
static size_t date_template(const xb_ctx *ctx, char c, char out_[XB_DATE_TEXT_SIZE]) {
  char digits[8];
  memset(digits, c, sizeof digits);
  return xb_date_format_digits(digits, ctx->env->date_format, out_);
}

static int string_placeholder(char c) { return strchr("!#9aAlLnNxXyY", c) != NULL && c; }

static void transform_string(xb_ctx *ctx, const xb_str *s, const char *pic, size_t picLen, unsigned flags, out *o) {
  char datePic[XB_DATE_TEXT_SIZE];
  if (flags & (P_DATE | P_BRITISH)) {
    picLen = date_template(ctx, 'X', datePic);
    pic = datePic;
  }
  if (!picLen) {
    for (size_t i = 0; i < s->len; ++i) put(o, flags & P_UPPER ? xb_ascii_upper(s->data[i]) : s->data[i]);
  } else {
    size_t at = 0; /* the next character of the value */
    for (size_t i = 0; i < picLen; ++i) {
      char p = pic[i];
      if (at >= s->len) { /* the value is used up: only R goes on, placeholders as blanks */
        if (!(flags & P_REMAIN)) break;
        put(o, string_placeholder(p) ? ' ' : p);
        continue;
      }
      char c = s->data[at++];
      if (p == '!') {
        put(o, xb_ascii_upper(c));
      } else if (p == 'y' || p == 'Y') {
        put(o, strchr("tTyY", c) && c ? 'Y' : 'N');
      } else if (string_placeholder(p)) {
        put(o, flags & P_UPPER ? xb_ascii_upper(c) : c);
      } else {
        put(o, p);
        if (flags & P_REMAIN) --at; /* inserted: the character waits */
      }
    }
    if ((flags & P_REMAIN) && at == 0) /* nothing was placed: the value follows whole */
      for (size_t i = 0; i < s->len; ++i) put(o, flags & P_UPPER ? xb_ascii_upper(s->data[i]) : s->data[i]);
  }
  if ((flags & P_BRITISH) && o->len >= 5) { /* Clipper's: characters 1-2 and 4-5 trade places */
    char a = o->p[0], b = o->p[1];
    o->p[0] = o->p[3];
    o->p[1] = o->p[4];
    o->p[3] = a;
    o->p[4] = b;
  }
}

static int number_placeholder(char c) { return c == '9' || c == '#' || c == '$' || c == '*'; }

/* The number as Str() gives it in a field of `width` characters (the point
 * and the decimals included), into `o`. */
static void str_field(const xb_item *v, int width, int dec, out *o) {
  int intWidth = width - (dec ? dec + 1 : 0);
  xb_str *t = xb_num_to_str(v, intWidth > 0 ? intWidth : 0, dec); /* nothing allocates while it is read */
  for (size_t i = 0; i < t->len; ++i) put(o, t->data[i]);
}

/* Returns the offset where @B starts trimming (past a parenthesis). */
static size_t transform_number(xb_ctx *ctx, const xb_item *value, const char *pic, size_t picLen, unsigned *flags,
                               out *o) {
  char datePic[XB_DATE_TEXT_SIZE];
  if (*flags & P_DATE) {
    picLen = date_template(ctx, '9', datePic);
    pic = datePic;
  }
  int width = 0, dec = 0;
  for (size_t i = 0; i < picLen; ++i) { /* the field the template asks for */
    if (pic[i] == '.') {
      while (++i < picLen)
        if (number_placeholder(pic[i])) {
          ++width;
          ++dec;
        }
      if (dec) ++width;
      break;
    }
    if (number_placeholder(pic[i])) ++width;
  }
  int pointFirst = 0; /* ".99": the field is "0.99" without its zero */
  if (width == 0) { /* the number's own field */
    xb_num_field(value, &width, &dec);
    if (ctx->env->set_fixed && value->type == XB_T_INT) width += xb_num_fixed_int_extra(ctx);
    else if (ctx->env->set_fixed) dec = xb_decimals(ctx);
    if (dec) width += dec + 1;
  } else if (dec > 0 && width - dec == 1) {
    pointFirst = 1;
    ++width;
  }
  double d = xb_num_value(value);
  xb_item abs = *value;
  if ((*flags & (P_DEBIT | P_PARENS | P_PARENS_TIGHT)) && d < 0) { /* the sign is shown another way */
    if (value->type == XB_T_INT && value->v.i != INT64_MIN) abs.v.i = -value->v.i;
    else xb_set_double(&abs, -d, value->dec);
  }
  if (d != 0) *flags &= ~(unsigned)P_BLANK;
  out str = {0};
  str_field(&abs, width, dec, &str);
  if (pointFirst) {
    --width;
    if (str.p[0] != '0') { /* no room: stars */
      str.p[0] = '.';
      memset(str.p + 1, '*', (size_t)width - 1);
    } else {
      memmove(str.p, str.p + 1, (size_t)width);
    }
    str.len = (size_t)width;
  }
  if (*flags & P_ZEROS) {
    size_t i = 0;
    for (; i < str.len && str.p[i] == ' '; ++i) str.p[i] = '0';
    if (i && i < str.len && str.p[i] == '-') { /* the sign goes first */
      str.p[0] = '-';
      str.p[i] = '0';
    }
  }
  int exchange = (*flags & P_BRITISH) != 0;
  if (!picLen) {
    for (size_t i = 0; i < str.len; ++i) put(o, str.p[i]);
    if (exchange) {
      char *point = memchr(o->p, '.', o->len);
      if (point) *point = ',';
    }
  } else {
    size_t used = 0;
    for (size_t i = 0; i < picLen; ++i) {
      char p = pic[i];
      if (p == '9' || p == '#') {
        put(o, used < (size_t)width ? str.p[used++] : ' ');
      } else if (p == '$' || p == '*') {
        if (used < (size_t)width) {
          put(o, str.p[used] == ' ' ? p : str.p[used]);
          ++used;
        } else {
          put(o, ' ');
        }
      } else if (p == '.' && used < (size_t)width) {
        put(o, exchange ? ',' : '.');
        ++used;
      } else if (p == ',' && i && used < (size_t)width) {
        char before = o->p[i - 1];
        if (before >= '0' && before <= '9') {
          put(o, exchange ? '.' : ',');
        } else { /* before the digits: what precedes it moves right */
          put(o, before);
          if (before == '-') o->p[i - 1] = i > 1 && o->p[i - 2] != '$' ? o->p[i - 2] : ' ';
        }
      } else {
        put(o, p);
      }
    }
  }
  free(str.p);
  size_t offset = 0;
  if (d < 0) {
    size_t n = o->len;
    char *r = o->p;
    if (*flags & P_PARENS_TIGHT) {
      size_t k = 0;
      if (picLen && n > 1 && pic[0] == r[0] && (pic[0] == '*' || pic[0] == '$') && r[1] == ' ') ++k;
      while (k + 1 < n && r[k + 1] == ' ') ++k;
      /* a digit where the parenthesis goes: no room, the digits become stars */
      if (r[k] >= '1' && r[k] <= '9' && (!picLen || pic[k] == '9' || pic[k] != r[k])) {
        r[k] = '(';
        for (++k; k < n; ++k)
          if (r[k] >= '0' && r[k] <= '9' && (!picLen || pic[k] == '9' || pic[k] != r[k])) r[k] = '*';
      } else {
        r[k] = '(';
      }
      put(o, ')');
    } else if (*flags & P_PARENS) {
      if (r[0] >= '1' && r[0] <= '9' && (!picLen || pic[0] == '9' || pic[0] != r[0]))
        for (size_t k = 1; k < n; ++k)
          if (r[k] >= '0' && r[k] <= '9' && (!picLen || pic[k] == '9' || pic[k] != r[k])) r[k] = '*';
      o->p[0] = '(';
      put(o, ')');
      offset = 1;
    }
    if (*flags & P_DEBIT) {
      put(o, ' ');
      put(o, 'D');
      put(o, 'B');
    }
  } else if ((*flags & P_CREDIT) && d > 0) {
    put(o, ' ');
    put(o, 'C');
    put(o, 'R');
  }
  return offset;
}

/* A British date picture for SET DATE's: its D, M and Y runs in the order
 * day, month, year (DDMMYYYY with SET CENTURY, DDMMYY without), its other
 * characters kept. */
static void british_format(const xb_ctx *ctx, char fmt[XB_DATE_TEXT_SIZE]) {
  const char *order = ctx->env->century ? "DDMMYYYY" : "DDMMYY";
  const char *f = ctx->env->date_format;
  char last = 'x';
  size_t n = 0;
  while (n < XB_DATE_TEXT_SIZE - 1) {
    if (*order && *order == last) {
      fmt[n++] = *order++;
    } else if (!*f) {
      break;
    } else if (*order && strchr("YyDdMm", *f)) {
      fmt[n++] = last = *order++;
      do {
        ++f;
      } while (f[-1] == f[0]); /* the rest of the run */
    } else {
      fmt[n++] = *f++;
    }
  }
  fmt[n] = '\0';
}

static void transform_date(xb_ctx *ctx, const xb_item *value, unsigned flags, out *o) {
  char fmt[XB_DATE_TEXT_SIZE], text[XB_DATE_TEXT_SIZE];
  if (flags & P_BRITISH) {
    british_format(ctx, fmt);
  } else { /* the picture's first 10 characters, all a date's text takes */
    size_t n = strlen(ctx->env->date_format);
    if (n > sizeof fmt - 1) n = sizeof fmt - 1;
    memcpy(fmt, ctx->env->date_format, n);
    fmt[n] = '\0';
  }
  size_t n = xb_date_format(value->v.i, fmt, text);
  for (size_t i = 0; i < n; ++i) put(o, text[i]);
  if (flags & P_REMAIN) { /* the picture's separators are inserted where they stand */
    char digits[8], tmpl[XB_DATE_TEXT_SIZE];
    memset(digits, '9', sizeof digits);
    size_t t = xb_date_format_digits(digits, fmt, tmpl);
    for (size_t i = 0; i < t; ++i) {
      if (tmpl[i] == '9') continue;
      put(o, ' ');
      if (i < o->len - 1) memmove(o->p + i + 1, o->p + i, o->len - 1 - i);
      o->p[i] = tmpl[i];
    }
    if (o->len > 12) o->len = 12;
  }
}

static void transform_logical(xb_ctx *ctx, int value, const char *pic, size_t picLen, unsigned flags, out *o) {
  char datePic[XB_DATE_TEXT_SIZE];
  if (flags & (P_DATE | P_BRITISH)) {
    picLen = date_template(ctx, '9', datePic);
    pic = datePic;
  }
  int done = 0;
  for (size_t i = 0;; ++i) {
    int last = i >= picLen; /* past the template: the value itself, once */
    if (last && done) break;
    char p = last ? 'L' : pic[i];
    if (p == 'y' || p == 'Y') put(o, done ? ' ' : value ? 'Y' : 'N');
    else if (p == '#' || p == 'l' || p == 'L') put(o, done ? ' ' : value ? 'T' : 'F');
    else put(o, p);
    if (p == 'y' || p == 'Y' || p == '#' || p == 'l' || p == 'L') done = 1;
    if (last || !(flags & P_REMAIN)) break;
  }
}

void xbf_TRANSFORM(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_item nil;
  xb_set_nil(&nil);
  const xb_item *value = argc ? &argv[0] : &nil, *picture = argc > 1 ? &argv[1] : &nil;
  int kind = value->type == XB_T_STRING ? 1 : xb_is_number(value) ? 2 : value->type == XB_T_DATE ? 3
                                                                    : value->type == XB_T_LOGICAL ? 4 : 0;
  if (!kind || !(picture->type == XB_T_STRING || picture->type == XB_T_NIL)) {
    xb_fn_arg_error(ctx, ret, 1122, "TRANSFORM", argc < 2 ? argc : 2, argv);
    return;
  }
  if (picture->type == XB_T_NIL || picture->v.s->len == 0) { /* no picture: the value's plain text */
    if (kind == 1) *ret = *value;
    else if (kind == 4) xb_set_str(ret, xb_str_new(value->v.l ? "T" : "F", 1));
    else if (value->type == XB_T_INT && ctx->env->set_fixed) {
      int width, dec;
      xb_num_field(value, &width, &dec);
      xb_set_str(ret, xb_num_to_str(value, width + xb_num_fixed_int_extra(ctx), 0));
    } else xb_set_str(ret, xb_item_display(ctx, value));
    return;
  }
  const char *pic = picture->v.s->data;
  size_t picLen = picture->v.s->len;
  unsigned flags = 0;
  size_t width = 0; /* @S */
  if (*pic == '@') {
    ++pic;
    --picLen;
    while (picLen) {
      char f = *pic++;
      --picLen;
      if (f == ' ' || f == '\t') break; /* the template follows */
      switch (xb_ascii_upper(f)) {
      case '!': flags |= P_UPPER; break;
      case '(': flags |= P_PARENS; break;
      case ')': flags |= P_PARENS_TIGHT; break;
      case 'L': case '0': flags |= P_ZEROS; break;
      case 'B': flags |= P_LEFT; break;
      case 'C': flags |= P_CREDIT; break;
      case 'D': flags |= P_DATE; break;
      case 'E': flags |= P_BRITISH; break;
      case 'R': flags |= P_REMAIN; break;
      case 'X': flags |= P_DEBIT; break;
      case 'Z': flags |= P_BLANK; break;
      case 'S':
        width = 0;
        while (picLen && *pic >= '0' && *pic <= '9') {
          width = width * 10 + (size_t)(*pic++ - '0');
          --picLen;
        }
        break;
      default: break; /* T (times) and unknown letters change nothing */
      }
    }
  }
  out o = {0};
  size_t offset = 0;
  if (kind == 1) transform_string(ctx, value->v.s, pic, picLen, flags, &o);
  else if (kind == 2) offset = transform_number(ctx, value, pic, picLen, &flags, &o);
  else if (kind == 3) transform_date(ctx, value, flags, &o);
  else transform_logical(ctx, value->v.l != 0, pic, picLen, flags, &o);
  if (flags & P_BLANK) {
    if (o.len) memset(o.p, ' ', o.len);
  } else if (flags & P_LEFT) { /* leading blanks move to the end */
    size_t first = offset;
    while (first < o.len && o.p[first] == ' ') ++first;
    if (first > offset && first < o.len) {
      memmove(o.p + offset, o.p + first, o.len - first);
      memset(o.p + offset + o.len - first, ' ', first - offset);
    }
  }
  size_t n = width && o.len > width ? width : o.len;
  xb_set_str(ret, xb_str_new(o.p ? o.p : "", n));
  free(o.p);
}
