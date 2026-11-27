/* Clipper's color strings (docs/terminal.md#colors): reading a pair into an attribute, and writing one back. A pair
 * is read in order over one attribute, a side at a time (the foreground first): what each character does depends on
 * what came before it. */
#include "term/screen.h"

#include "xbrt_int.h"

#include <string.h>

/* The names of the eight colors, by their bits (blue 1, green 2, red 4). */
static const char *const kNames[8] = {"N", "B", "G", "BG", "R", "BR", "GR", "W"};

typedef struct reading {
  unsigned side[2];   /* the colors of the foreground (0) and the background (1) */
  int underline[2];   /* the side was written U last */
  int at;             /* the side the next character is for */
  int bright, blink;
} reading;

static void set_side(reading *r, unsigned value) {
  r->side[r->at] = value & 7u;
  r->underline[r->at] = 0;
}

/* A character that is a color's: what it does (1), or 0 for any other, which counts for nothing. */
static int color_char(reading *r, char c) {
  switch (c) {
  case 'N': set_side(r, 0); return 1;
  case 'B': set_side(r, r->side[r->at] | 1u); return 1;
  case 'G': set_side(r, r->side[r->at] | 2u); return 1;
  case 'R': set_side(r, r->side[r->at] | 4u); return 1;
  case 'W': set_side(r, 7); return 1;
  case 'U': /* blue, written U: a letter after it is that letter's color */
    set_side(r, 1);
    r->underline[r->at] = 1;
    return 1;
  case 'X': /* both sides black, the brightness and the blinking kept */
    r->side[0] = r->side[1] = 0;
    r->underline[0] = r->underline[1] = 0;
    return 1;
  case 'I': /* black on white so far, the background read next */
    r->side[0] = 0;
    r->side[1] = 7;
    r->underline[0] = r->underline[1] = 0;
    r->at = 1;
    return 1;
  case '/': /* past the background, the background's colors become the foreground's */
    if (r->at) {
      r->side[0] = r->side[1];
      r->underline[0] = r->underline[1];
      r->side[1] = 0;
      r->underline[1] = 0;
    }
    r->at = 1;
    return 1;
  case '+': r->bright = 1; return 1;
  case '*': r->blink = 1; return 1;
  default: return 0;
  }
}

int xb_color_read(const char *text, size_t len, xb_color *out) {
  reading r = {{0, 0}, {0, 0}, 0, 0, 0};
  int written = 0;
  for (size_t i = 0; i < len; ++i) {
    char c = xb_ascii_upper(text[i]);
    if (c >= '0' && c <= '9') { /* a number: its low three bits; its 8 brightens or blinks, as + or * would */
      unsigned n = 0;
      for (; i < len && text[i] >= '0' && text[i] <= '9'; ++i) n = (n * 10 + (unsigned)(text[i] - '0')) & 0xFFu;
      --i;
      set_side(&r, n);
      if (n & 8u) *(r.at ? &r.blink : &r.bright) = 1;
      written = 1;
    } else {
      written |= color_char(&r, c);
    }
  }
  if (!written) return 0;
  out->attr = (uint8_t)(r.side[1] << 4 | (r.blink ? 0x80u : 0) | r.side[0] | (r.bright ? 8u : 0));
  out->underline = (uint8_t)((r.underline[0] ? 1 : 0) | (r.underline[1] ? 2 : 0));
  return 1;
}

size_t xb_color_write(const xb_color *c, char out[XB_COLOR_TEXT]) {
  const char *fg = c->underline & 1 ? "U" : kNames[c->attr & 7u];
  const char *bg = c->underline & 2 ? "U" : kNames[(c->attr >> 4) & 7u];
  size_t n = 0;
  memcpy(out + n, fg, strlen(fg));
  n += strlen(fg);
  if (c->attr & 8u) out[n++] = '+';
  out[n++] = '/';
  memcpy(out + n, bg, strlen(bg));
  n += strlen(bg);
  if (c->attr & 0x80u) out[n++] = '*';
  return n;
}
