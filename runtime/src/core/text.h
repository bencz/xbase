/* Byte-level text rules shared by the runtime. Case changes and comparisons
 * of keywords only touch the ASCII letters; other bytes pass unchanged. */
#ifndef XB_TEXT_H
#define XB_TEXT_H

static inline char xb_ascii_upper(char c) { return (char)(c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c); }
static inline char xb_ascii_lower(char c) { return (char)(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c); }

/* The blanks that leading trimming, Empty() and Val() skip. */
static inline int xb_is_blank(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

#endif
