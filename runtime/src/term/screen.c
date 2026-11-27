/* The screen model (docs/terminal.md): a grid of cells, each a character with the mark that may follow it and a color
 * attribute, and the cursor. A wide character takes two cells: its first holds it, its second continues it; a write, a
 * clear or a move that leaves either half alone blanks it. The rows are a ring in storage: a scroll of the whole
 * screen (the console's, once per line past the bottom) turns it and blanks one row instead of moving all of them. */
#include "term/screen.h"

#include "core/codepage.h"
#include "core/text.h"
#include "xbrt_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WHOLE, WIDE_FIRST, WIDE_SECOND };

typedef struct cell {
  uint32_t ch;
  uint32_t mark; /* a character of no width that follows it (a combining accent), 0: none */
  uint8_t attr;
  uint8_t part;  /* WHOLE, or a half of a wide character */
} cell;

/* SetColor()'s defaults: W/N,N/W,N/N,N/N,N/W. */
#define XB_DEFAULT_COLORS {{XB_ATTR_STANDARD, 0}, {XB_ATTR_INVERSE, 0}, {0, 0}, {0, 0}, {XB_ATTR_INVERSE, 0}}

/* Sizes SetMode may ask for: a screen of more cells is refused. */
#define XB_SCREEN_MOST_CELLS ((int64_t)1 << 22)

static struct {
  uint32_t lock;
  int32_t rows, cols;
  int32_t top;               /* the row of storage that holds the screen's first row */
  cell *cells;
  int32_t row, col;          /* the cursor */
  int32_t markRow, markCol;  /* the cell of the character a mark goes on, -1: none (it was not shown) */
  char pending[4];           /* the bytes of a UTF-8 character the console's last text ended inside of */
  uint8_t pendingLen;
  cell *blanks;              /* a row of blanks in `blanksAttr`, `blanksLen` of them, that fills copy */
  size_t blanksLen;
  unsigned blanksAttr;
  xb_color colors[XB_COLORS]; /* SetColor()'s pairs */
  int selected;              /* ColorSelect()'s */
  int blink;                 /* SetBlink() */
} screen = {.markRow = -1, .colors = XB_DEFAULT_COLORS, .blink = 1};

static const xb_color kDefaultColors[XB_COLORS] = XB_DEFAULT_COLORS;

static unsigned selected_attr(void) { return screen.colors[screen.selected].attr; }

static void lock(void) { xb_word_lock(&screen.lock); }
static void unlock(void) { xb_word_unlock(&screen.lock); }

static cell blank(unsigned attr) { return (cell){' ', 0, (uint8_t)attr, WHOLE}; }

static cell *row_cells(int32_t row) {
  int32_t stored = row + screen.top;
  if (stored >= screen.rows) stored -= screen.rows;
  return &screen.cells[(size_t)stored * (size_t)screen.cols];
}

static cell *at(int32_t row, int32_t col) { return row_cells(row) + col; }

/* `n` blanks from `c`, at most a row of them: copied from the row of blanks (made again for another attribute or a
 * wider screen). */
static void fill(cell *c, size_t n, unsigned attr) {
  if (n > screen.blanksLen || attr != screen.blanksAttr) {
    size_t len = n > screen.blanksLen ? n : screen.blanksLen;
    cell *row = realloc(screen.blanks, len * sizeof(cell));
    if (!row) xb_gc_fatal("out of memory");
    for (size_t k = 0; k < len; ++k) row[k] = blank(attr);
    screen.blanks = row;
    screen.blanksLen = len;
    screen.blanksAttr = attr;
  }
  memcpy(c, screen.blanks, n * sizeof(cell));
}

/* The model exists from the first operation on: Clipper's size until a driver or SetMode says another. */
static void ensure(void) {
  if (screen.cells) return;
  screen.cells = malloc((size_t)XB_SCREEN_ROWS * XB_SCREEN_COLS * sizeof(cell));
  if (!screen.cells) xb_gc_fatal("out of memory");
  screen.rows = XB_SCREEN_ROWS;
  screen.cols = XB_SCREEN_COLS;
  for (int32_t r = 0; r < screen.rows; ++r) fill(row_cells(r), (size_t)screen.cols, XB_ATTR_STANDARD);
}

/* In columns `from` to `to` of a row: a half of a wide character whose other half is gone becomes a blank. */
static void repair_span(int32_t row, int32_t from, int32_t to) {
  for (int32_t c = from; c <= to; ++c) {
    cell *x = at(row, c);
    if (x->part == WIDE_SECOND && (c == 0 || at(row, c - 1)->part != WIDE_FIRST)) *x = blank(x->attr);
    else if (x->part == WIDE_FIRST && (c + 1 == screen.cols || at(row, c + 1)->part != WIDE_SECOND))
      *x = blank(x->attr);
  }
}

/* Before columns `left` to `right` of a row change: a wide character that crosses either edge loses its outer half. */
static void cut_edges(int32_t row, int32_t left, int32_t right) {
  if (left > 0 && at(row, left - 1)->part == WIDE_FIRST) *at(row, left - 1) = blank(at(row, left - 1)->attr);
  if (right + 1 < screen.cols && at(row, right + 1)->part == WIDE_SECOND)
    *at(row, right + 1) = blank(at(row, right + 1)->attr);
}

/* Writes a character of width 1 or 2 at a position within the screen (both cells, for a wide one); a mark after it
 * goes on it. */
static void put(int32_t row, int32_t col, uint32_t ch, int width, unsigned attr) {
  int32_t last = col + width - 1;
  cut_edges(row, col, last);
  *at(row, col) = (cell){ch, 0, (uint8_t)attr, width == 2 ? WIDE_FIRST : WHOLE};
  if (width == 2) *at(row, last) = (cell){0, 0, (uint8_t)attr, WIDE_SECOND};
  screen.markRow = row;
  screen.markCol = col;
}

/* Where a mark at the start of a text goes: on the character before the cursor, when that one is on the screen. */
static void mark_before_cursor(void) {
  screen.markRow = -1;
  if (screen.row < 0 || screen.row >= screen.rows || screen.col <= 0 || screen.col > screen.cols) return;
  int32_t c = screen.col - 1;
  if (at(screen.row, c)->part == WIDE_SECOND) --c;
  screen.markRow = screen.row;
  screen.markCol = c;
}

static void add_mark(uint32_t ch) {
  if (screen.markRow < 0) return;
  cell *x = at(screen.markRow, screen.markCol);
  if (!x->mark) x->mark = ch; /* one mark a character: later ones go */
}

static void scroll_locked(int32_t top, int32_t left, int32_t bottom, int32_t right, int32_t rows, int32_t cols,
                          unsigned attr) {
  if (top < 0) top = 0;
  if (left < 0) left = 0;
  if (bottom >= screen.rows) bottom = screen.rows - 1;
  if (right >= screen.cols) right = screen.cols - 1;
  if (top > bottom || left > right) return;
  int32_t height = bottom - top + 1;
  size_t width = (size_t)(right - left + 1);
  if (rows == 0 && cols == 0) {
    for (int32_t r = top; r <= bottom; ++r) {
      cut_edges(r, left, right);
      fill(at(r, left), width, attr);
    }
    return;
  }
  if (cols == 0 && left == 0 && right == screen.cols - 1) { /* whole rows move: no wide character is cut */
    if (rows >= height || rows <= -height) {
      for (int32_t r = top; r <= bottom; ++r) fill(row_cells(r), width, attr);
    } else if (top == 0 && bottom == screen.rows - 1) { /* the whole screen: the ring turns */
      int32_t n = rows > 0 ? rows : -rows;
      int32_t from = rows > 0 ? 0 : screen.rows - n; /* the rows that come back blank at the other end */
      for (int32_t r = from; r < from + n; ++r) fill(row_cells(r), width, attr);
      screen.top = (int32_t)(((int64_t)screen.top + rows + screen.rows) % screen.rows);
    } else if (rows > 0) {
      for (int32_t r = top; r <= bottom - rows; ++r) memcpy(row_cells(r), row_cells(r + rows), width * sizeof(cell));
      for (int32_t r = bottom - rows + 1; r <= bottom; ++r) fill(row_cells(r), width, attr);
    } else {
      for (int32_t r = bottom; r >= top - rows; --r) memcpy(row_cells(r), row_cells(r + rows), width * sizeof(cell));
      for (int32_t r = top; r < top - rows; ++r) fill(row_cells(r), width, attr);
    }
    return;
  }
  cell *old = malloc(width * (size_t)height * sizeof(cell));
  if (!old) xb_gc_fatal("out of memory");
  for (int32_t r = 0; r < height; ++r) {
    cell *src = old + (size_t)r * width;
    memcpy(src, at(top + r, left), width * sizeof(cell));
    if (src[0].part == WIDE_SECOND) src[0] = blank(src[0].attr); /* halves whose other half is outside */
    if (src[width - 1].part == WIDE_FIRST) src[width - 1] = blank(src[width - 1].attr);
  }
  for (int32_t r = top; r <= bottom; ++r) {
    cut_edges(r, left, right);
    for (int32_t c = left; c <= right; ++c) {
      int64_t sr = (int64_t)r + rows, sc = (int64_t)c + cols;
      *at(r, c) = sr >= top && sr <= bottom && sc >= left && sc <= right
                      ? old[(size_t)(sr - top) * width + (size_t)(sc - left)]
                      : blank(attr);
    }
    repair_span(r, left, right);
  }
  free(old);
}

/* Past the bottom row the screen scrolls up one: the ring turns, its first row becoming the blank last one. */
static void new_line(void) {
  screen.col = 0;
  if (++screen.row < screen.rows) return;
  screen.row = screen.rows - 1;
  fill(row_cells(0), (size_t)screen.cols, selected_attr());
  if (++screen.top == screen.rows) screen.top = 0;
}

/* The length of `s` up to the UTF-8 character it ends inside of (all of it when it ends with a whole one). */
static size_t whole_utf8(const char *s, size_t len) {
  for (size_t k = 1; k <= 3 && k <= len; ++k) {
    unsigned char b = (unsigned char)s[len - k];
    if ((b & 0xC0) == 0x80) continue; /* a continuation byte: the lead is further back */
    size_t need = b >= 0xF0 ? 4 : b >= 0xE0 ? 3 : b >= 0xC0 ? 2 : 1;
    return need > k ? len - k : len;
  }
  return len;
}

/* A byte that is a character of width 1 in every codepage, which no console rule changes: printable ASCII. */
static int plain_byte(unsigned char b) { return b >= 0x20 && b < 0x7F; }

static void console_char(uint32_t ch) {
  if (ch == '\r') {
    screen.col = 0;
  } else if (ch == '\n') {
    new_line();
  } else if (ch == '\b') { /* at a line's start: the last column of the line before */
    if (screen.col > 0) --screen.col;
    else if (screen.row > 0) {
      --screen.row;
      screen.col = screen.cols - 1;
    }
  } else if (ch != 7) {
    int width = xb_cp_width(ch);
    if (!width) {
      add_mark(ch);
      return;
    }
    if (width > screen.cols) { /* wider than the screen: not shown */
      screen.markRow = -1;
      return;
    }
    if (screen.col + width > screen.cols) new_line();
    put(screen.row, screen.col, ch, width, selected_attr());
    screen.col += width;
    if (screen.col >= screen.cols) new_line();
  }
}

void xb_screen_console(const xb_codepage *cp, const char *text, size_t len) {
  lock();
  ensure();
  if (screen.row < 0) screen.row = 0;
  if (screen.row >= screen.rows) screen.row = screen.rows - 1;
  if (screen.col < 0) screen.col = 0;
  if (screen.col >= screen.cols) screen.col = screen.cols - 1;
  mark_before_cursor();
  int utf8 = !cp->chars;
  char *joined = NULL;
  if (screen.pendingLen && utf8) { /* the character the last text ended inside of comes first */
    joined = malloc(screen.pendingLen + len);
    if (!joined) xb_gc_fatal("out of memory");
    memcpy(joined, screen.pending, screen.pendingLen);
    memcpy(joined + screen.pendingLen, text, len);
    len += screen.pendingLen;
    text = joined;
  }
  size_t whole = utf8 ? whole_utf8(text, len) : len;
  memcpy(screen.pending, text + whole, len - whole);
  screen.pendingLen = (uint8_t)(len - whole);
  for (size_t at_byte = 0, n = 0; at_byte < whole; at_byte += n) {
    if (!plain_byte((unsigned char)text[at_byte])) {
      console_char(xb_cp_decode(cp, text, whole, at_byte, &n));
      continue;
    }
    /* A run of plain bytes on the cursor's row, up to its end: each in its cell. */
    cell *line = row_cells(screen.row);
    int32_t col = screen.col;
    uint8_t attr = (uint8_t)selected_attr();
    size_t k = at_byte;
    for (; k < whole && col < screen.cols && plain_byte((unsigned char)text[k]); ++k, ++col) {
      if (line[col].part != WHOLE) cut_edges(screen.row, col, col);
      line[col] = (cell){(unsigned char)text[k], 0, attr, WHOLE};
    }
    n = k - at_byte;
    screen.markRow = screen.row;
    screen.markCol = col - 1;
    screen.col = col;
    if (col >= screen.cols) new_line();
  }
  free(joined);
  unlock();
}

void xb_screen_say(const xb_codepage *cp, const char *text, size_t len, unsigned attr) {
  lock();
  ensure();
  mark_before_cursor();
  int shown = screen.row >= 0 && screen.row < screen.rows;
  for (size_t at_byte = 0, n = 0; at_byte < len; at_byte += n) {
    uint32_t ch = xb_cp_decode(cp, text, len, at_byte, &n);
    int width = xb_cp_width(ch);
    if (!width) {
      add_mark(ch);
      continue;
    }
    int64_t next = (int64_t)screen.col + width;
    if (shown && screen.col >= 0 && next <= screen.cols) put(screen.row, screen.col, ch, width, attr);
    else screen.markRow = -1;
    screen.col = next < screen.cols ? (int32_t)next : screen.cols;
  }
  unlock();
}

size_t xb_screen_colors(const char *spec, size_t len, char *was) {
  xb_color read[XB_COLORS];
  int given[XB_COLORS] = {0};
  for (size_t at_byte = 0, k = 0; spec && len && k < XB_COLORS && at_byte <= len; ++k) {
    const char *comma = memchr(spec + at_byte, ',', len - at_byte);
    size_t end = comma ? (size_t)(comma - spec) : len;
    given[k] = xb_color_read(spec + at_byte, end - at_byte, &read[k]);
    at_byte = end + 1;
  }
  size_t n = 0;
  lock();
  for (int k = 0; k < XB_COLORS; ++k) {
    if (k) was[n++] = ',';
    n += xb_color_write(&screen.colors[k], was + n);
  }
  if (spec) {
    if (!len) memcpy(screen.colors, kDefaultColors, sizeof kDefaultColors);
    for (int k = 0; k < XB_COLORS; ++k)
      if (given[k]) screen.colors[k] = read[k];
    if (given[XB_COLOR_ENHANCED] && !given[XB_COLOR_UNSELECTED])
      screen.colors[XB_COLOR_UNSELECTED] = screen.colors[XB_COLOR_ENHANCED];
    screen.selected = XB_COLOR_STANDARD;
  }
  unlock();
  return n;
}

unsigned xb_screen_color(void) {
  lock();
  unsigned attr = selected_attr();
  unlock();
  return attr;
}

unsigned xb_screen_color_of(const char *spec, size_t len) {
  const char *comma = memchr(spec, ',', len);
  xb_color pair;
  int given = xb_color_read(spec, comma ? (size_t)(comma - spec) : len, &pair);
  lock();
  const xb_color *none = len ? &screen.colors[XB_COLOR_STANDARD] : &kDefaultColors[XB_COLOR_STANDARD];
  unsigned attr = given ? pair.attr : none->attr;
  screen.selected = XB_COLOR_STANDARD;
  unlock();
  return attr;
}

void xb_screen_select_color(int64_t n) {
  if (n < 0 || n >= XB_COLORS) return;
  lock();
  screen.selected = (int)n;
  unlock();
}

int xb_screen_blink(int on) {
  lock();
  int was = screen.blink;
  if (on >= 0) screen.blink = on;
  unlock();
  return was;
}

void xb_screen_cursor(int32_t *row, int32_t *col) {
  lock();
  *row = screen.row;
  *col = screen.col;
  unlock();
}

void xb_screen_set_cursor(int32_t row, int32_t col) {
  lock();
  screen.row = row;
  screen.col = col;
  unlock();
}

void xb_screen_size(int32_t *rows, int32_t *cols) {
  lock();
  ensure();
  *rows = screen.rows;
  *cols = screen.cols;
  unlock();
}

int xb_screen_set_size(int32_t rows, int32_t cols) {
  if (rows < 1 || cols < 1 || (int64_t)rows * cols > XB_SCREEN_MOST_CELLS) return 0;
  cell *cells = malloc((size_t)rows * (size_t)cols * sizeof(cell));
  if (!cells) return 0;
  lock();
  ensure();
  for (int32_t r = 0; r < rows; ++r)
    for (int32_t c = 0; c < cols; ++c)
      cells[(size_t)r * (size_t)cols + (size_t)c] = r < screen.rows && c < screen.cols ? *at(r, c)
                                                                                      : blank(XB_ATTR_STANDARD);
  free(screen.cells);
  screen.cells = cells;
  screen.rows = rows;
  screen.cols = cols;
  screen.top = 0;
  for (int32_t r = 0; r < rows; ++r) repair_span(r, cols - 1, cols - 1);
  if (screen.row >= rows) screen.row = rows - 1;
  if (screen.col >= cols) screen.col = cols - 1;
  unlock();
  return 1;
}

void xb_screen_scroll(int32_t top, int32_t left, int32_t bottom, int32_t right, int32_t rows, int32_t cols,
                      unsigned attr) {
  lock();
  ensure();
  scroll_locked(top, left, bottom, right, rows, cols, attr);
  unlock();
}

static void put_text(xb_buffer *out, const char *text) { xb_buffer_put(out, text, strlen(text)); }

/* A character as the dump shows it: a control character as its Unicode picture (U+2400 on). */
static void dump_char(xb_buffer *out, uint32_t ch) {
  if (ch < 0x20) ch += 0x2400;
  else if (ch == 0x7F) ch = 0x2421;
  char bytes[4];
  xb_buffer_put(out, bytes, xb_cp_encode(xb_codepage_utf8(), ch, bytes));
}

/* A row's number, right-aligned in `digits` columns, and the bar after it. */
static void dump_row_number(xb_buffer *out, int digits, int32_t row) {
  char text[16];
  int n = snprintf(text, sizeof text, "%d", (int)row);
  for (; n < digits; ++n) put_text(out, " ");
  put_text(out, text);
  put_text(out, "|");
}

void xb_screen_dump(xb_buffer *out) {
  lock();
  ensure();
  char text[96];
  snprintf(text, sizeof text, "screen %d x %d, cursor %d, %d\n", (int)screen.rows, (int)screen.cols,
           (int)screen.row, (int)screen.col);
  put_text(out, text);
  int digits = 2;
  for (int32_t n = screen.rows - 1; n >= 100; n /= 10) ++digits;
  for (int32_t r = 0; r < screen.rows; ++r) {
    int32_t end = screen.cols;
    while (end > 0 && at(r, end - 1)->ch == ' ' && !at(r, end - 1)->mark) --end;
    dump_row_number(out, digits, r);
    for (int32_t c = 0; c < end; ++c) {
      const cell *x = at(r, c);
      if (x->part == WIDE_SECOND) continue;
      dump_char(out, x->ch);
      if (x->mark) dump_char(out, x->mark);
    }
    put_text(out, "\n");
  }
  int header = 0;
  for (int32_t r = 0; r < screen.rows; ++r) {
    int plain = 1;
    for (int32_t c = 0; c < screen.cols && plain; ++c) plain = at(r, c)->attr == XB_ATTR_STANDARD;
    if (plain) continue;
    if (!header) put_text(out, "colors\n");
    header = 1;
    dump_row_number(out, digits, r);
    for (int32_t c = 0; c < screen.cols;) {
      int32_t run = c;
      while (run < screen.cols && at(r, run)->attr == at(r, c)->attr) ++run;
      snprintf(text, sizeof text, "%s%02X*%d", c ? " " : "", (unsigned)at(r, c)->attr, (int)(run - c));
      put_text(out, text);
      c = run;
    }
    put_text(out, "\n");
  }
  unlock();
}
