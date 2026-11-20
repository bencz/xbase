/* The system's randomness and a file's synchronization: xb_platform_entropy gives the bytes asked (the system's source
 * is there where the tests run), two draws of xb_platform_random differ, and a file written and synchronized keeps
 * its bytes. */
#include "check.h"
#include "platform/platform.h"

#include <string.h>

int main(void) {
  unsigned char a[64], b[64];
  CHECK(xb_platform_entropy(a, sizeof a) == sizeof a);
  xb_platform_random(a, sizeof a);
  xb_platform_random(b, sizeof b);
  CHECK(memcmp(a, b, sizeof a) != 0);

  const char *path = "p_random_sync.tmp";
  xb_file f;
  int32_t err = 0;
  CHECK(xb_platform_file_create(path, 0, &f, &err));
  CHECK(xb_platform_file_write_at(f, 0, a, sizeof a, &err));
  CHECK(xb_platform_file_sync(f, &err));
  unsigned char back[64];
  CHECK(xb_platform_file_read_at(f, 0, back, sizeof back, &err) == (int64_t)sizeof back);
  CHECK(memcmp(a, back, sizeof a) == 0);
  CHECK(xb_platform_file_close(f, &err));
  CHECK(xb_platform_file_erase(path, &err));
  return 0;
}
