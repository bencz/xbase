/* The file functions of the language over the platform layer (platform/platform.h): a file is the system's handle as
 * a number. Each function stores the outcome's code for FError() (0 when it succeeded); one given an argument of the
 * wrong type returns its failure value and leaves FError() alone. MemoRead(), MemoWrit(), FReadStr(), File(), ADir()
 * and Directory() are written in xBase over these (rtl/files.prg). */
#include "xbrt_int.h"

#include "core/file.h"

#include <stdlib.h>
#include <string.h>

/* A string argument as a path, or NULL: not a string, or one holding a NUL (no file has that name). */
static const char *path_arg(uint32_t argc, const xb_item *argv, uint32_t k) {
  if (argc <= k || argv[k].type != XB_T_STRING) return NULL;
  const xb_str *s = argv[k].v.s;
  return memchr(s->data, '\0', s->len) ? NULL : s->data;
}

static int handle_arg(uint32_t argc, const xb_item *argv, uint32_t k, xb_file *f) {
  if (argc <= k || !xb_is_number(&argv[k])) return 0;
  *f = (xb_file)xb_num_to_i64(&argv[k]);
  return 1;
}

static int64_t num_or(uint32_t argc, const xb_item *argv, uint32_t k, int64_t dflt) {
  return argc > k && xb_is_number(&argv[k]) ? xb_num_to_i64(&argv[k]) : dflt;
}

/* The outcome of a file function: 0 after a success. */
static void set_error(xb_ctx *ctx, int ok, int32_t err) { ctx->file_error = ok ? 0 : err; }

/* FError(): the code of the last file function's outcome (platform.h lists them), 0 after a success. */
void xbf_FERROR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_set_int(ret, ctx->file_error);
}

/* FOpen( cFile, [nMode] ): the handle of an existing file opened with nMode (FO_READ by default, fileio.ch), -1 when
 * it cannot be (an access of 3: 5). A name that is not a string is BASE/2021. */
void xbf_FOPEN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || argv[0].type != XB_T_STRING) {
    xb_fn_arg_error(ctx, ret, 2021, "FOPEN", argc, argv);
    return;
  }
  const char *path = path_arg(argc, argv, 0);
  int32_t mode = (int32_t)num_or(argc, argv, 1, FO_READ);
  xb_file f = 0;
  int32_t err = (mode & 3) == 3 ? 5 : 2; /* access 3 is none of FO_READ, FO_WRITE and FO_READWRITE */
  int ok = path && (mode & 3) != 3 && xb_platform_file_open(path, mode, &f, &err);
  set_error(ctx, ok, err);
  xb_set_int(ret, ok ? (int64_t)f : -1);
}

/* FCreate( cFile, [nAttribute] ): the handle of a new (or emptied) file, open for reading and writing, with the
 * attribute (FC_NORMAL by default); -1 when it cannot be made. */
void xbf_FCREATE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_set_int(ret, -1);
  if (argc < 1 || argv[0].type != XB_T_STRING) return;
  const char *path = path_arg(argc, argv, 0);
  xb_file f = 0;
  int32_t err = 2;
  int ok = path && xb_platform_file_create(path, (int32_t)num_or(argc, argv, 1, FC_NORMAL), &f, &err);
  set_error(ctx, ok, err);
  if (ok) xb_set_int(ret, (int64_t)f);
}

/* FClose( nHandle ): whether the file closed without an error. */
void xbf_FCLOSE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_file f;
  xb_set_logical(ret, 0);
  if (!handle_arg(argc, argv, 0, &f)) return;
  int32_t err = 0;
  int ok = xb_platform_file_close(f, &err);
  set_error(ctx, ok, err);
  xb_set_logical(ret, ok);
}

/* FRead( nHandle, @cBuffer, nBytes ): reads nBytes into the first bytes of the buffer, which keeps its length: the
 * count read. Nothing is read without a buffer passed by reference, or for nBytes below 1 or past the buffer's
 * length. */
void xbf_FREAD(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_file f;
  xb_set_int(ret, 0);
  if (!handle_arg(argc, argv, 0, &f) || argc < 3 || (argv[1].type != XB_T_CELL && argv[1].type != XB_T_REF) ||
      !xb_is_number(&argv[2]))
    return;
  const xb_item *target = xb_ref_target(ctx, &argv[1]);
  if (ctx->pending && xb_pending(ctx)) return;
  if (target->type != XB_T_STRING) return;
  int64_t n = xb_num_to_i64(&argv[2]);
  size_t len = target->v.s->len;
  if (n < 1 || (uint64_t)n > len) {
    ctx->file_error = 0;
    return;
  }
  xb_str *buf = xb_str_alloc(len);
  target = xb_ref_target(ctx, &argv[1]); /* the variable as it is after the allocation */
  if (target->type != XB_T_STRING || target->v.s->len != len) return;
  memcpy(buf->data, target->v.s->data, len);
  int32_t err = 0;
  int64_t got = xb_file_read_up_to(f, buf->data, (size_t)n, &err);
  set_error(ctx, got >= 0, err);
  if (got <= 0) return;
  xb_item *var = xb_ref_target(ctx, &argv[1]);
  xb_set_str(var, buf);
  xb_ref_written(&argv[1]);
  xb_set_int(ret, got);
}

/* FWrite( nHandle, cBuffer, [nBytes] ): writes the first nBytes of the buffer (all of it without nBytes, or with one
 * below 0 or past its length): the count written, 0 after an error. */
void xbf_FWRITE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_file f;
  xb_set_int(ret, 0);
  if (!handle_arg(argc, argv, 0, &f) || argc < 2 || argv[1].type != XB_T_STRING) return;
  const xb_str *s = argv[1].v.s;
  int64_t n = num_or(argc, argv, 2, (int64_t)s->len);
  if (n < 0 || (uint64_t)n > s->len) n = (int64_t)s->len;
  int32_t err = 0;
  int ok = n == 0 || xb_platform_file_write(f, s->data, (size_t)n, &err);
  set_error(ctx, ok, err);
  if (ok) xb_set_int(ret, n);
}

/* FSeek( nHandle, nOffset, [nOrigin] ): moves to nOffset from the start (FS_SET, the default), the current position
 * (FS_RELATIVE) or the end (FS_END; any other origin is FS_SET): the new position. A move that fails (before the
 * start: 25, from any origin) leaves the position as it was and returns it. */
void xbf_FSEEK(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_file f;
  xb_set_int(ret, 0);
  if (!handle_arg(argc, argv, 0, &f) || argc < 2 || !xb_is_number(&argv[1])) return;
  int32_t err = 0, ignored = 0;
  int64_t origin = num_or(argc, argv, 2, FS_SET);
  if (origin != FS_RELATIVE && origin != FS_END) origin = FS_SET;
  int64_t pos = xb_platform_file_seek(f, xb_num_to_i64(&argv[1]), (int)origin, &err);
  set_error(ctx, pos >= 0, err);
  if (pos < 0) pos = xb_platform_file_seek(f, 0, FS_RELATIVE, &ignored);
  xb_set_int(ret, pos < 0 ? 0 : pos);
}

/* FErase( cFile ): 0 when the file was deleted, else -1. */
void xbf_FERASE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_set_int(ret, -1);
  if (argc < 1 || argv[0].type != XB_T_STRING) return;
  const char *path = path_arg(argc, argv, 0);
  int32_t err = 2;
  int ok = path && xb_platform_file_erase(path, &err);
  set_error(ctx, ok, err);
  if (ok) xb_set_int(ret, 0);
}

/* FRename( cOld, cNew ): 0 when the file was renamed, else -1; nothing is replaced. */
void xbf_FRENAME(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_set_int(ret, -1);
  if (argc < 2 || argv[0].type != XB_T_STRING || argv[1].type != XB_T_STRING) return;
  const char *from = path_arg(argc, argv, 0), *to = path_arg(argc, argv, 1);
  int32_t err = 2;
  int ok = from && to && xb_platform_file_rename(from, to, &err);
  set_error(ctx, ok, err);
  if (ok) xb_set_int(ret, 0);
}

/* CurDir(): the current directory without its drive and the separators before its first name ("" for the root), as
 * DOS gives it. */
void xbf_CURDIR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  int32_t err = 0;
  char *cwd = xb_platform_cwd(&err);
  const char *p = cwd ? cwd : "";
  if (p[0] && p[1] == ':') p += 2; /* a drive */
  while (*p && xb_platform_path_separator(*p)) ++p;
  xb_set_str(ret, xb_str_new(p, strlen(p)));
  free(cwd);
}

/* DirChange( cDirectory ): 0 when it became the current directory, else the error's code (-1 for a name that is not a
 * string). */
void xbf_DIRCHANGE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  xb_set_int(ret, -1);
  if (argc < 1 || argv[0].type != XB_T_STRING) return;
  const char *path = path_arg(argc, argv, 0);
  int32_t err = 2;
  xb_set_int(ret, path && xb_platform_chdir(path, &err) ? 0 : err);
}

/* DiskSpace( [nDrive] ): the bytes free on the drive (0 or none: the current one), 0 when it cannot be told. */
void xbf_DISKSPACE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  int64_t drive = num_or(argc, argv, 0, 0), bytes = 0;
  int32_t err = 0;
  int ok = xb_platform_disk_free(drive < INT32_MIN || drive > INT32_MAX ? -1 : (int)drive, &bytes, &err);
  set_error(ctx, ok, err);
  xb_set_int(ret, ok ? bytes : 0);
}

/* GetEnv( cName ): the value of the environment variable, "" when it is not set. */
void xbf_GETENV(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  const char *name = path_arg(argc, argv, 0);
  char *value = name && name[0] ? xb_platform_getenv(name) : NULL;
  xb_set_str(ret, value ? xb_str_new(value, strlen(value)) : xb_str_empty());
  free(value);
}

static int same_char(char a, char b, int fold) { return fold ? xb_ascii_upper(a) == xb_ascii_upper(b) : a == b; }

/* Whether `name` matches `mask` (`*` any run of characters, `?` one character), letter case aside with `fold`. */
static int wildcards_match(const char *name, const char *mask, size_t maskLen, int fold) {
  const char *n = name, *afterStar = NULL;
  size_t m = 0, starAt = 0;
  int star = 0;
  while (*n) {
    if (m < maskLen && mask[m] == '*') {
      star = 1;
      starAt = ++m;
      afterStar = n;
    } else if (m < maskLen && (mask[m] == '?' || same_char(*n, mask[m], fold))) {
      ++m;
      ++n;
    } else if (star) {
      m = starAt;
      n = ++afterStar;
    } else {
      return 0;
    }
  }
  while (m < maskLen && mask[m] == '*') ++m;
  return m == maskLen;
}

/* Whether `name` matches the DOS mask `mask`: a mask ending in ".*" also takes names without a dot, so "*.*" takes
 * every name. */
static int mask_matches(const char *name, const char *mask, size_t maskLen, int fold) {
  if (wildcards_match(name, mask, maskLen, fold)) return 1;
  return maskLen >= 2 && mask[maskLen - 2] == '.' && mask[maskLen - 1] == '*' && !strchr(name, '.') &&
         wildcards_match(name, mask, maskLen - 2, fold);
}

/* Appends { cName, nSize, dDate, cTime, cAttributes } for `e` to the array in `out` (a root): the attributes as
 * letters in the order RHSDA (read-only, hidden, system, directory, archive). */
static void add_entry(xb_item *out, const xb_dir_entry *e) {
  xb_array *list = out->v.p;
  size_t at = list->len;
  xb_array_resize(list, (uint64_t)at + 1);
  xb_array *entry = xb_array_alloc(5);
  xb_set_array(&list->items[at], entry);
  xb_gc_write_item(list, at);
  xb_set_str(&entry->items[0], xb_str_new(e->name, strlen(e->name)));
  xb_gc_write_item(entry, 0);
  xb_set_int(&entry->items[1], e->size);
  xb_set_date(&entry->items[2], xb_date_encode(e->year, e->month, e->day));
  char time[9] = {(char)('0' + e->hour / 10),   (char)('0' + e->hour % 10),   ':',
                  (char)('0' + e->minute / 10), (char)('0' + e->minute % 10), ':',
                  (char)('0' + e->second / 10), (char)('0' + e->second % 10), '\0'};
  xb_set_str(&entry->items[3], xb_str_new(time, 8));
  xb_gc_write_item(entry, 3);
  static const struct {
    uint32_t bit;
    char letter;
  } letters[] = {{XB_FA_READONLY, 'R'}, {XB_FA_HIDDEN, 'H'}, {XB_FA_SYSTEM, 'S'}, {XB_FA_DIRECTORY, 'D'},
                 {XB_FA_ARCHIVE, 'A'}};
  char attrs[sizeof letters / sizeof letters[0]];
  size_t n = 0;
  for (size_t k = 0; k < sizeof letters / sizeof letters[0]; ++k)
    if (e->attrs & letters[k].bit) attrs[n++] = letters[k].letter;
  xb_set_str(&entry->items[4], xb_str_new(attrs, n));
  xb_gc_write_item(entry, 4);
}

/* __DirScan( [cSpec] ): { cName, nSize, dDate, cTime, cAttributes } for each entry, of any attributes, whose name
 * matches the mask of cSpec (its last part; none, as for "" or "dir/": every entry) in its directory; a mask without
 * wildcards names one entry. For Directory() (rtl/files.prg). */
void xbf___DIRSCAN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  xb_set_array(ret, xb_array_alloc(0));
  const char *spec = argc > 0 && argv[0].type == XB_T_STRING ? path_arg(argc, argv, 0) : "";
  if (!spec) return;
  size_t len = strlen(spec), maskAt = 0;
  for (size_t i = 0; i < len; ++i)
    if (xb_platform_path_separator(spec[i])) maskAt = i + 1;
  const char *mask = maskAt < len ? spec + maskAt : "*"; /* none, or a directory's name with its separator: all */
  size_t maskLen = maskAt < len ? len - maskAt : 1;
  int32_t err = 0;
  xb_dir_entry e;
  if (!strpbrk(mask, "*?")) {
    if (xb_platform_file_info(spec, mask, &e, &err)) add_entry(ret, &e);
    return;
  }
  char *dir = malloc(maskAt + 1);
  if (!dir) xb_gc_fatal("out of memory");
  memcpy(dir, spec, maskAt);
  dir[maskAt] = '\0';
  xb_dir d;
  int opened = xb_platform_dir_open(dir, &d, &err);
  int fold = !xb_platform_names_case_sensitive(dir);
  free(dir);
  if (!opened) return;
  while (xb_platform_dir_next(d, &e))
    if (mask_matches(e.name, mask, maskLen, fold)) add_entry(ret, &e);
  xb_platform_dir_close(d);
}
