/* File helpers over the platform layer (platform/platform.h) that the runtime's file users share. */
#ifndef XB_FILE_H
#define XB_FILE_H

#include "platform/platform.h"

#include <stddef.h>
#include <stdint.h>

/* Reads up to `n` bytes into `buf`, unless the file ends first: the count read, -1 after an error (in `*err`). */
int64_t xb_file_read_up_to(xb_file f, void *buf, size_t n, int32_t *err);

/* Reads `n` bytes at `pos`: whether all of them were there (a file that ends early is XB_FERR_FAILURE). */
int xb_file_read_at(xb_file f, int64_t pos, void *buf, size_t n, int32_t *err);

/* Where the last component of `len` bytes of `name` starts (`*start`) and where its extension, the part from its last
 * '.', starts (`*ext`; `len` when it has none). */
void xb_file_name_parts(const char *name, size_t len, size_t *start, size_t *ext);

/* `len` bytes of `name` with `ext` (".dbf") added when its last component has no extension: malloc'ed, NUL ended. */
char *xb_file_path_with_ext(const char *name, size_t len, const char *ext);

/* `path` with its last component's extension (if any) replaced by `ext` (".dbt"): malloc'ed, NUL ended. */
char *xb_file_path_replace_ext(const char *path, const char *ext);

#endif
