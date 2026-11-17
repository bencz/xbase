/* Handles opened outside the registry of shared files (FOpen, a .mem file, a text file) and closed while the file has
 * users of the registry: on POSIX systems closing any handle of a file drops all the process's locks on it, so such a
 * handle is kept. Another process still finds a user's record lock after the close, and none once the user is gone;
 * an opening for the same access takes the kept handle again (from the file's start), one for another access does
 * not; a second close of a handle, or a close of a user's handle, is error 6, and a kept handle is not open for the
 * program's file functions. On Windows, where locks belong to a handle, a handle simply closes. Everywhere, an opening
 * that would keep the users from the file (exclusive, denying reads, denying writes while a user writes) and a
 * creation, which would empty it, are refused with 32. */
#include "check.h"
#include "core/shared_file.h"
#include "xbrt_int.h"

#include <stdio.h>

#define FILE_NAME "shared_plain.tmp"

#ifdef _WIN32
int main(void) {
  xb_platform_init();
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_shared_init();
  xb_file f, g;
  int32_t err;
  CHECK(xb_shared_plain_create(FILE_NAME, 0, &f, &err) && xb_shared_plain_close(f, &err));
  xb_shared *s;
  CHECK(xb_shared_open(FILE_NAME, 0, 0, 0, &s, &err) && xb_shared_lock(s, 100, 1, XB_LOCK_WRITE, 0, &err));
  CHECK(xb_shared_plain_open(FILE_NAME, FO_READ, &g, &err) && xb_shared_plain_close(g, &err));
  CHECK(!xb_shared_plain_create(FILE_NAME, 0, &g, &err) && err == 32);
  CHECK(!xb_shared_plain_open(FILE_NAME, FO_READ | FO_EXCLUSIVE, &g, &err) && err == 32);
  CHECK(!xb_shared_plain_open(FILE_NAME, FO_READ | FO_DENYWRITE, &g, &err) && err == 32);
  CHECK(xb_shared_close(s, &err));
  xb_platform_file_erase(FILE_NAME, &err);
  puts("shared_plain ok");
  return 0;
}
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

/* Whether another process finds byte 100 of the file write-locked (a child that asks the system, with nothing of the
 * runtime's: the parent has threads). */
static int locked_elsewhere(void) {
  pid_t pid = fork();
  CHECK(pid >= 0);
  if (pid == 0) {
    int fd = open(FILE_NAME, O_RDWR);
    struct flock l = {0};
    l.l_type = F_WRLCK;
    l.l_whence = SEEK_SET;
    l.l_start = 100;
    l.l_len = 1;
    _exit(fd < 0 || fcntl(fd, F_GETLK, &l) != 0 ? 2 : l.l_type != F_UNLCK);
  }
  int status;
  CHECK(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) < 2);
  return WEXITSTATUS(status) == 1;
}

int main(void) {
  xb_platform_init();
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_shared_init();
  xb_file f, g, h;
  int32_t err = 0;
  char byte = 0;
  CHECK(xb_shared_plain_create(FILE_NAME, 0, &f, &err) && xb_platform_file_write(f, "x", 1, &err) &&
        xb_shared_plain_close(f, &err));

  xb_shared *s;
  CHECK(xb_shared_open(FILE_NAME, 0, 0, 0, &s, &err) && xb_shared_lock(s, 100, 1, XB_LOCK_WRITE, 0, &err));
  CHECK(locked_elsewhere());
  CHECK(xb_shared_plain_open(FILE_NAME, FO_READ, &f, &err));
  CHECK(xb_platform_file_read(f, &byte, 1, &err) == 1 && byte == 'x');
  CHECK(xb_shared_plain_close(f, &err));
  CHECK(locked_elsewhere()); /* the user's lock is still there */

  CHECK(!xb_shared_plain_close(f, &err) && err == 6); /* closed already */
  CHECK(xb_shared_plain_live(f) == (xb_file)-1 && xb_platform_file_read(xb_shared_plain_live(f), &byte, 1, &err) < 0);
  CHECK(!xb_shared_plain_close(xb_shared_handle(s), &err) && err == 6); /* the user's own: never the program's */
  CHECK(locked_elsewhere());
  CHECK(!xb_shared_plain_create(FILE_NAME, 0, &g, &err) && err == 32); /* it would empty the user's file */
  CHECK(!xb_shared_plain_open(FILE_NAME, FO_READ | FO_EXCLUSIVE, &g, &err) && err == 32);
  CHECK(!xb_shared_plain_open(FILE_NAME, FO_READ | FO_DENYREAD, &g, &err) && err == 32);
  CHECK(!xb_shared_plain_open(FILE_NAME, FO_READ | FO_DENYWRITE, &g, &err) && err == 32); /* the user writes */
  CHECK(xb_shared_plain_open(FILE_NAME, FO_READWRITE, &g, &err) && g != f); /* another access: a new handle */
  CHECK(xb_shared_plain_open(FILE_NAME, FO_READ, &h, &err) && h == f);      /* the kept one, from the start */
  CHECK(xb_shared_plain_live(h) == h);
  CHECK(xb_platform_file_read(h, &byte, 1, &err) == 1 && byte == 'x');
  for (int k = 0; k < 5000; ++k) /* again and again: the same handle each time, never one more */
    CHECK(xb_shared_plain_close(h, &err) && xb_shared_plain_open(FILE_NAME, FO_READ, &h, &err) && h == f);
  CHECK(xb_shared_plain_close(h, &err) && xb_shared_plain_close(g, &err));
  CHECK(locked_elsewhere());

  CHECK(xb_shared_plain_create(FILE_NAME ".mem", 0, &f, &err) && xb_shared_plain_close(f, &err)); /* another file */
  xb_platform_file_erase(FILE_NAME ".mem", &err);
  CHECK(xb_shared_close(s, &err)); /* the last user: the kept handles close with it */
  CHECK(!locked_elsewhere());
  CHECK(xb_shared_plain_open(FILE_NAME, FO_READ, &f, &err) && xb_shared_plain_close(f, &err)); /* no user now */
  CHECK(!xb_shared_plain_close(f, &err) && err == 6);
  xb_platform_file_erase(FILE_NAME, &err);
  puts("shared_plain ok");
  return 0;
}
#endif
