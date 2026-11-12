/* The operating system's side of the platform functions that may block (platform/blocking.c runs each one through
 * xb_blocking_run): implemented in each platform directory, used by nothing else. */
#ifndef XB_PLATFORM_OS_H
#define XB_PLATFORM_OS_H

#include "platform/platform.h"

int xb_os_file_open(const char *path, int32_t mode, xb_file *f, int32_t *err);
int xb_os_file_open_always(const char *path, int32_t mode, xb_file *f, int32_t *err);
int xb_os_file_create(const char *path, int32_t attr, xb_file *f, int32_t *err);
int64_t xb_os_file_read(xb_file f, void *buf, size_t n, int32_t *err);
int xb_os_file_write(xb_file f, const void *buf, size_t n, int32_t *err);
int64_t xb_os_file_read_at(xb_file f, int64_t pos, void *buf, size_t n, int32_t *err);
int xb_os_file_write_at(xb_file f, int64_t pos, const void *buf, size_t n, int32_t *err);
int64_t xb_os_file_seek(xb_file f, int64_t offset, int origin, int32_t *err);
int xb_os_file_truncate(xb_file f, int64_t size, int32_t *err);
int xb_os_file_close(xb_file f, int32_t *err);
int xb_os_file_share(xb_file f, int kind, int32_t *err);
int xb_os_file_lock(xb_file f, uint64_t pos, uint64_t len, int kind, int32_t *err);
int xb_os_file_id(xb_file f, xb_file_id *id, int32_t *err);
int xb_os_path_id(const char *path, xb_file_id *id, int32_t *err);
int xb_os_file_erase(const char *path, int32_t *err);
int xb_os_file_rename(const char *from, const char *to, int32_t *err);
int xb_os_file_info(const char *path, const char *name, xb_dir_entry *e, int32_t *err);
int xb_os_dir_open(const char *path, xb_dir *d, int32_t *err);
int xb_os_dir_next(xb_dir d, xb_dir_entry *e);
void xb_os_dir_close(xb_dir d);
int xb_os_names_case_sensitive(const char *dir);
char *xb_os_cwd(int32_t *err);
int xb_os_chdir(const char *path, int32_t *err);
int xb_os_disk_free(int drive, int64_t *bytes, int32_t *err);

#endif
