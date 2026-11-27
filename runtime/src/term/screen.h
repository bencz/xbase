/* The terminal layer (docs/terminal.md): the screen model every screen function works on, and the drivers that show
 * it. One screen for the program. */
#ifndef XB_TERM_SCREEN_H
#define XB_TERM_SCREEN_H

#include <stddef.h>
#include <stdint.h>

struct xb_codepage;
struct xb_buffer;

/* Clipper's color attribute: the foreground in the low four bits, the background in the high four. */
#define XB_ATTR_STANDARD 0x07u /* W/N */

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

/* The screen as text into `out`: its size and the cursor, each row's characters (UTF-8, trailing blanks left out, a
 * control character as its Unicode picture), then the rows whose colors are not all W/N, as runs of attribute*count. */
void xb_screen_dump(struct xb_buffer *out);

#endif
