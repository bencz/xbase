/* File helpers over the platform layer (platform/platform.h) that the runtime's file users share. */
#ifndef XB_FILE_H
#define XB_FILE_H

#include "platform/platform.h"

#include <stddef.h>
#include <stdint.h>

/* Reads up to `n` bytes into `buf`, unless the file ends first: the count read, -1 after an error (in `*err`). */
int64_t xb_file_read_up_to(xb_file f, void *buf, size_t n, int32_t *err);

/* `len` bytes of `name` with `ext` (".dbf") added when its last component has no extension: malloc'ed, NUL ended. */
char *xb_file_path_with_ext(const char *name, size_t len, const char *ext);

#endif
