/* Byte-level text rules shared by the runtime (the ASCII letter case rule,
 * xb_ascii_upper and xb_ascii_lower, is in xbrt.h). */
#ifndef XB_TEXT_H
#define XB_TEXT_H

/* The blanks that leading trimming, Empty() and Val() skip. */
static inline int xb_is_blank(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

#endif
