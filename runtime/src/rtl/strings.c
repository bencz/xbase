/* String functions of the language. Positions are 1-based and count bytes, as in Clipper; the case functions, the
 * character classes and the Char*() functions read characters as SET CODEPAGE says (core/codepage.h). */
#include "xbrt_int.h"

#include "core/codepage.h"

#include <string.h>

static const xb_str *str_arg(const xb_item *argv, uint32_t argc, uint32_t i) {
  return i < argc && argv[i].type == XB_T_STRING ? argv[i].v.s : NULL;
}

static int64_t num_arg(const xb_item *argv, uint32_t argc, uint32_t i, int64_t dflt) {
  return i < argc && xb_is_number(&argv[i]) ? xb_num_to_i64(&argv[i]) : dflt;
}

static void ret_str(xb_item *ret, const char *p, size_t n) { xb_set_str(ret, xb_str_new(p, n)); }

/* Returns `s` itself when the result is the whole string. */
static void ret_part(xb_item *ret, const xb_item *whole, size_t from, size_t n) {
  if (from == 0 && n == whole->v.s->len) *ret = *whole;
  else ret_str(ret, whole->v.s->data + from, n);
}

/* SubStr( c, nStart [, nCount] ) of `len` units (bytes or characters): the 0-based first unit of the part and how
 * many it takes (0: none). A negative start counts from the end. */
static int64_t substr_range(int64_t len, uint32_t argc, const xb_item *argv, int64_t *first) {
  int64_t start = xb_num_to_i64(&argv[1]);
  if (start < 0) start += len + 1;
  if (start < 1) start = 1;
  int64_t count = argc > 2 ? xb_num_to_i64(&argv[2]) : len;
  *first = start - 1;
  if (start > len || count <= 0) return 0;
  return count > len - start + 1 ? len - start + 1 : count;
}

/* SubStr and CharSubStr: in bytes, or in characters as the codepage reads them. */
static void substr(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, int chars, int32_t code, const char *fn) {
  const xb_str *s = str_arg(argv, argc, 0);
  if (!s || argc < 2 || !xb_is_number(&argv[1]) || (argc > 2 && !xb_is_number(&argv[2]))) {
    xb_fn_arg_error(ctx, ret, code, fn, argc, argv);
    return;
  }
  const xb_codepage *cp = xb_codepage_of(ctx);
  int64_t first, n = substr_range(chars ? (int64_t)xb_cp_count(cp, s->data, s->len) : (int64_t)s->len, argc, argv,
                                   &first);
  if (n == 0) {
    ret_str(ret, "", 0);
    return;
  }
  if (!chars) {
    ret_part(ret, &argv[0], (size_t)first, (size_t)n);
    return;
  }
  size_t from = xb_cp_offset(cp, s->data, s->len, (size_t)first);
  ret_part(ret, &argv[0], from, xb_cp_offset(cp, s->data + from, s->len - from, (size_t)n));
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
  if (n <= 0) {
    ret_str(ret, "", 0);
    return;
  }
  const xb_codepage *cp = xb_codepage_of(ctx);
  size_t units = chars ? xb_cp_count(cp, s->data, s->len) : s->len;
  size_t take = (uint64_t)n > units ? units : (size_t)n;
  if (!chars) {
    ret_part(ret, &argv[0], right ? s->len - take : 0, take);
    return;
  }
  size_t from = right ? xb_cp_offset(cp, s->data, s->len, units - take) : 0;
  ret_part(ret, &argv[0], from, right ? s->len - from : xb_cp_offset(cp, s->data, s->len, take));
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
  size_t i = 0;
  uint64_t high = 0; /* bytes above 0x7F: characters the codepage reads */
  for (; i + 8 <= s->len; i += 8) {
    uint64_t w;
    memcpy(&w, s->data + i, 8);
    high |= w;
    w = change_case8(w, upper);
    memcpy(r->data + i, &w, 8);
  }
  for (; i < s->len; ++i) {
    high |= (unsigned char)s->data[i];
    r->data[i] = upper ? xb_ascii_upper(s->data[i]) : xb_ascii_lower(s->data[i]);
  }
  if (high & UINT64_C(0x8080808080808080)) xb_cp_change_case(xb_codepage_of(ctx), r->data, r->len, upper);
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
  size_t from = 0, to = s->len;
  if (which & 1)
    while (from < to && xb_is_blank(s->data[from])) ++from;
  if (which & 2)
    while (to > from && s->data[to - 1] == ' ') --to;
  ret_part(ret, &argv[0], from, to - from);
}

void xbf_LTRIM(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  trim(ctx, ret, argc, argv, 1, 1101, "LTRIM");
}
void xbf_RTRIM(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) { trim(ctx, ret, argc, argv, 2, 1100, "TRIM"); }
void xbf_TRIM(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) { trim(ctx, ret, argc, argv, 2, 1100, "TRIM"); }
void xbf_ALLTRIM(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  trim(ctx, ret, argc, argv, 3, 2022, "ALLTRIM");
}

void xbf_SPACE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || !xb_is_number(&argv[0])) {
    xb_fn_arg_error(ctx, ret, 1105, "SPACE", argc, argv);
    return;
  }
  int64_t n = xb_num_to_i64(&argv[0]);
  xb_str *r = xb_str_alloc(n > 0 ? (uint64_t)n : 0);
  memset(r->data, ' ', r->len);
  xb_set_str(ret, r);
}

/* Whether a string result of `len` bytes can be made; the length is computed
 * in 64 bits and tested before it is narrowed to size_t (32-bit hosts). */
static int result_fits(xb_ctx *ctx, xb_item *ret, uint32_t argc, const xb_item *argv, uint64_t len,
                       const char *function) {
  if (len <= XB_STR_MAX_LEN) return 1;
  xb_error_in(ctx, function, ret, EG_STROVERFLOW, 1234, function, XB_ERR_SUBSTITUTE, argc, argv);
  return 0;
}

void xbf_REPLICATE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *s = str_arg(argv, argc, 0);
  if (!s || argc < 2 || !xb_is_number(&argv[1])) {
    xb_fn_arg_error(ctx, ret, 1106, "REPLICATE", argc, argv);
    return;
  }
  int64_t n = xb_num_to_i64(&argv[1]);
  uint64_t count = n > 0 && s->len ? (uint64_t)n : 0;
  if (!result_fits(ctx, ret, argc, argv, xb_mul_sat_u64(count, s->len), "REPLICATE")) return;
  size_t times = (size_t)count;
  xb_str *r = xb_str_alloc(s->len * times);
  for (size_t i = 0; i < times; ++i) memcpy(r->data + i * s->len, s->data, s->len);
  xb_set_str(ret, r);
}

/* 1-based position of the first (or last) occurrence, 0 if none. */
/* The 1-based position of `needle` in `hay` (the last one with `last`), 0 if
 * none: candidates are the places of its first byte (memchr going forward),
 * where the rest is compared. */
static size_t find(const xb_str *needle, const xb_str *hay, int last) {
  size_t n = needle->len;
  if (n == 0 || n > hay->len) return 0;
  const char *first = needle->data, *end = hay->data + hay->len - n + 1; /* candidates are before `end` */
  if (last) {
    for (const char *p = end; p-- > hay->data;)
      if (*p == *first && memcmp(p + 1, first + 1, n - 1) == 0) return (size_t)(p - hay->data) + 1;
    return 0;
  }
  for (const char *p = hay->data; p < end; ++p) {
    p = memchr(p, *first, (size_t)(end - p));
    if (!p) return 0;
    if (memcmp(p + 1, first + 1, n - 1) == 0) return (size_t)(p - hay->data) + 1;
  }
  return 0;
}

/* At( cSearch, cTarget ): position of the first occurrence, 0 if none. */
void xbf_AT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *needle = str_arg(argv, argc, 0), *hay = str_arg(argv, argc, 1);
  if (!needle || !hay) {
    xb_fn_arg_error(ctx, ret, 1108, "AT", argc, argv);
    return;
  }
  xb_set_int(ret, (int64_t)find(needle, hay, 0));
}

void xbf_RAT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *needle = str_arg(argv, argc, 0), *hay = str_arg(argv, argc, 1);
  XB_UNUSED(ctx);
  xb_set_int(ret, needle && hay ? (int64_t)find(needle, hay, 1) : 0);
}

void xbf_CHR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || !xb_is_number(&argv[0])) {
    xb_fn_arg_error(ctx, ret, 1104, "CHR", argc, argv);
    return;
  }
  char c = (char)(unsigned char)(xb_num_to_i64(&argv[0]) & 0xFF);
  ret_str(ret, &c, 1);
}

void xbf_ASC(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *s = str_arg(argv, argc, 0);
  if (!s) {
    xb_fn_arg_error(ctx, ret, 1107, "ASC", argc, argv);
    return;
  }
  xb_set_int(ret, s->len ? (unsigned char)s->data[0] : 0);
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
  const char *rp = repl ? repl->data : "";
  size_t rlen = repl ? repl->len : 0;
  int64_t start = num_arg(argv, argc, 3, 1);
  int64_t count = num_arg(argv, argc, 4, -1);
  if (start == 0 || count == 0) {
    ret_str(ret, "", 0);
    return;
  }
  if (search->len == 0 || start < 0) {
    *ret = argv[0];
    return;
  }
  /* First pass: count replacements, to size the result. */
  size_t hits = 0;
  int64_t found = 0;
  for (size_t i = 0; i + search->len <= s->len;) {
    if (memcmp(s->data + i, search->data, search->len) == 0) {
      ++found;
      if (found >= start && (count < 0 || (int64_t)hits < count)) ++hits;
      i += search->len;
    } else {
      ++i;
    }
  }
  if (!hits) {
    *ret = argv[0];
    return;
  }
  uint64_t added = xb_mul_sat_u64(hits, rlen); /* the removed search texts fit in s->len */
  uint64_t len = added > UINT64_MAX - s->len ? UINT64_MAX : s->len - (uint64_t)hits * search->len + added;
  if (!result_fits(ctx, ret, argc, argv, len, "STRTRAN")) return;
  xb_str *r = xb_str_alloc(len);
  size_t o = 0, done = 0;
  found = 0;
  for (size_t i = 0; i < s->len;) {
    if (i + search->len <= s->len && memcmp(s->data + i, search->data, search->len) == 0) {
      ++found;
      if (found >= start && done < hits) {
        memcpy(r->data + o, rp, rlen);
        o += rlen;
        ++done;
      } else {
        memcpy(r->data + o, s->data + i, search->len);
        o += search->len;
      }
      i += search->len;
    } else {
      r->data[o++] = s->data[i++];
    }
  }
  xb_set_str(ret, r);
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
  memcpy(r->data, s->data, (size_t)(start - 1));
  if (ilen) memcpy(r->data + start - 1, ins->data, ilen);
  memcpy(r->data + (size_t)(start - 1) + ilen, s->data + (size_t)(start - 1 + del), len - (size_t)(start - 1 + del));
  xb_set_str(ret, r);
}

/* Whether the first character is a letter (0), a digit (1), an uppercase (2) or a lowercase letter (3); one with a
 * byte above 0x7F as the codepage says (digits are ASCII). */
static void char_class(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, int which) {
  const xb_str *s = str_arg(argv, argc, 0);
  char c = s && s->len ? s->data[0] : 0;
  int r = 0;
  if ((unsigned char)c >= 0x80) {
    int kind = xb_cp_kind(xb_codepage_of(ctx), s->data, s->len);
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
      case XB_T_STRING:
        for (size_t i = 0; i < it->v.s->len && empty; ++i) empty = xb_is_blank(it->v.s->data[i]);
        break;
      case XB_T_ARRAY: empty = ((const xb_array *)it->v.p)->len == 0; break;
      case XB_T_HASH: empty = ((const xb_hash *)it->v.p)->len == 0; break;
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
  char with = fill && fill->len ? fill->data[0] : ' ';
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
  const char *p = text.v.s->data;
  size_t len = text.v.s->len;
  if (xb_is_number(&argv[0])) {
    while (len && *p == ' ') {
      ++p;
      --len;
    }
  }
  size_t width = n > 0 ? (size_t)(n < (int64_t)XB_STR_MAX_LEN ? n : (int64_t)XB_STR_MAX_LEN) : 0;
  xb_str *r = xb_str_alloc(width);
  if (len >= width) {
    memcpy(r->data, p, width);
  } else {
    size_t extra = width - len, before = side == PAD_LEFT ? extra : side == PAD_CENTER ? extra / 2 : 0;
    memset(r->data, with, width);
    memcpy(r->data + before, p, len);
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
  for (size_t i = 0; s && i < s->len && at < sizeof code; ++i) {
    char c = xb_ascii_upper(s->data[i]);
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
  xb_set_int(ret, (int64_t)xb_cp_count(xb_codepage_of(ctx), s->data, s->len));
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
  for (size_t i = 0; i + needle->len <= hay->len; ++chars) { /* a match starts where a character does */
    if (memcmp(hay->data + i, needle->data, needle->len) == 0) {
      xb_set_int(ret, chars);
      return;
    }
    size_t n;
    xb_cp_decode(cp, hay->data, hay->len, i, &n);
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
  xb_set_int(ret, s->len ? (int64_t)xb_cp_decode(xb_codepage_of(ctx), s->data, s->len, 0, &n) : 0);
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
  const xb_codepage *from = fromName ? xb_codepage_find(fromName->data, fromName->len) : NULL;
  const xb_codepage *to = toName ? xb_codepage_find(toName->data, toName->len) : NULL;
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
      size_t m = xb_cp_encode(to, xb_cp_decode(from, s->data, s->len, i, &n), put);
      if (!m) {
        put[0] = '?';
        m = 1;
      }
      if (r) memcpy(r->data + (size_t)at, put, m);
      at += m;
      i += n;
    }
    if (r) xb_set_str(ret, r);
    len = at;
  }
}
