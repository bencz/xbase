/* String functions of the language. Positions are 1-based and count bytes, as in Clipper; the case functions, the
 * character classes and the Char*() functions read characters as SET CODEPAGE says (core/codepage.h). */
#include "xbrt_int.h"

#include "core/codepage.h"

#include <string.h>

static const xb_str *str_arg(const xb_item *argv, uint32_t argc, uint32_t i) {
  return i < argc && argv[i].type == XB_T_STRING ? argv[i].v.s : NULL;
}

static void ret_str(xb_item *ret, const char *p, size_t n) { xb_set_str(ret, xb_str_new(p, n)); }

/* The bytes from `from` of `s`, `n` of them: `s` itself when that is all of it. */
static xb_str *part(xb_str *s, size_t from, size_t n) {
  return from == 0 && n == s->len ? s : xb_str_new(xb_str_data(s) + from, n);
}

/* A part of the string `whole` holds: the item itself when it is all of it (a memo field's value stays one). */
static void ret_result(xb_item *ret, const xb_item *whole, xb_str *r) {
  if (r == whole->v.s) *ret = *whole;
  else xb_set_str(ret, r);
}

static void ret_part(xb_item *ret, const xb_item *whole, size_t from, size_t n) {
  ret_result(ret, whole, part(whole->v.s, from, n));
}

xb_str *xb_str_left(xb_str *s, int64_t n) { return part(s, 0, xb_take_count(n, s->len)); }

xb_str *xb_str_right(xb_str *s, int64_t n) {
  size_t take = xb_take_count(n, s->len);
  return part(s, s->len - take, take);
}

xb_str *xb_str_sub(xb_str *s, int64_t start, int has_count, int64_t count) {
  int64_t first, n = xb_substr_range((int64_t)s->len, start, has_count, count, &first);
  return n ? part(s, (size_t)first, (size_t)n) : xb_str_new("", 0);
}

/* SubStr( c, nStart [, nCount] ) of `len` units (bytes or characters): xb_substr_range. */
static int64_t substr_range(int64_t len, uint32_t argc, const xb_item *argv, int64_t *first) {
  return xb_substr_range(len, xb_num_to_i64(&argv[1]), argc > 2, argc > 2 ? xb_num_to_i64(&argv[2]) : 0, first);
}

/* SubStr and CharSubStr: in bytes, or in characters as the codepage reads them. */
static void substr(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, int chars, int32_t code, const char *fn) {
  const xb_str *s = str_arg(argv, argc, 0);
  if (!s || argc < 2 || !xb_is_number(&argv[1]) || (argc > 2 && !xb_is_number(&argv[2]))) {
    xb_fn_arg_error(ctx, ret, code, fn, argc, argv);
    return;
  }
  if (!chars) {
    int64_t count = argc > 2 ? xb_num_to_i64(&argv[2]) : 0;
    ret_result(ret, &argv[0], xb_str_sub(argv[0].v.s, xb_num_to_i64(&argv[1]), argc > 2, count));
    return;
  }
  const xb_codepage *cp = xb_codepage_of(ctx);
  int64_t first, n = substr_range((int64_t)xb_cp_count(cp, xb_str_data(s), s->len), argc, argv, &first);
  if (n == 0) {
    ret_str(ret, "", 0);
    return;
  }
  size_t from = xb_cp_offset(cp, xb_str_data(s), s->len, (size_t)first);
  ret_part(ret, &argv[0], from, xb_cp_offset(cp, xb_str_data(s) + from, s->len - from, (size_t)n));
}

void xbf_SUBSTR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  substr(ctx, ret, argc, argv, 0, 1110, "SUBSTR");
}

/* Left and Right (and CharLeft, CharRight): the first or last `n` bytes, or characters as the codepage reads them.
 * Left needs a string and a count; Right returns "" for anything else. */
static void left_right(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, int right, int chars, int code,
                       const char *fn) {
  const xb_str *s = str_arg(argv, argc, 0);
  if (!s || argc < 2 || !xb_is_number(&argv[1])) {
    if (right) ret_str(ret, "", 0);
    else xb_fn_arg_error(ctx, ret, code, fn, argc, argv);
    return;
  }
  int64_t n = xb_num_to_i64(&argv[1]);
  if (!chars) {
    ret_result(ret, &argv[0], right ? xb_str_right(argv[0].v.s, n) : xb_str_left(argv[0].v.s, n));
    return;
  }
  if (n <= 0) {
    ret_str(ret, "", 0);
    return;
  }
  const xb_codepage *cp = xb_codepage_of(ctx);
  size_t units = xb_cp_count(cp, xb_str_data(s), s->len);
  size_t take = xb_take_count(n, units);
  size_t from = right ? xb_cp_offset(cp, xb_str_data(s), s->len, units - take) : 0;
  ret_part(ret, &argv[0], from, right ? s->len - from : xb_cp_offset(cp, xb_str_data(s), s->len, take));
}

void xbf_LEFT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  left_right(ctx, ret, argc, argv, 0, 0, 1124, "LEFT");
}
void xbf_RIGHT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  left_right(ctx, ret, argc, argv, 1, 0, 1124, "RIGHT");
}

/* Eight bytes with the ASCII letters of one case changed to the other (to
 * upper case with `upper`), as xb_ascii_upper and xb_ascii_lower change one:
 * the high bit of each byte tells whether it is in the letters' range, and
 * no byte carries into the next, so the byte order does not matter. */
static uint64_t change_case8(uint64_t w, int upper) {
  const uint64_t ones = UINT64_C(0x0101010101010101), high = UINT64_C(0x8080808080808080);
  unsigned first = upper ? 'a' : 'A', last = upper ? 'z' : 'Z';
  uint64_t low7 = w & ~high;                       /* each byte below 0x80: sums stay within it */
  uint64_t atLeast = low7 + ones * (0x80u - first); /* high bit: the byte is `first` or above */
  uint64_t above = low7 + ones * (0x7Fu - last);    /* high bit: the byte is above `last` */
  uint64_t letters = atLeast & ~above & ~w & high;  /* ASCII bytes in first..last */
  return upper ? w - (letters >> 2) : w + (letters >> 2); /* 0x20 in each such byte */
}

static void change_case(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, int upper, const char *fn) {
  const xb_str *s = str_arg(argv, argc, 0);
  if (!s) {
    xb_fn_arg_error(ctx, ret, upper ? 1102 : 1103, fn, argc, argv);
    return;
  }
  xb_str *r = xb_str_alloc(s->len);
  const char *p = xb_str_data(s);
  char *out = xb_str_buffer(r);
  size_t i = 0;
  uint64_t high = 0; /* bytes above 0x7F: characters the codepage reads */
  for (; i + 8 <= s->len; i += 8) {
    uint64_t w;
    memcpy(&w, p + i, 8);
    high |= w;
    w = change_case8(w, upper);
    memcpy(out + i, &w, 8);
  }
  for (; i < s->len; ++i) {
    high |= (unsigned char)p[i];
    out[i] = upper ? xb_ascii_upper(p[i]) : xb_ascii_lower(p[i]);
  }
  if (high & UINT64_C(0x8080808080808080)) xb_cp_change_case(xb_codepage_of(ctx), xb_str_buffer(r), r->len, upper);
  xb_set_str(ret, r);
}

void xbf_UPPER(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  change_case(ctx, ret, argc, argv, 1, "UPPER");
}
void xbf_LOWER(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  change_case(ctx, ret, argc, argv, 0, "LOWER");
}

/* which: 1 leading, 2 trailing, 3 both. Trailing trimming removes spaces only. */
static void trim(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, int which, int code, const char *fn) {
  const xb_str *s = str_arg(argv, argc, 0);
  if (!s) {
    xb_fn_arg_error(ctx, ret, code, fn, argc, argv);
    return;
  }
  size_t from, n = xb_trim_range(xb_str_data(s), s->len, which, &from);
  ret_part(ret, &argv[0], from, n);
}

void xbf_LTRIM(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  trim(ctx, ret, argc, argv, XB_TRIM_LEADING, 1101, "LTRIM");
}
void xbf_RTRIM(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  trim(ctx, ret, argc, argv, XB_TRIM_TRAILING, 1100, "TRIM");
}
void xbf_TRIM(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  trim(ctx, ret, argc, argv, XB_TRIM_TRAILING, 1100, "TRIM");
}
void xbf_ALLTRIM(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  trim(ctx, ret, argc, argv, XB_TRIM_LEADING | XB_TRIM_TRAILING, 2022, "ALLTRIM");
}

/* Whether a string result of `len` bytes can be made; the length is computed
 * in 64 bits and tested before it is narrowed to size_t (32-bit hosts). */
static int result_fits(xb_ctx *ctx, xb_item *ret, uint32_t argc, const xb_item *argv, uint64_t len,
                       const char *function) {
  if (len <= XB_STR_MAX_LEN) return 1;
  xb_error_overflow(ctx, ret, function, argc, argv);
  return 0;
}

/* A string result of `len` bytes (a size the program chose): NULL, with the error BASE/9032 Memory low raised (which
 * the program may handle), when there is no memory for it; the string overflow when it is past the largest string. */
static xb_str *result_alloc(xb_ctx *ctx, xb_item *ret, uint32_t argc, const xb_item *argv, uint64_t len,
                            const char *function) {
  if (!result_fits(ctx, ret, argc, argv, len, function)) return NULL;
  xb_str *r = xb_str_try_alloc(len);
  if (!r) xb_error_memory(ctx, ret, function, argc, argv);
  return r;
}

void xbf_SPACE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || !xb_is_number(&argv[0])) {
    xb_fn_arg_error(ctx, ret, 1105, "SPACE", argc, argv);
    return;
  }
  xb_str *r = result_alloc(ctx, ret, argc, argv, xb_count_arg(xb_num_to_i64(&argv[0])), "SPACE");
  if (!r) return;
  memset(xb_str_buffer(r), ' ', r->len);
  xb_set_str(ret, r);
}

void xbf_REPLICATE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *s = str_arg(argv, argc, 0);
  if (!s || argc < 2 || !xb_is_number(&argv[1])) {
    xb_fn_arg_error(ctx, ret, 1106, "REPLICATE", argc, argv);
    return;
  }
  uint64_t count = s->len ? xb_count_arg(xb_num_to_i64(&argv[1])) : 0;
  xb_str *r = result_alloc(ctx, ret, argc, argv, xb_mul_sat_u64(count, s->len), "REPLICATE");
  if (!r) return;
  size_t total = r->len;
  if (s->len == 1) {
    memset(xb_str_buffer(r), xb_str_data(s)[0], total);
  } else if (total) { /* the text once, then what is written so far copied after itself: log2(count) copies */
    memcpy(xb_str_buffer(r), xb_str_data(s), s->len);
    for (size_t done = s->len; done < total; done *= 2)
      memcpy(xb_str_buffer(r) + done, xb_str_buffer(r), done < total - done ? done : total - done);
  }
  xb_set_str(ret, r);
}

/* At( cSearch, cTarget ): position of the first occurrence, 0 if none. */
void xbf_AT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *needle = str_arg(argv, argc, 0), *hay = str_arg(argv, argc, 1);
  if (!needle || !hay) {
    xb_fn_arg_error(ctx, ret, 1108, "AT", argc, argv);
    return;
  }
  xb_set_int(ret, (int64_t)xb_bytes_find(xb_str_data(needle), needle->len, xb_str_data(hay), hay->len, 0));
}

void xbf_RAT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *needle = str_arg(argv, argc, 0), *hay = str_arg(argv, argc, 1);
  XB_UNUSED(ctx);
  xb_set_int(ret, needle && hay
                      ? (int64_t)xb_bytes_find(xb_str_data(needle), needle->len, xb_str_data(hay), hay->len, 1)
                      : 0);
}

void xbf_CHR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || !xb_is_number(&argv[0])) {
    xb_fn_arg_error(ctx, ret, 1104, "CHR", argc, argv);
    return;
  }
  char c = xb_chr_byte(xb_num_to_i64(&argv[0]));
  ret_str(ret, &c, 1);
}

void xbf_ASC(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *s = str_arg(argv, argc, 0);
  if (!s) {
    xb_fn_arg_error(ctx, ret, 1107, "ASC", argc, argv);
    return;
  }
  xb_set_int(ret, xb_asc_code(xb_str_data(s), s->len));
}

/* StrTran( c, cSearch [, cReplace] [, nStart] [, nCount] ): nStart is the
 * first occurrence replaced, nCount how many; either one 0 gives "". */
void xbf_STRTRAN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *s = str_arg(argv, argc, 0), *search = str_arg(argv, argc, 1);
  if (!s || !search) {
    xb_fn_arg_error(ctx, ret, 1126, "STRTRAN", argc, argv);
    return;
  }
  const xb_str *repl = str_arg(argv, argc, 2);
  const char *rp = repl ? xb_str_data(repl) : "";
  size_t rlen = repl ? repl->len : 0;
  int64_t start = xb_num_arg(argc, argv, 3, 1);
  int64_t count = xb_num_arg(argc, argv, 4, -1);
  if (start == 0 || count == 0) {
    ret_str(ret, "", 0);
    return;
  }
  if (search->len == 0 || start < 0) {
    *ret = argv[0];
    return;
  }
  /* First pass: count replacements, to size the result (each search from where the last match ended: xb_bytes_find,
   * linear whatever the texts). */
  size_t hits = 0, n = search->len;
  int64_t found = 0;
  for (size_t i = 0, at; (at = xb_bytes_find(xb_str_data(search), n, xb_str_data(s) + i, s->len - i, 0)) != 0;
       i += at - 1 + n) {
    ++found;
    if (found >= start && (count < 0 || (int64_t)hits < count)) ++hits;
  }
  if (!hits) {
    *ret = argv[0];
    return;
  }
  uint64_t added = xb_mul_sat_u64(hits, rlen); /* the removed search texts fit in s->len */
  uint64_t len = added > UINT64_MAX - s->len ? UINT64_MAX : s->len - (uint64_t)hits * n + added;
  xb_str *r = result_alloc(ctx, ret, argc, argv, len, "STRTRAN");
  if (!r) return;
  size_t o = 0, done = 0, i = 0;
  found = 0;
  for (size_t at;
       done < hits && (at = xb_bytes_find(xb_str_data(search), n, xb_str_data(s) + i, s->len - i, 0)) != 0;) {
    size_t run = at - 1; /* the bytes before the match, copied whole */
    memcpy(xb_str_buffer(r) + o, xb_str_data(s) + i, run);
    o += run;
    i += run;
    int replaced = ++found >= start;
    memcpy(xb_str_buffer(r) + o, replaced ? rp : xb_str_data(s) + i, replaced ? rlen : n);
    o += replaced ? rlen : n;
    done += (size_t)replaced;
    i += n;
  }
  memcpy(xb_str_buffer(r) + o, xb_str_data(s) + i, s->len - i); /* the rest */
  xb_set_str(ret, r);
}

/* __Affix( cText, cPart, lEnd ): whether cText starts (ends, with lEnd) with cPart, compared in place
 * (CHARACTER:StartsWith and EndsWith: no substring made). */
void xbf___AFFIX(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *s = str_arg(argv, argc, 0), *part = str_arg(argv, argc, 1);
  int end = argc > 2 && argv[2].type == XB_T_LOGICAL && argv[2].v.l;
  if (!s || !part) {
    xb_fn_arg_error(ctx, ret, XB_CODE_AFFIX, end ? "CHARACTER:ENDSWITH" : "CHARACTER:STARTSWITH", argc, argv);
    return;
  }
  xb_set_logical(ret, part->len <= s->len &&
                          memcmp(xb_str_data(s) + (end ? s->len - part->len : 0), xb_str_data(part), part->len) == 0);
}

/* __Split( cText, cSeparator ): the pieces of cText between the separators (a separator of one byte or more), an
 * array: one pass, each separator looked for from where the last one ended (CHARACTER:Split). */
void xbf___SPLIT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *s = str_arg(argv, argc, 0), *sep = str_arg(argv, argc, 1);
  if (!s || !sep || !sep->len) {
    xb_fn_arg_error(ctx, ret, XB_CODE_SPLIT, "CHARACTER:SPLIT", argc, argv);
    return;
  }
  size_t pieces = 1, n = sep->len;
  const char *by = xb_str_data(sep);
  for (size_t i = 0, at; (at = xb_bytes_find(by, n, xb_str_data(s) + i, s->len - i, 0)) != 0; i += at - 1 + n) ++pieces;
  xb_array *a = xb_array_alloc(pieces); /* rooted in `ret`; `s` and `sep`, pinned by the stack, stay where they are */
  xb_set_array(ret, a);
  size_t i = 0;
  for (size_t k = 0; k < pieces; ++k) {
    size_t at = k + 1 < pieces ? xb_bytes_find(xb_str_data(sep), n, xb_str_data(s) + i, s->len - i, 0) : s->len - i + 1;
    xb_str *piece = xb_str_new(xb_str_data(s) + i, at - 1);
    a = ret->v.p; /* the array may have moved */
    xb_set_str(&xb_array_items(a)[k], piece);
    xb_gc_write_item(a, k);
    i += at - 1 + n;
  }
}

/* Stuff( c, nStart, nDelete, cInsert ): any other argument types give "".
 * A start of 0 is 1; a negative start or count counts as past the end. */
void xbf_STUFF(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  const xb_str *s = str_arg(argv, argc, 0), *ins = str_arg(argv, argc, 3);
  if (!s || !ins || !xb_is_number(&argv[1]) || !xb_is_number(&argv[2])) {
    ret_str(ret, "", 0);
    return;
  }
  size_t len = s->len;
  int64_t start = xb_num_to_i64(&argv[1]), del = xb_num_to_i64(&argv[2]);
  if (start == 0) start = 1;
  if (start < 0 || (uint64_t)start > len + 1) start = (int64_t)len + 1;
  if (del < 0 || (uint64_t)del > len - (size_t)(start - 1)) del = (int64_t)(len - (size_t)(start - 1));
  size_t ilen = ins->len;
  xb_str *r = xb_str_alloc(len - (size_t)del + ilen);
  memcpy(xb_str_buffer(r), xb_str_data(s), (size_t)(start - 1));
  if (ilen) memcpy(xb_str_buffer(r) + start - 1, xb_str_data(ins), ilen);
  size_t rest = (size_t)(start - 1 + del); /* the first byte kept after the deleted ones */
  memcpy(xb_str_buffer(r) + (size_t)(start - 1) + ilen, xb_str_data(s) + rest, len - rest);
  xb_set_str(ret, r);
}

/* Whether the first character is a letter (0), a digit (1), an uppercase (2) or a lowercase letter (3); one with a
 * byte above 0x7F as the codepage says (digits are ASCII). */
static void char_class(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, int which) {
  const xb_str *s = str_arg(argv, argc, 0);
  char c = s && s->len ? xb_str_data(s)[0] : 0;
  int r = 0;
  if ((unsigned char)c >= 0x80) {
    int kind = xb_cp_kind(xb_codepage_of(ctx), xb_str_data(s), s->len);
    r = which == 0 ? kind != XB_CP_NONE : which == 2 ? kind == XB_CP_UPPER : which == 3 && kind == XB_CP_LOWER;
  } else if (which == 0) {
    r = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  } else if (which == 1) {
    r = c >= '0' && c <= '9';
  } else if (which == 2) {
    r = c >= 'A' && c <= 'Z';
  } else {
    r = c >= 'a' && c <= 'z';
  }
  xb_set_logical(ret, r);
}

void xbf_ISALPHA(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  char_class(ctx, ret, argc, argv, 0);
}
void xbf_ISDIGIT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  char_class(ctx, ret, argc, argv, 1);
}
void xbf_ISUPPER(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  char_class(ctx, ret, argc, argv, 2);
}
void xbf_ISLOWER(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  char_class(ctx, ret, argc, argv, 3);
}

/* Empty( x ): NIL, 0, "" or blanks only, .F., an empty array, the empty date. */
void xbf_EMPTY(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  int empty = 1;
  if (argc >= 1) {
    const xb_item *it = &argv[0];
    switch ((xb_type)it->type) {
      case XB_T_NIL: empty = 1; break;
      case XB_T_LOGICAL: empty = !it->v.l; break;
      case XB_T_INT: empty = it->v.i == 0; break;
      case XB_T_DOUBLE: empty = it->v.d == 0.0; break;
      case XB_T_STRING: {
        size_t from;
        empty = xb_trim_range(xb_str_data(it->v.s), it->v.s->len, XB_TRIM_LEADING, &from) == 0;
        break;
      }
      case XB_T_ARRAY: empty = xb_array_length(it->v.p) == 0; break;
      case XB_T_HASH: empty = xb_hash_length(it->v.p) == 0; break;
      case XB_T_DATE: empty = it->v.i == 0; break;
      default: empty = 0; break;
    }
  }
  xb_set_logical(ret, empty);
}

/* PadL/PadR/PadC( xValue, nLength, [cFill] ): the value's text padded with
 * the fill's first character (a blank by default) to nLength, or its first
 * nLength characters: a string as it is, a number as Str() gives it without
 * its leading blanks, a date as DToC() does; "" for any other value. PadL
 * pads on the left, PadC around (the extra half, rounded down, on the
 * left). */
typedef enum pad_side { PAD_RIGHT, PAD_LEFT, PAD_CENTER } pad_side;

static void pad(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, pad_side side) {
  int64_t n = argc > 1 && xb_is_number(&argv[1]) ? xb_num_to_i64(&argv[1]) : 0;
  const xb_str *fill = str_arg(argv, argc, 2);
  char with = fill && fill->len ? xb_str_data(fill)[0] : ' ';
  if (!argc || !(argv[0].type == XB_T_STRING || xb_is_number(&argv[0]) || argv[0].type == XB_T_DATE)) {
    xb_set_str(ret, xb_str_empty());
    return;
  }
  if (argv[0].type == XB_T_STRING && n > 0 && (uint64_t)n == argv[0].v.s->len) { /* already that long */
    *ret = argv[0];
    return;
  }
  xb_item text; /* the value's text, rooted while the result is made */
  xb_set_str(&text, xb_item_display(ctx, &argv[0]));
  xb_gc_root_push(ctx, &text);
  const char *p = xb_str_data(text.v.s);
  size_t len = text.v.s->len;
  if (xb_is_number(&argv[0])) {
    while (len && *p == ' ') {
      ++p;
      --len;
    }
  }
  size_t width = n > 0 ? (size_t)(n < (int64_t)XB_STR_MAX_LEN ? n : (int64_t)XB_STR_MAX_LEN) : 0;
  const char *function = side == PAD_LEFT ? "PADL" : side == PAD_CENTER ? "PADC" : "PADR";
  xb_str *r = result_alloc(ctx, ret, argc, argv, width, function);
  if (!r) {
    xb_gc_root_pop(ctx, 1);
    return;
  }
  if (len >= width) {
    memcpy(xb_str_buffer(r), p, width);
  } else {
    size_t extra = width - len, before = side == PAD_LEFT ? extra : side == PAD_CENTER ? extra / 2 : 0;
    memset(xb_str_buffer(r), with, width);
    memcpy(xb_str_buffer(r) + before, p, len);
  }
  xb_gc_root_pop(ctx, 1);
  xb_set_str(ret, r);
}

void xbf_PADR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) { pad(ctx, ret, argc, argv, PAD_RIGHT); }
void xbf_PAD(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) { pad(ctx, ret, argc, argv, PAD_RIGHT); }
void xbf_PADL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) { pad(ctx, ret, argc, argv, PAD_LEFT); }
void xbf_PADC(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) { pad(ctx, ret, argc, argv, PAD_CENTER); }

/* SoundEx( cString ): the string's four-character Soundex code: its first
 * letter, then the digits of the letters after it (B F P V 1, C G J K Q S X
 * Z 2, D T 3, L 4, M N 5, R 6), none for A E I O U H W Y, which only part two
 * letters of the same digit, and a digit once for letters next to each other
 * with it; anything but a letter is skipped; zeros fill the rest. */
void xbf_SOUNDEX(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  static const char digits[] = "01230120022455012623010202"; /* A .. Z */
  char code[4] = {'0', '0', '0', '0'};
  const xb_str *s = str_arg(argv, argc, 0);
  size_t at = 0;
  char prev = '0';
  const char *p = s ? xb_str_data(s) : NULL;
  for (size_t i = 0; s && i < s->len && at < sizeof code; ++i) {
    char c = xb_ascii_upper(p[i]);
    if (c < 'A' || c > 'Z') continue;
    char d = digits[c - 'A'];
    if (at == 0) code[at++] = c;
    else if (d != '0' && d != prev) code[at++] = d;
    prev = d;
  }
  ret_str(ret, code, sizeof code);
}

/* The Char*() functions, xbase's own: what Len(), SubStr(), Left(), Right(), At(), Asc() and Chr() do for bytes, for
 * characters as SET CODEPAGE reads them (one per byte in a single-byte codepage; in UTF-8, a byte that starts no
 * valid sequence counts as one character, U+FFFD). Their argument errors have xbase's own codes. */
enum { CODE_CHARLEN = 9009, CODE_CHARSUBSTR, CODE_CHARLEFT, CODE_CHARAT, CODE_CHARASC, CODE_CHARCHR, CODE_CPCONVERT };
/* (9001 to 9008 are the bit functions', xb_bit_function in xbrt.h.) */

/* CharLen( cString ): its characters. */
void xbf_CHARLEN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *s = str_arg(argv, argc, 0);
  if (!s) {
    xb_fn_arg_error(ctx, ret, CODE_CHARLEN, "CHARLEN", argc, argv);
    return;
  }
  xb_set_int(ret, (int64_t)xb_cp_count(xb_codepage_of(ctx), xb_str_data(s), s->len));
}

/* CharSubStr( cString, nStart, [nCount] ). */
void xbf_CHARSUBSTR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  substr(ctx, ret, argc, argv, 1, CODE_CHARSUBSTR, "CHARSUBSTR");
}

/* CharLeft( cString, nCount ), CharRight( cString, nCount ). */
void xbf_CHARLEFT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  left_right(ctx, ret, argc, argv, 0, 1, CODE_CHARLEFT, "CHARLEFT");
}
void xbf_CHARRIGHT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  left_right(ctx, ret, argc, argv, 1, 1, 0, "CHARRIGHT");
}

/* CharAt( cSearch, cString ): the character where cSearch first starts in cString (a match must start where a
 * character does), 0 when it does not. */
void xbf_CHARAT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *needle = str_arg(argv, argc, 0), *hay = str_arg(argv, argc, 1);
  if (!needle || !hay) {
    xb_fn_arg_error(ctx, ret, CODE_CHARAT, "CHARAT", argc, argv);
    return;
  }
  const xb_codepage *cp = xb_codepage_of(ctx);
  int64_t chars = 1;
  xb_set_int(ret, 0);
  if (needle->len == 0) return;
  const char *h = xb_str_data(hay), *find = xb_str_data(needle);
  for (size_t i = 0; i + needle->len <= hay->len; ++chars) { /* a match starts where a character does */
    if (memcmp(h + i, find, needle->len) == 0) {
      xb_set_int(ret, chars);
      return;
    }
    size_t n;
    xb_cp_decode(cp, h, hay->len, i, &n);
    i += n;
  }
}

/* CharAsc( cString ): the code point of its first character, 0 for "". */
void xbf_CHARASC(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *s = str_arg(argv, argc, 0);
  if (!s) {
    xb_fn_arg_error(ctx, ret, CODE_CHARASC, "CHARASC", argc, argv);
    return;
  }
  size_t n;
  xb_set_int(ret, s->len ? (int64_t)xb_cp_decode(xb_codepage_of(ctx), xb_str_data(s), s->len, 0, &n) : 0);
}

/* CharChr( nCode ): the character of a code point, "" when the codepage has none (or it is none). */
void xbf_CHARCHR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || !xb_is_number(&argv[0])) {
    xb_fn_arg_error(ctx, ret, CODE_CHARCHR, "CHARCHR", argc, argv);
    return;
  }
  int64_t code = xb_num_to_i64(&argv[0]);
  char out[4];
  size_t n = code >= 0 && code <= 0x10FFFF ? xb_cp_encode(xb_codepage_of(ctx), (uint32_t)code, out) : 0;
  ret_str(ret, out, n);
}

/* CPConvert( cText, cFrom, cTo ): the text read in codepage cFrom, written in codepage cTo; a character cTo has not
 * becomes "?" (an invalid UTF-8 byte reads as U+FFFD). An unknown codepage, or an argument that is no string, is an
 * argument error. */
void xbf_CPCONVERT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *s = str_arg(argv, argc, 0), *fromName = str_arg(argv, argc, 1), *toName = str_arg(argv, argc, 2);
  const xb_codepage *from = fromName ? xb_codepage_find(xb_str_data(fromName), fromName->len) : NULL;
  const xb_codepage *to = toName ? xb_codepage_find(xb_str_data(toName), toName->len) : NULL;
  if (!s || !from || !to) {
    xb_fn_arg_error(ctx, ret, CODE_CPCONVERT, "CPCONVERT", argc, argv);
    return;
  }
  if (from == to) {
    *ret = argv[0];
    return;
  }
  uint64_t len = 0; /* the first pass measures (xb_str_alloc checks the length), the second writes */
  for (int pass = 0; pass < 2; ++pass) {
    xb_str *r = pass ? xb_str_alloc(len) : NULL;
    uint64_t at = 0;
    for (size_t i = 0; i < s->len;) {
      size_t n;
      char put[4];
      size_t m = xb_cp_encode(to, xb_cp_decode(from, xb_str_data(s), s->len, i, &n), put);
      if (!m) {
        put[0] = '?';
        m = 1;
      }
      if (r) memcpy(xb_str_buffer(r) + (size_t)at, put, m);
      at += m;
      i += n;
    }
    if (r) xb_set_str(ret, r);
    len = at;
  }
}
