/* The terminal drivers (docs/terminal.md): Stream, the console's text to the standard output as it was written (no
 * screen shows: the screen functions keep the model), and Dump, the screen written as text when the program ends. */
#include "term/screen.h"

#include "core/codepage.h"
#include "xbrt_int.h"

#include <stdio.h>
#include <string.h>

static void stream_console(const char *text, size_t len) { fwrite(text, 1, len, stdout); }

static const xb_term_driver kStream = {"STREAM", stream_console, NULL};
static const xb_term_driver kDump = {"DUMP", NULL, xb_term_dump};
static const xb_term_driver *const kDrivers[] = {&kStream, &kDump};

static const xb_term_driver *driver = &kStream;
static int ended;

void xb_term_start(const char *name) {
  for (size_t k = 0; k < sizeof kDrivers / sizeof kDrivers[0]; ++k)
    if (xb_name_is(name, strlen(name), kDrivers[k]->name)) driver = kDrivers[k];
}

void xb_term_end(void) {
  if (ended) return;
  ended = 1;
  if (driver->end) driver->end();
}

void xb_term_console_locked(const xb_codepage *cp, const char *text, size_t len) {
  if (driver->console) driver->console(text, len);
  xb_screen_console(cp, text, len);
}

static void write_dump(void *p) {
  const xb_buffer *text = p;
  xb_output_lock();
  fwrite(text->p, 1, text->len, stdout);
  xb_platform_flush(stdout);
  xb_output_unlock();
}

void xb_term_dump(void) {
  xb_buffer text = {0};
  xb_screen_dump(&text);
  xb_blocking_run(write_dump, &text);
  xb_buffer_free(&text);
}
