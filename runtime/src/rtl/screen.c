/* The screen functions (docs/terminal.md): positions, sizes and output on the screen model (term/screen.h). */
#include "xbrt_int.h"

#include "core/codepage.h"
#include "term/screen.h"

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

/* __ScreenDump(): the screen as text on the standard output (term/screen.h), for tests. */
void xbf___SCREENDUMP(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_term_dump();
}
