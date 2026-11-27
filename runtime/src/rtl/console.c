#include "xbrt_int.h"

#include "core/codepage.h"
#include "term/screen.h"

#include <stdio.h>

/* What one call writes: a new line first or not, then the display forms of the values separated by spaces. The bytes
 * are gathered first (a number's display is a new string, which a collection may move: copied as soon as it is
 * made), then written at once under the output lock (no other thread's output splits them), as a blocking operation
 * (the system may hold it while a pipe is full). */

typedef struct text_write {
  const xb_buffer *text;
  FILE *out;                    /* a standard stream, or NULL: the console (term/screen.h) */
  const xb_codepage *codepage; /* the console's: how its text reads */
} text_write;

static void run_write(void *p) {
  const text_write *w = p;
  xb_output_lock();
  if (!w->out) xb_term_console_locked(w->codepage, w->text->p, w->text->len);
  else {
    if (w->out == stderr) fflush(stdout); /* the two keep their order on a terminal */
    fwrite(w->text->p, 1, w->text->len, w->out);
  }
  xb_output_unlock();
}

static void write_list(xb_ctx *ctx, FILE *out, int newLine, uint32_t argc, const xb_item *argv) {
  char in_place[XB_TEXT_IN_PLACE];
  xb_buffer text = {in_place, 0, sizeof in_place, 0};
  if (newLine) xb_buffer_put(&text, "\n", 1);
  for (uint32_t k = 0; k < argc; ++k) {
    if (k) xb_buffer_put(&text, " ", 1);
    const xb_str *s = xb_item_display(ctx, &argv[k]);
    xb_buffer_put(&text, xb_str_data(s), s->len);
  }
  if (text.len) {
    text_write w = {&text, out, xb_codepage_of(ctx)};
    xb_blocking_run(run_write, &w);
  }
  xb_buffer_free(&text);
}

/* __ConsoleWrite( lNewLine, [xValues...] ): a new line when lNewLine is .T., then the display forms of the values
 * (a string as it is, numbers, dates and logicals as QOut() shows them) separated by spaces, to the console: the
 * screen at its cursor, and the driver's console (the Stream driver's: the standard output). */
void xbf___CONSOLEWRITE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  if (!argc) return;
  write_list(ctx, NULL, argv[0].type == XB_T_LOGICAL && argv[0].v.l, argc - 1, argv + 1);
}

/* __StdWrite( nHandle, [xValues...] ): the display forms of the values separated by spaces, to the standard output
 * (handle 1) or the standard error (handle 2), never to the console; any other handle is ignored. The standard output
 * is flushed before the standard error is written, so the two keep their order on a terminal. */
void xbf___STDWRITE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  if (!argc || !xb_is_number(&argv[0])) return;
  int64_t handle = xb_num_to_i64(&argv[0]);
  if (handle != 1 && handle != 2) return;
  write_list(ctx, handle == 1 ? stdout : stderr, 0, argc - 1, argv + 1);
}
