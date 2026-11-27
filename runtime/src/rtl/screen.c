/* The screen functions (docs/terminal.md): positions, sizes and output on the screen model (term/screen.h). */
#include "xbrt_int.h"

#include "core/codepage.h"
#include "term/screen.h"

#include <string.h>

/* A position argument (xb_num_arg) held within 32 bits; `dflt` when it is no number. */
static int32_t position(const xb_item *argv, uint32_t argc, uint32_t k, int32_t dflt) {
  int64_t n = xb_num_arg(argc, argv, k, dflt);
  return n < INT32_MIN ? INT32_MIN : n > INT32_MAX ? INT32_MAX : (int32_t)n;
}

/* The attribute a color argument writes in (xb_screen_color_of), or the selected pair's when it is no string. */
static unsigned color_arg(const xb_item *argv, uint32_t argc, uint32_t k) {
  if (k >= argc || argv[k].type != XB_T_STRING) return xb_screen_color();
  return xb_screen_color_of(xb_str_data(argv[k].v.s), argv[k].v.s->len);
}

/* What a device shows of a value, in attribute `attr`: its display form (term/screen.h), a logical as T or F. */
static void say(xb_ctx *ctx, const xb_item *value, unsigned attr) {
  char in_place[XB_TEXT_IN_PLACE];
  xb_buffer text = {in_place, 0, sizeof in_place, 0};
  if (value->type == XB_T_LOGICAL) {
    xb_buffer_put(&text, value->v.l ? "T" : "F", 1);
  } else {
    const xb_str *s = xb_item_display(ctx, value); /* copied before anything else allocates */
    xb_buffer_put(&text, xb_str_data(s), s->len);
  }
  xb_screen_say(xb_codepage_of(ctx), text.p, text.len, attr);
  xb_buffer_free(&text);
}

/* Row(), Col(): the cursor's row and column (a position SetPos gave may lie outside the screen). */
void xbf_ROW(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  int32_t row, col;
  xb_screen_cursor(&row, &col);
  xb_set_int(ret, row);
}

void xbf_COL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  int32_t row, col;
  xb_screen_cursor(&row, &col);
  xb_set_int(ret, col);
}

/* MaxRow(), MaxCol(): the last row and column of the screen. */
void xbf_MAXROW(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  int32_t rows, cols;
  xb_screen_size(&rows, &cols);
  xb_set_int(ret, rows - 1);
}

void xbf_MAXCOL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  int32_t rows, cols;
  xb_screen_size(&rows, &cols);
  xb_set_int(ret, cols - 1);
}

/* SetPos( nRow, nCol ), DevPos( nRow, nCol ): the cursor there (both numbers; else nothing moves). */
static void set_position(uint32_t argc, const xb_item *argv) {
  if (argc < 2 || !xb_is_number(&argv[0]) || !xb_is_number(&argv[1])) return;
  xb_screen_set_cursor(position(argv, argc, 0, 0), position(argv, argc, 1, 0));
}

void xbf_SETPOS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(ret);
  set_position(argc, argv);
}

void xbf_DEVPOS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(ret);
  set_position(argc, argv);
}

/* DevOut( xValue, [cColor] ), DispOut( xValue, [cColor] ): the value's display form at the cursor (@ SAY's), in the
 * color's first pair (the selected one without it). */
void xbf_DEVOUT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  if (argc) say(ctx, &argv[0], color_arg(argv, argc, 1));
}

void xbf_DISPOUT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  if (argc) say(ctx, &argv[0], color_arg(argv, argc, 1));
}

/* DispOutAt( nRow, nCol, xValue, [cColor] ): the cursor there, then the value's display form; the cursor stays past
 * it. Without a value, nothing moves. */
void xbf_DISPOUTAT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  if (argc < 3) return;
  set_position(argc, argv);
  say(ctx, &argv[2], color_arg(argv, argc, 3));
}

/* Scroll( [nTop], [nLeft], [nBottom], [nRight], [nVertical], [nHorizontal] ): the rectangle (the whole screen by
 * default) moved up by nVertical rows and left by nHorizontal columns (down and right when negative); without
 * either, cleared; blank in the selected pair. The cursor stays. */
void xbf_SCROLL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(ret);
  int32_t rows, cols;
  xb_screen_size(&rows, &cols);
  xb_screen_scroll(position(argv, argc, 0, 0), position(argv, argc, 1, 0), position(argv, argc, 2, rows - 1),
                   position(argv, argc, 3, cols - 1), position(argv, argc, 4, 0), position(argv, argc, 5, 0),
                   xb_screen_color());
}

/* SetColor( [cColors] ): the five pairs as they were (term/screen.h), after which cColors sets them. */
void xbf_SETCOLOR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  char text[XB_COLORS * XB_COLOR_TEXT];
  int set = argc && argv[0].type == XB_T_STRING;
  size_t n = xb_screen_colors(set ? xb_str_data(argv[0].v.s) : NULL, set ? argv[0].v.s->len : 0, text);
  xb_set_str(ret, xb_str_new(text, n));
}

/* ColorSelect( nPair ): the pair (0 standard, 1 enhanced, 2 border, 3 background, 4 unselected) the output takes. */
void xbf_COLORSELECT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(ret);
  xb_screen_select_color(xb_num_arg(argc, argv, 0, -1));
}

/* SetBlink( [lBlink] ): whether * blinks (else it is a bright background), before lBlink sets it. */
void xbf_SETBLINK(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  int on = argc && argv[0].type == XB_T_LOGICAL ? (argv[0].v.l ? 1 : 0) : -1;
  xb_set_logical(ret, xb_screen_blink(on));
}

/* IsColor(): .T.: every driver shows colors. */
void xbf_ISCOLOR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_set_logical(ret, 1);
}

/* SetMode( [nRows], [nCols] ): .T. when the screen takes that size (the cells that fit kept; a size not given, the
 * one it has), else .F. */
void xbf_SETMODE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  int32_t rows, cols;
  xb_screen_size(&rows, &cols);
  xb_set_logical(ret, xb_screen_set_size(position(argv, argc, 0, rows), position(argv, argc, 1, cols)));
}

/* The frames DispBox() draws for a number: 2 the double one, any other the single one (top left, top, top right,
 * right, bottom right, bottom, bottom left, left), the inside left as it is. */
static const uint32_t kSingleFrame[9] = {0x250C, 0x2500, 0x2510, 0x2502, 0x2518, 0x2500, 0x2514, 0x2502, 0};
static const uint32_t kDoubleFrame[9] = {0x2554, 0x2550, 0x2557, 0x2551, 0x255D, 0x2550, 0x255A, 0x2551, 0};

/* Whether `text` reads in UTF-8 with no byte that starts no character. */
static int valid_utf8(const char *text, size_t len) {
  const xb_codepage *utf8 = xb_codepage_utf8();
  for (size_t at_byte = 0, n = 0; at_byte < len; at_byte += n)
    if (xb_cp_decode(utf8, text, len, at_byte, &n) == 0xFFFD && n == 1) return 0;
  return 1;
}

/* The frame a string gives (read in the codepage up to a Chr(0); under UTF-8, one that is not UTF-8 in CP437, as
 * Clipper's frames are written in bytes): its characters in DispBox()'s order, marks of no width left out, a missing
 * one the one before it (the ninth, filling the inside, only when given); an empty string blanks the whole box. */
static void frame_of(const xb_codepage *cp, const char *text, size_t len, uint32_t frame[9]) {
  const char *end = memchr(text, 0, len);
  if (end) len = (size_t)(end - text);
  if (!cp->chars && !valid_utf8(text, len)) cp = xb_codepage_find("CP437", 5);
  size_t k = 0;
  for (size_t at_byte = 0, n = 0; at_byte < len && k < 9; at_byte += n) {
    uint32_t ch = xb_cp_decode(cp, text, len, at_byte, &n);
    if (xb_cp_width(ch)) frame[k++] = ch;
  }
  if (!k) {
    for (; k < 9; ++k) frame[k] = ' ';
    return;
  }
  for (size_t j = k; j < 8; ++j) frame[j] = frame[k - 1];
  if (k < 9) frame[8] = 0;
}

/* DispBox( nTop, nLeft, nBottom, nRight, [cnFrame], [cColor] ): a frame (term/screen.h): cnFrame's characters, or 2
 * for the double frame and any other the single one (the same frames as box.ch's B_SINGLE and B_DOUBLE); the cursor
 * after it at (nTop + 1, nLeft + 1). */
void xbf_DISPBOX(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  for (uint32_t k = 0; k < 4; ++k)
    if (k >= argc || !xb_is_number(&argv[k])) return; /* a corner that is no number: no frame */
  int32_t top = position(argv, argc, 0, 0), left = position(argv, argc, 1, 0);
  uint32_t frame[9];
  if (argc > 4 && argv[4].type == XB_T_STRING)
    frame_of(xb_codepage_of(ctx), xb_str_data(argv[4].v.s), argv[4].v.s->len, frame);
  else memcpy(frame, xb_num_arg(argc, argv, 4, 1) == 2 ? kDoubleFrame : kSingleFrame, sizeof frame);
  xb_screen_box(top, left, position(argv, argc, 2, 0), position(argv, argc, 3, 0), frame, color_arg(argv, argc, 5));
  xb_screen_set_cursor(top < INT32_MAX ? top + 1 : top, left < INT32_MAX ? left + 1 : left);
}

/* SaveScreen( [nTop], [nLeft], [nBottom], [nRight] ): the rectangle (the whole screen by default) as a string
 * (term/screen.h). */
void xbf_SAVESCREEN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  int32_t rows, cols;
  xb_screen_size(&rows, &cols);
  xb_buffer text = {0};
  xb_screen_save(position(argv, argc, 0, 0), position(argv, argc, 1, 0), position(argv, argc, 2, rows - 1),
                 position(argv, argc, 3, cols - 1), xb_codepage_of(ctx), &text);
  xb_set_str(ret, xb_str_new(text.p ? text.p : "", text.len));
  xb_buffer_free(&text);
}

/* RestScreen( [nTop], [nLeft], [nBottom], [nRight], cScreen ): the string SaveScreen() made back on the rectangle. */
void xbf_RESTSCREEN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  if (argc < 5 || argv[4].type != XB_T_STRING) return;
  int32_t rows, cols;
  xb_screen_size(&rows, &cols);
  const xb_str *s = argv[4].v.s;
  char in_place[XB_TEXT_IN_PLACE];
  xb_buffer text = {in_place, 0, sizeof in_place, 0}; /* a copy: the string may move while the model's lock waits */
  xb_buffer_put(&text, xb_str_data(s), s->len);
  xb_screen_restore(position(argv, argc, 0, 0), position(argv, argc, 1, 0), position(argv, argc, 2, rows - 1),
                    position(argv, argc, 3, cols - 1), xb_codepage_of(ctx), text.p, text.len);
  xb_buffer_free(&text);
}

/* DispBegin(), DispEnd(), DispCount(): the depth of the screen's held updates, one more, one less, as it is. */
void xbf_DISPBEGIN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_screen_hold(1);
}

void xbf_DISPEND(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_screen_hold(-1);
}

void xbf_DISPCOUNT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_set_int(ret, xb_screen_hold(0));
}

/* SetCursor( [nShape] ): the cursor's shape (setcurs.ch) before nShape (0 to 4; another number, the normal one) sets
 * it. */
void xbf_SETCURSOR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  int set = argc && xb_is_number(&argv[0]);
  int64_t shape = xb_num_arg(argc, argv, 0, 1);
  xb_set_int(ret, xb_screen_cursor_shape(!set ? -1 : shape >= 0 && shape <= 4 ? (int)shape : 1));
}

/* __ScreenDump(): the screen as text on the standard output (term/screen.h), for tests. */
void xbf___SCREENDUMP(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_term_dump();
}
