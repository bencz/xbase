/* Codepages: reading and writing characters, their classes and case (see codepage.h). */
#include "core/codepage.h"

#include <string.h>

static const xb_codepage utf8 = {"UTF8", NULL, NULL, NULL, NULL};

const xb_codepage *xb_codepage_utf8(void) { return &utf8; }

static int same_name(const char *a, const char *b, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    char c = a[i] >= 'a' && a[i] <= 'z' ? (char)(a[i] - 'a' + 'A') : a[i];
    if (c != b[i] || !b[i]) return 0;
  }
  return b[len] == '\0';
}

const xb_codepage *xb_codepage_find(const char *name, size_t len) {
  static const struct {
    const char *alias, *name;
  } aliases[] = {{"UTF-8", "UTF8"},          {"IBM437", "CP437"},         {"IBM850", "CP850"},
                 {"WINDOWS-1252", "CP1252"}, {"ISO-8859-1", "ISO8859-1"}, {"LATIN1", "ISO8859-1"}};
  for (size_t k = 0; k < sizeof aliases / sizeof aliases[0]; ++k)
    if (same_name(name, aliases[k].alias, len)) {
      name = aliases[k].name;
      len = strlen(name);
      break;
    }
  if (same_name(name, utf8.name, len)) return &utf8;
  for (size_t k = 0; k < xb_cp_singles_count; ++k)
    if (same_name(name, xb_cp_singles[k].name, len)) return &xb_cp_singles[k];
  return NULL;
}

static uint32_t utf8_decode(const unsigned char *p, size_t avail, size_t *n) {
  uint32_t c = p[0], min;
  size_t need;
  *n = 1;
  if (c < 0x80) return c;
  if (c >= 0xC2 && c <= 0xDF) {
    need = 2;
    c &= 0x1F;
    min = 0x80;
  } else if (c >= 0xE0 && c <= 0xEF) {
    need = 3;
    c &= 0x0F;
    min = 0x800;
  } else if (c >= 0xF0 && c <= 0xF4) {
    need = 4;
    c &= 0x07;
    min = 0x10000;
  } else {
    return 0xFFFD;
  }
  if (avail < need) return 0xFFFD;
  for (size_t i = 1; i < need; ++i) {
    if ((p[i] & 0xC0) != 0x80) return 0xFFFD;
    c = c << 6 | (p[i] & 0x3Fu);
  }
  if (c < min || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) return 0xFFFD; /* overlong, past Unicode, surrogate */
  *n = need;
  return c;
}

uint32_t xb_cp_decode(const xb_codepage *cp, const char *s, size_t len, size_t at, size_t *n) {
  const unsigned char *p = (const unsigned char *)s + at;
  if (cp->chars) {
    *n = 1;
    return cp->chars[*p];
  }
  return utf8_decode(p, len - at, n);
}

size_t xb_cp_encode(const xb_codepage *cp, uint32_t c, char out[4]) {
  if (cp->chars) {
    if (c < 0x80 && cp->chars[c] == c) { /* ASCII, which every single-byte codepage here keeps */
      out[0] = (char)c;
      return 1;
    }
    for (unsigned b = 0; b < 256; ++b)
      if (cp->chars[b] == c) {
        out[0] = (char)b;
        return 1;
      }
    return 0;
  }
  if (c < 0x80) {
    out[0] = (char)c;
    return 1;
  }
  if (c < 0x800) {
    out[0] = (char)(0xC0 | c >> 6);
    out[1] = (char)(0x80 | (c & 0x3F));
    return 2;
  }
  if (c >= 0xD800 && c <= 0xDFFF) return 0;
  if (c < 0x10000) {
    out[0] = (char)(0xE0 | c >> 12);
    out[1] = (char)(0x80 | (c >> 6 & 0x3F));
    out[2] = (char)(0x80 | (c & 0x3F));
    return 3;
  }
  if (c > 0x10FFFF) return 0;
  out[0] = (char)(0xF0 | c >> 18);
  out[1] = (char)(0x80 | (c >> 12 & 0x3F));
  out[2] = (char)(0x80 | (c >> 6 & 0x3F));
  out[3] = (char)(0x80 | (c & 0x3F));
  return 4;
}

/* What code point `c` is (the letter ranges). */
static int kind_of(uint32_t c) {
  size_t lo = 0, hi = xb_cp_letters_count;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (xb_cp_letters[mid].last < c) lo = mid + 1;
    else hi = mid;
  }
  return lo < xb_cp_letters_count && xb_cp_letters[lo].first <= c ? xb_cp_letters[lo].kind : XB_CP_NONE;
}

/* Code point `c` in the other case, by the runs of `table` (itself when it has none). */
static uint32_t case_of(const xb_cp_case *table, size_t count, uint32_t c) {
  size_t lo = 0, hi = count;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (table[mid].last < c) lo = mid + 1;
    else hi = mid;
  }
  if (lo == count || table[lo].first > c || (c - table[lo].first) % table[lo].stride) return c;
  return (uint32_t)((int64_t)c + table[lo].delta);
}

int xb_cp_kind(const xb_codepage *cp, const char *s, size_t len) {
  if (!len) return XB_CP_NONE;
  if (cp->chars) return cp->kind[(unsigned char)s[0]];
  size_t n;
  return kind_of(utf8_decode((const unsigned char *)s, len, &n));
}

void xb_cp_change_case(const xb_codepage *cp, char *s, size_t len, int upper) {
  if (cp->chars) {
    const uint8_t *to = upper ? cp->upper : cp->lower;
    for (size_t i = 0; i < len; ++i)
      if ((unsigned char)s[i] >= 0x80) s[i] = (char)to[(unsigned char)s[i]];
    return;
  }
  const xb_cp_case *table = upper ? xb_cp_upper : xb_cp_lower;
  size_t count = upper ? xb_cp_upper_count : xb_cp_lower_count;
  for (size_t i = 0; i < len;) {
    size_t n;
    if ((unsigned char)s[i] < 0x80) {
      ++i;
      continue;
    }
    uint32_t c = utf8_decode((const unsigned char *)s + i, len - i, &n);
    uint32_t other = n > 1 ? case_of(table, count, c) : c;
    char out[4];
    if (other != c && xb_cp_encode(cp, other, out) == n) memcpy(s + i, out, n); /* the data keeps lengths */
    i += n;
  }
}

size_t xb_cp_count(const xb_codepage *cp, const char *s, size_t len) {
  if (cp->chars) return len;
  size_t chars = 0;
  for (size_t i = 0; i < len; ++chars) {
    if ((unsigned char)s[i] < 0x80) {
      ++i;
      continue;
    }
    size_t n;
    utf8_decode((const unsigned char *)s + i, len - i, &n);
    i += n;
  }
  return chars;
}

size_t xb_cp_offset(const xb_codepage *cp, const char *s, size_t len, size_t chars) {
  if (cp->chars) return chars < len ? chars : len;
  size_t i = 0;
  for (; i < len && chars > 0; --chars) {
    size_t n = 1;
    if ((unsigned char)s[i] >= 0x80) utf8_decode((const unsigned char *)s + i, len - i, &n);
    i += n;
  }
  return i;
}
