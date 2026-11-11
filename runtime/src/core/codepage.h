/* Codepages (SET CODEPAGE): how the text functions read the bytes of a string, as UTF-8 (the default) or as one of
 * the single-byte codepages of DOS and Windows. Strings stay bytes: Len(), SubStr() and the rest count bytes; the
 * case functions, the character classes and the Char*() functions read characters. */
#ifndef XB_CODEPAGE_H
#define XB_CODEPAGE_H

#include <stddef.h>
#include <stdint.h>

/* The generated data (core/codepage_data.c, which describes it). */
typedef struct xb_cp_range {
  uint32_t first, last;
  uint8_t kind; /* XB_CP_UPPER, XB_CP_LOWER or XB_CP_LETTER */
} xb_cp_range;
typedef struct xb_cp_case {
  uint32_t first, last;
  uint32_t stride;
  int32_t delta;
} xb_cp_case;
/* A codepage: UTF-8 (no tables) or a single-byte one, whose tables have an entry per byte. */
typedef struct xb_codepage {
  const char *name;
  const uint32_t *chars;
  const uint8_t *upper, *lower, *kind;
} xb_codepage;
extern const xb_cp_range xb_cp_letters[];
extern const size_t xb_cp_letters_count;
extern const xb_cp_case xb_cp_upper[], xb_cp_lower[];
extern const size_t xb_cp_upper_count, xb_cp_lower_count;
extern const xb_codepage xb_cp_singles[];
extern const size_t xb_cp_singles_count;

/* What a character is. */
enum { XB_CP_NONE, XB_CP_UPPER, XB_CP_LOWER, XB_CP_LETTER };

/* The codepage of a name (any letter case; "UTF-8", "IBM850", "WINDOWS-1252", "LATIN1" ... are other names of
 * UTF8, CP850, CP1252, ISO8859-1 ...), NULL for an unknown one. xb_codepage_utf8: UTF-8. */
const xb_codepage *xb_codepage_find(const char *name, size_t len);
const xb_codepage *xb_codepage_utf8(void);
/* The codepage of a context (SET CODEPAGE). */
#define xb_codepage_of(ctx) ((ctx)->codepage ? (ctx)->codepage : xb_codepage_utf8())

/* The character at byte `at` of `s` (`len` bytes): its code point (U+FFFD for a byte that starts no valid UTF-8
 * sequence, which counts as a character of its own), and in `*n` its length in bytes. */
uint32_t xb_cp_decode(const xb_codepage *cp, const char *s, size_t len, size_t at, size_t *n);
/* Code point `c` in the codepage, in `out`: its length, 0 when the codepage has no such character. */
size_t xb_cp_encode(const xb_codepage *cp, uint32_t c, char out[4]);
/* What the character at the start of `s` is (XB_CP_NONE for an empty string). */
int xb_cp_kind(const xb_codepage *cp, const char *s, size_t len);
/* Changes the case of the characters with a byte above 0x7F in place: the string keeps its length. */
void xb_cp_change_case(const xb_codepage *cp, char *s, size_t len, int upper);
/* The characters of `s`; the byte offset after the first `chars` of them (`len` when it has fewer). */
size_t xb_cp_count(const xb_codepage *cp, const char *s, size_t len);
size_t xb_cp_offset(const xb_codepage *cp, const char *s, size_t len, size_t chars);

#endif
