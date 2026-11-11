/* The memo-line functions: a text cut into lines of a width, Clipper's way.
 *
 * A line ends at a hard return (CR LF, taken with the line), or where the
 * next character would pass the width: with word wrap, after the last blank
 * of the line (a blank that passes the width ends the line itself), or before
 * the character when the line has no blank; without it, before the character.
 * A soft return (the byte 141 and LF, which word wrap leaves in a text) is
 * skipped with word wrap and ends the line without it. A tab takes the
 * columns up to the next multiple of the tab size. These rules, and their
 * corner cases, are Clipper's; MLCToPos() and MPosToLC() stop a line at a
 * column or a position. */
#include "xbrt_int.h"

#include <stdlib.h>
#include <string.h>

#define SOFT1 ((char)141)
#define SOFT2 '\n'
#define TAB '\t'

typedef struct scan {
  const char *s;
  size_t len;
  size_t width, tab;
  int wrap;
  size_t off;               /* where the next line starts */
  size_t col;               /* the columns the line just read takes */
  size_t stopCol;           /* MLCToPos: the line stops at this column (0: no stop) */
  size_t stopPos;           /* MPosToLC: columns are counted up to this 1-based position (0: none) */
} scan;

static int at_soft(const scan *m, size_t i) { return i + 1 < m->len && m->s[i] == SOFT1 && m->s[i + 1] == SOFT2; }
static int at_hard(const scan *m, size_t i) { return i + 1 < m->len && m->s[i] == '\r' && m->s[i + 1] == '\n'; }

/* A number argument, `dflt` when it is not a number. */
static int64_t num_or(uint32_t argc, const xb_item *argv, uint32_t i, int64_t dflt) {
  return i < argc && xb_is_number(&argv[i]) ? xb_num_to_i64(&argv[i]) : dflt;
}

/* The text (argument 1), the width (2, 79 by default), the tab size and word
 * wrap (at `at` and after it: 4 and .T. by default): 0 when there is no text
 * or no positive width. A tab size of 0 is 1; one as wide as the line, or
 * negative, is the width less one. */
static int scan_init(scan *m, uint32_t argc, const xb_item *argv, uint32_t at) {
  memset(m, 0, sizeof *m);
  int64_t width = num_or(argc, argv, 1, 79);
  if (!argc || argv[0].type != XB_T_STRING || width <= 0) return 0;
  m->s = argv[0].v.s->data;
  m->len = argv[0].v.s->len;
  m->width = (size_t)width;
  int64_t tab = num_or(argc, argv, at, 4);
  m->tab = tab < 0 || (uint64_t)tab >= m->width ? m->width - 1 : (size_t)tab;
  if (m->tab == 0) m->tab = 1;
  m->wrap = at + 1 < argc && argv[at + 1].type == XB_T_LOGICAL ? argv[at + 1].v.l != 0 : 1;
  return 1;
}

static size_t advance_col(const scan *m, size_t col, char c) { return col + (c == TAB ? m->tab - col % m->tab : 1); }

/* Reads the next line: 0 at the end of the text. `off` moves past it, `col`
 * is its width in columns. */
static int scan_line(scan *m) {
  size_t blankCol = 0, blankPos = 0, lastCol = 0;
  m->col = 0;
  if (m->off >= m->len) return 0;
  while (m->off < m->len) {
    if (at_soft(m, m->off)) {
      if (m->stopCol && m->col) break;
      if (m->off < m->stopPos) lastCol = m->col;
      m->off += 2;
      if (!m->wrap) break;
      if (blankPos + 2 == m->off) blankPos += 2; /* a blank right before it: the line may end after both */
      continue;
    }
    if (at_hard(m, m->off)) {
      if (m->off < m->stopPos) lastCol = m->col;
      if (!m->stopCol) m->off += 2;
      break;
    }
    if (!m->wrap && m->col >= m->width) break;
    size_t before = m->off;
    char c = m->s[m->off++];
    if (m->off <= m->stopPos) lastCol = m->col;
    m->col = advance_col(m, m->col, c);
    if (m->stopCol && m->col >= m->stopCol) {
      if (m->col > m->stopCol) m->off = before;
      break;
    }
    if (m->col > m->width) {
      if (m->wrap && (c == ' ' || c == TAB)) break; /* the blank ends the line */
      if (m->wrap && blankCol) {
        m->col = blankCol;
        m->off = blankPos;
      } else {
        m->off = before;
      }
      break;
    }
    if (m->col > 1 && (c == ' ' || c == TAB)) {
      blankCol = m->col;
      blankPos = m->off;
    }
  }
  if (m->stopPos && m->col > lastCol) m->col = lastCol;
  else if (m->col > m->width) m->col = m->width;
  return 1;
}

/* Skips `n` lines: the lines left to skip when the text ended first. */
static int64_t skip_lines(scan *m, int64_t n) {
  while (n > 0 && scan_line(m)) --n;
  return n;
}

/* MemoLine( cText, [nWidth], [nLine], [nTabSize], [lWrap] ): line nLine (1 by
 * default) with its tabs as blanks, padded with blanks to nWidth; "" past the
 * text (as in Clipper, the line right after the last one is all blanks). */
void xbf_MEMOLINE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  scan m;
  int64_t line = num_or(argc, argv, 2, 1);
  if (line < 1 || !scan_init(&m, argc, argv, 3) || skip_lines(&m, line - 1) > 0) {
    xb_set_str(ret, xb_str_empty());
    return;
  }
  size_t from = m.off;
  scan_line(&m);
  char *out = malloc(m.width ? m.width : 1);
  if (!out) xb_gc_fatal("out of memory");
  size_t n = 0, col = 0;
  for (size_t i = from; i < m.len && col < m.col;) {
    if (at_soft(&m, i)) {
      i += 2;
      continue;
    }
    char c = m.s[i++];
    if (c != TAB) {
      out[n++] = c;
      ++col;
      continue;
    }
    size_t blanks = m.tab - n % m.tab; /* Clipper counts from the text written, not the column */
    do {
      out[n++] = ' ';
    } while (++col < m.col && --blanks);
  }
  if (col < m.width) {
    size_t pad = m.width - col;
    if (pad > m.width - n) pad = m.width - n;
    memset(out + n, ' ', pad);
    n += pad;
  }
  xb_set_str(ret, xb_str_new(out, n));
  free(out);
}

/* MLCount( cText, [nWidth], [nTabSize], [lWrap] ): the number of lines. */
void xbf_MLCOUNT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  scan m;
  int64_t lines = 0;
  if (scan_init(&m, argc, argv, 2))
    while (scan_line(&m)) ++lines;
  xb_set_int(ret, lines);
}

/* MLPos( cText, nWidth, nLine, [nTabSize], [lWrap] ): the 1-based position
 * where line nLine starts (the text's length when it starts at the end, one
 * line past it too; 0 further on). */
void xbf_MLPOS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  scan m;
  int64_t line = num_or(argc, argv, 2, 1), pos = 0;
  if (line >= 1 && scan_init(&m, argc, argv, 3) && skip_lines(&m, line - 1) <= 1)
    pos = (int64_t)m.off + (m.off < m.len ? 1 : 0);
  xb_set_int(ret, pos);
}

/* MLCToPos( cText, nWidth, nLine, nCol, [nTabSize], [lWrap] ): the 1-based
 * position of column nCol (0-based) of line nLine; a width below 5 gives 1. */
void xbf_MLCTOPOS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  scan m;
  int64_t line = num_or(argc, argv, 2, 0), col = num_or(argc, argv, 3, 0), pos = 0;
  if (line != 0 && argc > 3 && xb_is_number(&argv[3]) && scan_init(&m, argc, argv, 4) && m.width > 4) {
    /* a negative line or column is past every one */
    int64_t left = skip_lines(&m, line < 0 ? INT64_MAX : line - 1);
    if (col != 0 && left == 0) {
      m.stopCol = m.width = col < 0 ? SIZE_MAX : (size_t)col;
      scan_line(&m);
    }
    pos = (int64_t)m.off;
  }
  xb_set_int(ret, pos + 1);
}

/* MPosToLC( cText, nWidth, nPos, [nTabSize], [lWrap] ): { nLine, nCol }, the
 * line (1-based) and column (0-based) of position nPos; { 0, 0 } for a
 * position below 1 or past the text and the one after it. */
void xbf_MPOSTOLC(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  scan m;
  int64_t pos = num_or(argc, argv, 2, 0);
  int64_t line = 0, col = 0;
  if (pos > 0 && scan_init(&m, argc, argv, 3) && (uint64_t)pos <= (uint64_t)m.len + 1) {
    m.stopPos = (size_t)pos;
    for (;;) {
      size_t from = m.off;
      scan_line(&m);
      col = (int64_t)m.col;
      ++line;
      if (m.off == from || m.off >= m.stopPos) break;
    }
  }
  xb_item lc[2];
  xb_set_int(&lc[0], line);
  xb_set_int(&lc[1], col);
  xb_array_new(ctx, ret, 2, lc);
}

/* HardCR( cText ): the soft returns made hard (CR LF). */
void xbf_HARDCR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  if (!argc || argv[0].type != XB_T_STRING) {
    xb_set_str(ret, xb_str_empty());
    return;
  }
  const xb_str *s = argv[0].v.s;
  if (!memchr(s->data, SOFT1, s->len)) {
    xb_set_str(ret, argv[0].v.s); /* a new value, as when it changes something: a memo's is a plain string */
    return;
  }
  xb_str *r = xb_str_alloc(s->len);
  memcpy(r->data, s->data, s->len);
  for (size_t i = 0; i + 1 < r->len; ++i)
    if (r->data[i] == SOFT1 && r->data[i + 1] == SOFT2) r->data[i] = '\r';
  xb_set_str(ret, r);
}

/* MemoTran( cText, [cHard], [cSoft] ): each hard return (CR LF) replaced by
 * the first character of cHard (";" by default), each soft one by that of
 * cSoft (a blank by default); an empty replacement counts as the default. */
void xbf_MEMOTRAN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  if (!argc || argv[0].type != XB_T_STRING) {
    xb_set_str(ret, xb_str_empty());
    return;
  }
  char hard = argc > 1 && argv[1].type == XB_T_STRING && argv[1].v.s->len ? argv[1].v.s->data[0] : ';';
  char soft = argc > 2 && argv[2].type == XB_T_STRING && argv[2].v.s->len ? argv[2].v.s->data[0] : ' ';
  const xb_str *s = argv[0].v.s;
  char *out = malloc(s->len ? s->len : 1);
  if (!out) xb_gc_fatal("out of memory");
  size_t n = 0;
  for (size_t i = 0; i < s->len;) {
    int isHard = i + 1 < s->len && s->data[i] == '\r' && s->data[i + 1] == '\n';
    int isSoft = i + 1 < s->len && s->data[i] == SOFT1 && s->data[i + 1] == SOFT2;
    if (isHard || isSoft) {
      out[n++] = isHard ? hard : soft;
      i += 2;
    } else {
      out[n++] = s->data[i++];
    }
  }
  xb_set_str(ret, xb_str_new(out, n));
  free(out);
}
