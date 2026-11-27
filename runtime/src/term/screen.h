/* The terminal layer (docs/terminal.md): the screen model every screen function works on, and the drivers that show
 * it. One screen for the program. */
#ifndef XB_TERM_SCREEN_H
#define XB_TERM_SCREEN_H

#include <stddef.h>
#include <stdint.h>

struct xb_codepage;
struct xb_buffer;

/* Clipper's color attribute: the foreground in the low four bits (8: bright), the background in the high four (0x80:
 * blinking, or bright with SetBlink( .F. )). */
#define XB_ATTR_STANDARD 0x07u /* W/N */
#define XB_ATTR_INVERSE 0x70u  /* N/W */

/* A color pair (term/color.c): its attribute, and which sides were written U (1 the foreground, 2 the background),
 * which SetColor() writes back as U. */
typedef struct xb_color {
  uint8_t attr, underline;
} xb_color;

/* The bytes of the longest pair written ("GR+/GR*"), and one more. */
#define XB_COLOR_TEXT 8

/* The pair `text` holds (its `len` bytes, up to the comma after it), as SetColor() reads it (docs/terminal.md#colors);
 * 0 for a pair with no character of a color's (it keeps the pair there was). */
int xb_color_read(const char *text, size_t len, xb_color *out);
/* The pair as SetColor() writes it: foreground, + when bright, /, background, * when blinking. */
size_t xb_color_write(const xb_color *c, char out[XB_COLOR_TEXT]);

/* The five pairs of SetColor(), in its order. */
enum { XB_COLOR_STANDARD, XB_COLOR_ENHANCED, XB_COLOR_BORDER, XB_COLOR_BACKGROUND, XB_COLOR_UNSELECTED, XB_COLORS };

/* The screen when no driver tells another size: Clipper's. */
#define XB_SCREEN_ROWS 25
#define XB_SCREEN_COLS 80

/* A driver (term/drivers.c): what shows the model. `console` receives the console's text as the program wrote it,
 * in its codepage, under the output lock (NULL: the console shows on the screen only); `end` runs when the program
 * ends. */
typedef struct xb_term_driver {
  const char *name;
  void (*console)(const char *text, size_t len);
  void (*end)(void);
} xb_term_driver;

/* Chooses the driver of a name (the //TERM: runtime option; a name of no driver changes nothing), and ends it, once,
 * when the program ends. */
void xb_term_start(const char *name);
void xb_term_end(void);

/* The console's output (? and ??), under the output lock, which keeps the writes of threads whole and in order: the
 * driver's console gets the bytes, then the model. */
void xb_term_console_locked(const struct xb_codepage *cp, const char *text, size_t len);

/* The screen as text on the standard output (xb_screen_dump), for tests: the Dump driver's, __ScreenDump(). */
void xb_term_dump(void);

/* The operations on the model, each under the model's own lock (a word lock, held for the operation only: never
 * across I/O, so a waiter keeps no collection waiting). Positions are rows and columns from 0; the cursor may stand
 * outside the screen (SetPos takes any position). */

/* The console's output on the model: the text read in codepage `cp`, at the cursor (first brought within the
 * screen), as a terminal shows a stream: Chr(13) back to the line's start, Chr(10) to the next line's, Chr(8) one
 * column back (from a line's start, to the last column of the line before), Chr(7) nothing, any other character in
 * its cell; the line wraps once its last cell is written, and the screen scrolls up past the bottom one. A UTF-8
 * character the text ends inside of is completed by the next text. */
void xb_screen_console(const struct xb_codepage *cp, const char *text, size_t len);

/* A device's output (DevOut, DispOut, @ SAY): the text at the cursor, every character in its cell (control characters
 * too), on the cursor's row only: what lies past the screen's edges is not shown; the cursor moves past it, up to the
 * column after the last (from any column: one past the right edge comes back to it). */
void xb_screen_say(const struct xb_codepage *cp, const char *text, size_t len, unsigned attr);

/* SetColor(): the five pairs written, separated by commas, into `was` (XB_COLORS * XB_COLOR_TEXT bytes; their length
 * returned); then, unless `spec` is NULL, the pairs it gives, in order (an empty one keeps its pair; past the fifth,
 * none counts), the unselected pair following the enhanced one given without it, an empty string giving the defaults
 * (W/N,N/W,N/N,N/N,N/W), and the standard pair the selected one. */
size_t xb_screen_colors(const char *spec, size_t len, char *was);
/* The selected pair's attribute (ColorSelect()): what the output writes in and what clears and scrolls blank with. */
unsigned xb_screen_color(void);
/* The attribute a color argument (a COLOR clause, DevOut()'s color) writes in, as the colors would be set to it for
 * the output and set back after it: its first pair, the standard one when it gives none, the default when it is
 * empty; the standard pair becomes the selected one. */
unsigned xb_screen_color_of(const char *spec, size_t len);
/* ColorSelect( n ): pair n (0 to 4) becomes the selected one; another number changes nothing. */
void xb_screen_select_color(int64_t n);
/* SetBlink(): whether * is blinking (else a bright background), before `on` (1, 0; -1: unchanged) sets it. */
int xb_screen_blink(int on);

void xb_screen_cursor(int32_t *row, int32_t *col);
void xb_screen_set_cursor(int32_t row, int32_t col);
void xb_screen_size(int32_t *rows, int32_t *cols);
/* A new size (SetMode): the cells that still fit kept from the top left, new ones blank, the cursor brought within;
 * 0 when it cannot be had. */
int xb_screen_set_size(int32_t rows, int32_t cols);

/* Scroll(): the rectangle from (top, left) to (bottom, right), within the screen, moved up by `rows` (down when
 * negative) and left by `cols` (right when negative); what is uncovered, or the whole rectangle when both are 0,
 * blank in `attr`. */
void xb_screen_scroll(int32_t top, int32_t left, int32_t bottom, int32_t right, int32_t rows, int32_t cols,
                      unsigned attr);

/* DispBox(): a frame from (top, left) to (bottom, right) (in any order: the corners sorted), the part within the
 * screen: `frame` its characters, top left, top, top right, right, bottom right, bottom, bottom left, left, then the
 * one that fills the inside (0: the inside left as it is). A frame of one row is its top character, of one column its
 * right one; a character of no width or two shows as a blank. */
void xb_screen_box(int32_t top, int32_t left, int32_t bottom, int32_t right, const uint32_t frame[9], unsigned attr);

/* SaveScreen() and RestScreen(): the rectangle from (top, left) to (bottom, right), its corners sorted and brought
 * within the screen, as Clipper's string of it: two bytes a cell, row by row, its character (in `cp`, a single-byte
 * codepage, or CP437 for UTF-8: '?' when it has none) and its attribute; then, when cells hold what that byte does not
 * give back (another code point, a mark, a half of a wide character), records of them, each byte of theirs in a
 * character byte (its attribute byte 0, so that changing the attribute bytes leaves them), with their count and a
 * hash (term/screen.c). The string's bytes come back on the rectangle's cells row by row (bytes past its end as zeros:
 * Chr(0) in N/N), then the records that are whole and whose cells' character bytes are as saved; the cursor stays. */
void xb_screen_save(int32_t top, int32_t left, int32_t bottom, int32_t right, const struct xb_codepage *cp,
                    struct xb_buffer *out);
void xb_screen_restore(int32_t top, int32_t left, int32_t bottom, int32_t right, const struct xb_codepage *cp,
                       const char *text, size_t len);

/* DispBegin(), DispEnd(): the depth of held updates changed by `by` (never below 0), returned (DispCount() with 0). A
 * driver shows the screen once it is 0 again. */
int32_t xb_screen_hold(int32_t by);
/* SetCursor(): the cursor's shape (0 none, 1 normal, 2 insert, 3 and 4 special), before `shape` (-1: unchanged;
 * another, normal) sets it. */
int xb_screen_cursor_shape(int shape);

/* The screen as text into `out`: its size and the cursor, each row's characters (UTF-8, trailing blanks left out, a
 * control character as its Unicode picture), then the rows whose colors are not all W/N, as runs of attribute*count. */
void xb_screen_dump(struct xb_buffer *out);

#endif
