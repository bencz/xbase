/*
 * set.ch - setting numbers for Set( nSetting [, xNewValue] ).
 *
 * Only #define lines and C comments: the runtime (C) includes this same file,
 * so the numbers are defined once for xBase code and for the runtime.
 * The numbers are the Clipper ones, and xbase's own settings from 200; only
 * settings the runtime implements are listed.
 */

#ifndef XB_SET_CH
#define XB_SET_CH

#define _SET_EXACT      1
#define _SET_DECIMALS   3
#define _SET_DATEFORMAT 4
#define _SET_EPOCH      5
#define _SET_DELETED    11

#define _SET_LANGUAGE   200 /* the language of the runtime's texts: "EN" (the default), "PT", "ES" */
#define _SET_CODEPAGE   201 /* how the text functions read strings: "UTF8" (the default), "CP437", "CP850", "CP1252",
                               "ISO8859-1" */

#endif /* XB_SET_CH */
