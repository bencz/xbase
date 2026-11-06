/* String functions of the language. Positions are 1-based. */
#include "xbrt_int.h"

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

/* SubStr( c, nStart [, nCount] ): a negative start counts from the end. */
void xbf_SUBSTR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *s = str_arg(argv, argc, 0);
  if (!s || argc < 2 || !xb_is_number(&argv[1]) || (argc > 2 && !xb_is_number(&argv[2]))) {
    xb_fn_arg_error(ctx, ret, 1110, "SUBSTR", argc, argv);
    return;
  }
  int64_t len = (int64_t)s->len;
  int64_t start = xb_num_to_i64(&argv[1]);
  if (start < 0) start += len + 1;
  if (start < 1) start = 1;
  int64_t count = argc > 2 ? xb_num_to_i64(&argv[2]) : len;
  if (start > len || count <= 0) {
    ret_str(ret, "", 0);
    return;
  }
  if (count > len - start + 1) count = len - start + 1;
  ret_part(ret, &argv[0], (size_t)(start - 1), (size_t)count);
}

/* Left() needs a string and a count; Right() returns "" for anything else. */
static void left_right(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, int right, int code, const char *fn) {
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
  size_t take = (uint64_t)n > s->len ? s->len : (size_t)n;
  ret_part(ret, &argv[0], right ? s->len - take : 0, take);
}

void xbf_LEFT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  left_right(ctx, ret, argc, argv, 0, 1124, "LEFT");
}
void xbf_RIGHT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  left_right(ctx, ret, argc, argv, 1, 1124, "RIGHT");
}

static void change_case(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, int upper, const char *fn) {
  const xb_str *s = str_arg(argv, argc, 0);
  if (!s) {
    xb_fn_arg_error(ctx, ret, upper ? 1102 : 1103, fn, argc, argv);
    return;
  }
  xb_str *r = xb_str_alloc(s->len);
  for (size_t i = 0; i < s->len; ++i) r->data[i] = upper ? xb_ascii_upper(s->data[i]) : xb_ascii_lower(s->data[i]);
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
static size_t find(const xb_str *needle, const xb_str *hay, int last) {
  if (needle->len == 0 || needle->len > hay->len) return 0;
  if (last) {
    for (size_t i = hay->len - needle->len + 1; i-- > 0;)
      if (memcmp(hay->data + i, needle->data, needle->len) == 0) return i + 1;
    return 0;
  }
  for (size_t i = 0; i + needle->len <= hay->len; ++i)
    if (memcmp(hay->data + i, needle->data, needle->len) == 0) return i + 1;
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

static void char_class(xb_item *ret, uint32_t argc, xb_item *argv, int which) {
  const xb_str *s = str_arg(argv, argc, 0);
  char c = s && s->len ? s->data[0] : 0;
  int r = 0;
  if (which == 0) r = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  else if (which == 1) r = c >= '0' && c <= '9';
  else if (which == 2) r = c >= 'A' && c <= 'Z';
  else r = c >= 'a' && c <= 'z';
  xb_set_logical(ret, r);
}

void xbf_ISALPHA(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  char_class(ret, argc, argv, 0);
}
void xbf_ISDIGIT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  char_class(ret, argc, argv, 1);
}
void xbf_ISUPPER(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  char_class(ret, argc, argv, 2);
}
void xbf_ISLOWER(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  char_class(ret, argc, argv, 3);
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
