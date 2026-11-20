/* The language of the runtime's texts (SET LANGUAGE): the context keeps the current language's texts, which
 * __LanguageTexts() (rtl/languages.prg) makes; English until a program selects another. */
#include "xbrt_int.h"

#include "rtl/language.ch"

/* rtl/languages.prg */
void xbf___LANGUAGETEXTS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

/* Makes the texts of language `id` into `*texts` (rooted by the caller): 0 when it could not. */
static int make_texts(xb_ctx *ctx, const char *id, size_t len, xb_item *texts) {
  xb_item arg;
  xb_str *upper = xb_str_alloc(len);
  for (size_t i = 0; i < len; ++i) xb_str_buffer(upper)[i] = xb_ascii_upper(id[i]);
  xb_set_str(&arg, upper);
  xb_set_nil(texts);
  xb_gc_root_push(ctx, &arg);
  xbf___LANGUAGETEXTS(ctx, texts, 1, &arg);
  xb_gc_root_pop(ctx, 1);
  return !(xb_pending_get(ctx) & XB_PENDING_UNWIND) && texts->type == XB_T_ARRAY;
}

int xb_language_select(xb_ctx *ctx, const char *id, size_t len) {
  xb_item texts;
  xb_set_nil(&texts);
  xb_gc_root_push(ctx, &texts);
  int made = make_texts(ctx, id, len, &texts);
  if (made) ctx->env->language = texts;
  xb_gc_root_pop(ctx, 1);
  return made;
}

/* The English texts, made once for the process (the texts are never changed): every environment that starts without
 * texts takes them, shared, rather than making them again. `english` is a root of the collector once made, set under
 * `englishLock` and read without it once `englishReady` says so. */
static xb_item english;
static uint32_t englishReady;
static xb_mutex englishLock;
static xb_once_flag englishMade = XB_ONCE_INIT;

static void english_root(void) {
  if (!xb_mutex_init(&englishLock)) xb_gc_fatal("cannot create the language's lock");
  xb_set_nil(&english);
  xb_gc_add_root(&english, 1);
}

/* The texts of the current language: an environment without any takes the English ones, made once (a language a
 * destructor selected while they were made stays). */
const xb_array *xb_language(xb_ctx *ctx) {
  if (ctx->env->language.type != XB_T_ARRAY && xb_atomic_load32(&englishReady, XB_ACQUIRE)) {
    ctx->env->language = english;
  } else if (ctx->env->language.type != XB_T_ARRAY) {
    xb_item texts;
    xb_set_nil(&texts);
    xb_gc_root_push(ctx, &texts);
    if (make_texts(ctx, "EN", 2, &texts)) {
      xb_once(&englishMade, english_root);
      xb_share(&texts); /* before another thread may reach them */
      xb_mutex_lock(&englishLock);
      if (!xb_atomic_load32(&englishReady, XB_RELAXED)) {
        english = texts;
        xb_atomic_store32(&englishReady, 1, XB_RELEASE);
      }
      xb_mutex_unlock(&englishLock);
      if (ctx->env->language.type != XB_T_ARRAY) ctx->env->language = texts;
    }
    xb_gc_root_pop(ctx, 1);
  }
  return ctx->env->language.type == XB_T_ARRAY ? ctx->env->language.v.p : NULL;
}

/* The value of numeric key `key` in a hash item, or NULL (not a hash, or no such key). */
static int find_number(const xb_item *hash, int64_t key, xb_item *out) {
  if (hash->type != XB_T_HASH) return 0;
  xb_item k;
  xb_set_int(&k, key);
  return xb_hash_get(hash->v.p, &k, out); /* the table may be shared: read whole */
}

/* The description of an error in the current language, by its subcode's own text, else its generic code's. */
xb_str *xb_language_error(xb_ctx *ctx, int32_t gencode, int32_t subcode) {
  const xb_array *texts = xb_language(ctx);
  if (!texts || xb_array_length(texts) < LANG_SUBCODES) return NULL;
  xb_item part, subcodes, text; /* the table, a root, keeps them */
  int found = xb_array_get(texts, LANG_SUBCODES - 1, &part) && find_number(&part, gencode, &subcodes) &&
              find_number(&subcodes, subcode, &text) && text.type == XB_T_STRING;
  if (!found)
    found = xb_array_get(texts, LANG_ERRORS - 1, &part) && find_number(&part, gencode, &text) &&
            text.type == XB_T_STRING;
  return found ? text.v.s : NULL;
}

/* __LanguageCurrent(): the texts of the current language (language.ch numbers their parts), the ones the runtime
 * reads: never to be changed. */
void xbf___LANGUAGECURRENT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  if (xb_language(ctx)) *ret = ctx->env->language;
}
