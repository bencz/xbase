#include "xbrt_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static xb_seg *seg_alloc(xb_seg *prev, size_t cap) {
  xb_seg *s = malloc(sizeof(xb_seg) + cap * sizeof(xb_item));
  if (!s) {
    fputs("xbrt: out of memory (frame stack)\n", stderr);
    abort();
  }
  s->prev = prev;
  s->next = NULL;
  s->cap = cap;
  s->used = 0;
  return s;
}

xb_ctx *xb_ctx_new(void) {
  xb_ctx *ctx = calloc(1, sizeof *ctx);
  if (!ctx) abort();
  ctx->seg = seg_alloc(NULL, XB_SEG_DEFAULT);
  ctx->set_decimals = 2;
  ctx->set_exact = 0;
  return ctx;
}

void xb_ctx_free(xb_ctx *ctx) {
  if (!ctx) return;
  xb_seg *s = ctx->seg;
  while (s->next) s = s->next;
  while (s) {
    xb_seg *p = s->prev;
    free(s);
    s = p;
  }
  free(ctx);
}

uint32_t xb_ctx_pending(const xb_ctx *ctx) { return ctx->pending; }
void xb_ctx_raise(xb_ctx *ctx, uint32_t what) { ctx->pending |= what; }
int xb_ctx_exitcode(const xb_ctx *ctx) { return ctx->exitcode; }

xb_item *xb_frame_push(xb_ctx *ctx, uint32_t n) {
  xb_seg *s = ctx->seg;
  if (s->cap - s->used < n) {
    xb_seg *nx = s->next;
    if (!nx || nx->cap < n) {
      size_t cap = n > XB_SEG_DEFAULT ? n : XB_SEG_DEFAULT;
      xb_seg *fresh = seg_alloc(s, cap);
      if (nx) { /* spare too small: drop it and everything after it */
        while (nx) { xb_seg *t = nx->next; free(nx); nx = t; }
      }
      s->next = fresh;
      nx = fresh;
    }
    nx->used = 0;
    ctx->seg = s = nx;
  }
  xb_item *base = s->items + s->used;
  memset(base, 0, n * sizeof(xb_item));   /* all-zero == NIL */
  s->used += n;
  return base;
}

void xb_frame_pop(xb_ctx *ctx, xb_item *base) {
  xb_seg *s = ctx->seg;
  while (!(base >= s->items && base <= s->items + s->used)) {
    s->used = 0;
    s = s->prev;
    if (!s) {
      fputs("xbrt: frame stack corrupted\n", stderr);
      abort();
    }
  }
  s->used = (size_t)(base - s->items);
  ctx->seg = s;
}

void xb_args_copy(xb_item *dst, uint32_t nparams, uint32_t argc, const xb_item *argv) {
  uint32_t n = argc < nparams ? argc : nparams;
  if (n) memcpy(dst, argv, n * sizeof(xb_item));
}

void xb_abi_check(uint32_t version, uint32_t item_size, uint32_t str_hdr_size) {
  if (version != XB_ABI_VERSION || item_size != sizeof(xb_item) ||
      str_hdr_size != offsetof(xb_str, data)) {
    fprintf(stderr,
            "xbrt: ABI mismatch (module v%u item=%u str=%u, runtime v%u item=%u str=%u)\n",
            version, item_size, str_hdr_size, XB_ABI_VERSION,
            (unsigned)sizeof(xb_item), (unsigned)offsetof(xb_str, data));
    abort();
  }
}

void xb_rt_error(xb_ctx *ctx, const char *subsystem, int code, const char *desc,
                 const char *operation) {
  /* Until ErrorBlock()/BEGIN SEQUENCE exist, every error is fatal. */
  fflush(stdout);
  fprintf(stderr, "\nError %s/%d  %s", subsystem, code, desc);
  if (operation && *operation) fprintf(stderr, ": %s", operation);
  fputc('\n', stderr);
  ctx->exitcode = 1;
  ctx->pending |= XB_PENDING_ERROR;
}
