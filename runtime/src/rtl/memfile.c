/* SAVE and RESTORE: memory variables in .mem files, Clipper's format.
 *
 * A file is a record per variable, then the byte 0x1A. A record is a 32-byte
 * header, then the value:
 *   0..10   the name (upper case, its first 10 characters), padded with NULs
 *   11      the type, 'C', 'N', 'D' or 'L', plus 0x80
 *   16..17  C: the length of the value, its closing NUL included (16 bits,
 *           little-endian); N: the width (the integer columns, and the point
 *           and decimals when there are decimals), then the decimals; D and
 *           L: 1, then 0
 *   12..15 and 18..31 zero
 * The value: C the string's bytes and a NUL (65534 bytes of the string at
 * most); N an IEEE double, little-endian; D the Julian day number as such a
 * double (0: the empty date); L one byte, 1 or 0. Values of other types are
 * not saved. */
#include "xbrt_int.h"

#include "core/file.h"

#include <stdlib.h>
#include <string.h>

#define MEM_HEADER 32
#define MEM_NAME_MAX 10
#define MEM_STRING_MAX 65534 /* the length field counts the NUL too */
#define MEM_END 0x1A

/* A growing buffer of bytes. */
typedef struct bytes {
  unsigned char *p;
  size_t len, cap;
} bytes;

static unsigned char *grow(bytes *b, size_t n) {
  if (b->cap - b->len < n) {
    size_t cap = b->cap ? b->cap : 4096;
    while (cap - b->len < n) cap *= 2;
    unsigned char *p = realloc(b->p, cap);
    if (!p) xb_gc_fatal("out of memory");
    b->p = p;
    b->cap = cap;
  }
  unsigned char *at = b->p + b->len;
  b->len += n;
  return at;
}

/* A record's header, followed by room for a value of `size` bytes: the type
 * `type`, the name's first characters, zeros elsewhere. */
static unsigned char *put_header(bytes *out, const char *name, size_t len, char type, size_t size) {
  unsigned char *h = grow(out, MEM_HEADER + size);
  memset(h, 0, MEM_HEADER);
  memcpy(h, name, len < MEM_NAME_MAX ? len : MEM_NAME_MAX);
  h[11] = (unsigned char)(type | 0x80);
  return h;
}

/* The record of `v` named `name`; none for a type the format does not hold. */
static void put_record(bytes *out, const char *name, size_t len, const xb_item *v) {
  unsigned char *h;
  switch ((xb_type)v->type) {
  case XB_T_STRING: {
    size_t n = v->v.s->len < MEM_STRING_MAX ? v->v.s->len : MEM_STRING_MAX;
    h = put_header(out, name, len, 'C', n + 1);
    xb_put_le16(h + 16, (uint16_t)(n + 1));
    memcpy(h + MEM_HEADER, v->v.s->data, n);
    h[MEM_HEADER + n] = 0;
    break;
  }
  case XB_T_INT:
  case XB_T_DOUBLE: {
    int isInt = v->type == XB_T_INT;
    unsigned width = v->width ? v->width : isInt ? xb_num_int_width(v->v.i) : xb_num_dbl_width(v->v.d);
    width += v->dec ? v->dec + 1u : 0u;
    h = put_header(out, name, len, 'N', 8);
    h[16] = (unsigned char)(width < 255 ? width : 255);
    h[17] = (unsigned char)(v->dec < 255 ? v->dec : 255);
    xb_put_le_double(h + MEM_HEADER, isInt ? (double)v->v.i : v->v.d);
    break;
  }
  case XB_T_DATE:
    h = put_header(out, name, len, 'D', 8);
    h[16] = 1;
    xb_put_le_double(h + MEM_HEADER, (double)v->v.i);
    break;
  case XB_T_LOGICAL:
    h = put_header(out, name, len, 'L', 1);
    h[16] = 1;
    h[MEM_HEADER] = v->v.l ? 1 : 0;
    break;
  default:
    break;
  }
}

/* __MemSave( cFile, [cSkeleton], [lLike] ): SAVE TO cFile [ALL [LIKE |
 * EXCEPT skeleton]]: the variables visible whose names match the skeleton
 * ("*" by default) with lLike (the default), or do not without it. No file
 * name is error 2008 (EG_ARG). A file that cannot be written is error 2006
 * (EG_CREATE when it cannot be created, EG_WRITE when writing it failed),
 * which may be retried; the default gives up, leaving a file that was
 * created or partly written as it is. */
void xbf___MEMSAVE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  if (argc < 1 || argv[0].type != XB_T_STRING) {
    xb_error_in(ctx, "__MEMSAVE", NULL, EG_ARG, 2008, "__MEMSAVE", 0, argc, argv);
    return;
  }
  xb_str *all = xb_str_new("*", 1);
  const xb_str *skel = argc > 1 && argv[1].type == XB_T_STRING ? argv[1].v.s : all;
  int like = argc > 2 && argv[2].type == XB_T_LOGICAL ? argv[2].v.l != 0 : 1;
  bytes out = {0};
  for (uint32_t id = 0; id < xb_memvar_count(); ++id) {
    size_t len;
    const char *name = xb_memvar_name(id, &len);
    if (xb_memvar_like(skel, name, len) != like) continue;
    const xb_item *v = xb_memvar_value(ctx, id);
    if (ctx->pending & XB_PENDING_UNWIND) {
      free(out.p);
      return;
    }
    if (v) put_record(&out, name, len, v);
  }
  *grow(&out, 1) = MEM_END;
  char *path = xb_file_path_with_ext(argv[0].v.s->data, strlen(argv[0].v.s->data), ".mem");
  for (uint16_t tries = 1;; ++tries) {
    xb_file f;
    int32_t err = 0;
    int32_t gencode = EG_CREATE;
    if (xb_platform_file_create(path, FC_NORMAL, &f, &err)) {
      int written = xb_platform_file_write(f, out.p, out.len, &err);
      int32_t closeErr = 0;
      int closed = xb_platform_file_close(f, &closeErr);
      if (written && closed) break;
      if (written) err = closeErr;
      gencode = EG_WRITE;
    }
    if (xb_error_file(ctx, "__MEMSAVE", gencode, 2006, argv[0].v.s->data, err, tries) != 2) break;
  }
  free(path);
  free(out.p);
}

/* Reads `n` bytes, unless the file ends or fails first: whether it read them. */
static int read_all(xb_file f, unsigned char *buf, size_t n) {
  int32_t err;
  return xb_file_read_up_to(f, buf, n, &err) == (int64_t)n;
}

/* The next record's value in `*v` and its name: 0 at the end of the file (or
 * at a record it cannot read). */
static int get_record(xb_file f, xb_item *v, char *name, size_t *len) {
  unsigned char h[MEM_HEADER], num[8];
  if (!read_all(f, h, 1) || h[0] == MEM_END || !read_all(f, h + 1, MEM_HEADER - 1)) return 0;
  for (*len = 0; *len < MEM_NAME_MAX && h[*len]; ++*len) name[*len] = (char)h[*len];
  switch (h[11] & 0x7F) {
  case 'C': {
    size_t n = xb_get_le16(h + 16);
    unsigned char *text = malloc(n ? n : 1);
    if (!text) xb_gc_fatal("out of memory");
    int ok = read_all(f, text, n);
    if (ok) xb_set_str(v, xb_str_new((const char *)text, n ? n - 1 : 0)); /* without the NUL */
    free(text);
    return ok;
  }
  case 'N': {
    if (!read_all(f, num, 8)) return 0;
    double d = xb_get_le_double(num);
    uint16_t dec = h[17], width = h[16];
    uint16_t intWidth = (uint16_t)(width > (dec ? dec + 1 : 0) ? width - (dec ? dec + 1 : 0) : 0);
    uint64_t bits;
    memcpy(&bits, &d, 8);
    if (dec == 0 && xb_dbl_fits_i64(d) && (double)(int64_t)d == d)
      xb_put(v, xb_header_word(XB_T_INT, intWidth, 0), (uint64_t)(int64_t)d);
    else
      xb_put(v, xb_header_word(XB_T_DOUBLE, intWidth, dec), bits);
    return 1;
  }
  case 'D':
    if (!read_all(f, num, 8)) return 0;
    xb_set_date(v, xb_dbl_to_i64(xb_get_le_double(num)));
    return 1;
  case 'L':
    if (!read_all(f, num, 1)) return 0;
    xb_set_logical(v, num[0] != 0);
    return 1;
  default:
    return 0;
  }
}

/* __MemRestore( cFile, lAdditive ): RESTORE FROM cFile [ADDITIVE]: each
 * variable of the file takes its value, a new PRIVATE of the function calling
 * this one when no variable of that name is visible. Without lAdditive, no
 * PRIVATE or PUBLIC exists before (CLEAR MEMORY), once the file is open. No
 * file name is error 2007 (EG_ARG). A file that cannot be opened is error
 * 2005 (EG_OPEN), which may be retried; the default leaves the variables as
 * they were. Reading stops at the end mark (0x1A), at the end of the file, or
 * at a record it cannot read. */
void xbf___MEMRESTORE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  if (argc < 1 || argv[0].type != XB_T_STRING) {
    xb_error_in(ctx, "__MEMRESTORE", NULL, EG_ARG, 2007, "__MEMRESTORE", 0, argc, argv);
    return;
  }
  int additive = argc > 1 && argv[1].type == XB_T_LOGICAL && argv[1].v.l;
  char *path = xb_file_path_with_ext(argv[0].v.s->data, strlen(argv[0].v.s->data), ".mem");
  xb_file f;
  int opened = 0;
  for (uint16_t tries = 1; !opened; ++tries) {
    int32_t err = 0;
    opened = xb_platform_file_open(path, FO_READ, &f, &err);
    if (!opened && xb_error_file(ctx, "__MEMRESTORE", EG_OPEN, 2005, argv[0].v.s->data, err, tries) != 2) break;
  }
  free(path);
  if (!opened) return;
  if (!additive) xb_memvar_clear(ctx);
  xb_item v; /* a new string may be its only reference while an error block runs */
  xb_set_nil(&v);
  xb_gc_root_push(ctx, &v);
  char name[MEM_NAME_MAX];
  size_t len;
  while (!(ctx->pending & XB_PENDING_UNWIND) && get_record(f, &v, name, &len))
    if (len) xb_memvar_assign(ctx, name, len, &v);
  xb_gc_root_pop(ctx, 1);
  int32_t err;
  xb_platform_file_close(f, &err);
}
