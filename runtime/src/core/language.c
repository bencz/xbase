/* The language of the runtime's texts (SET LANGUAGE): the context keeps the current language's texts, which
 * __LanguageTexts() (rtl/languages.prg) makes; English until a program selects another. */
#include "xbrt_int.h"

#include "rtl/language.ch"

/* rtl/languages.prg */
void xbf___LANGUAGETEXTS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

/* Makes the texts of language `id`; stores them unless `onlyFirst` and a language was selected meanwhile (a
 * destructor that ran while they were made). */
static int select_language(xb_ctx *ctx, const char *id, size_t len, int onlyFirst) {
  xb_item arg, texts;
  xb_str *upper = xb_str_alloc(len);
  for (size_t i = 0; i < len; ++i) upper->data[i] = xb_ascii_upper(id[i]);
  xb_set_str(&arg, upper);
  xb_set_nil(&texts);
  xb_gc_root_push(ctx, &arg);
  xb_gc_root_push(ctx, &texts);
  xbf___LANGUAGETEXTS(ctx, &texts, 1, &arg);
  xb_gc_root_pop(ctx, 2);
  if ((xb_pending_get(ctx) & XB_PENDING_UNWIND) || texts.type != XB_T_ARRAY) return 0;
  if (!onlyFirst || ctx->env->language.type != XB_T_ARRAY) ctx->env->language = texts;
  return 1;
}

int xb_language_select(xb_ctx *ctx, const char *id, size_t len) { return select_language(ctx, id, len, 0); }

const xb_array *xb_language(xb_ctx *ctx) {
  if (ctx->env->language.type != XB_T_ARRAY) select_language(ctx, "EN", 2, 1);
  return ctx->env->language.type == XB_T_ARRAY ? ctx->env->language.v.p : NULL;
}

/* The value of numeric key `key` in a hash item, or NULL (not a hash, or no such key). */
static const xb_item *find_number(const xb_item *hash, int64_t key) {
  if (hash->type != XB_T_HASH) return NULL;
  const xb_hash *h = hash->v.p;
  xb_item k;
  xb_set_int(&k, key);
  int64_t e = xb_hash_find(h, &k);
  return e < 0 ? NULL : &h->entries[e].value;
}

/* The description of an error in the current language, by its subcode's own text, else its generic code's. */
xb_str *xb_language_error(xb_ctx *ctx, int32_t gencode, int32_t subcode) {
  const xb_array *texts = xb_language(ctx);
  if (!texts || texts->len < LANG_SUBCODES) return NULL;
  const xb_item *subcodes = find_number(&texts->items[LANG_SUBCODES - 1], gencode);
  const xb_item *text = subcodes ? find_number(subcodes, subcode) : NULL;
  if (!text || text->type != XB_T_STRING) text = find_number(&texts->items[LANG_ERRORS - 1], gencode);
  return text && text->type == XB_T_STRING ? text->v.s : NULL;
}

/* __LanguageCurrent(): the texts of the current language (language.ch numbers their parts), the ones the runtime
 * reads: never to be changed. */
void xbf___LANGUAGECURRENT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  if (xb_language(ctx)) *ret = ctx->env->language;
}
