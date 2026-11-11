/*
 * language.ch - the parts of a language's texts, the array that
 * __LanguageTexts() (rtl/languages.prg) returns.
 *
 * Only #define lines and C comments: the runtime's C includes this same file,
 * so the positions are defined once for xBase code and for the runtime.
 */

#ifndef XB_LANGUAGE_CH
#define XB_LANGUAGE_CH

#define LANG_ID      1 /* its name, as SET LANGUAGE TO takes it */
#define LANG_DAYS    2 /* the names of the days, from Sunday */
#define LANG_MONTHS  3 /* the names of the months */
#define LANG_ERRORS  4 /* a hash: the description of each generic error code (error.ch) */
#define LANG_SUBCODES 5 /* a hash: for a generic code, a hash of the descriptions of some of its subcodes */

#endif
