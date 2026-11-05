/*
 * set.ch - setting numbers for Set( nSetting [, xNewValue] ).
 *
 * Only #define lines and C comments: the runtime (C) includes this same file,
 * so the numbers are defined once for xBase code and for the runtime.
 * The numbers are the Clipper ones; only settings the runtime implements are
 * listed.
 */

#ifndef _SET_CH
#define _SET_CH

#define _SET_EXACT      1
#define _SET_DECIMALS   3
#define _SET_DATEFORMAT 4
#define _SET_EPOCH      5

#endif /* _SET_CH */
