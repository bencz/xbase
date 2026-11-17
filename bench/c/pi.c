/* bench/pi.prg in C, translated statement by statement: the tape machine's digits of pi. */
#include "bench.h"
#include <string.h>

static int x[128001];
static char out[4096];

static const char *pi_digits(int digits) {
  int xc = 1, nout = 0;
  memset(x, 0, sizeof x);
  xc++;
  x[xc] += digits;
  while (x[xc] != 0) {
    xc--;
    x[xc]++;
    xc += 8;
    x[xc] += 10;
    xc -= 7;
    x[xc]--;
  }
  xc++;
  x[xc] += 5;
  while (x[xc] != 0) {
    xc--;
    x[xc] += 9;
    xc++;
    x[xc]--;
  }
  x[xc]++;
  xc += 6;
  x[xc]++;
  while (x[xc] != 0) {
    xc -= 2;
    x[xc] += 3;
    while (x[xc] != 0) {
      xc += 2;
      while (x[xc] != 0) {
        x[xc]--;
        xc--;
      }
      xc--;
      while (x[xc] != 0) {
        xc++;
      }
      xc--;
      x[xc]--;
    }
    xc += 2;
    while (x[xc] != 0) {
      xc++;
      x[xc]++;
      xc++;
    }
    xc--;
    while (x[xc] != 0) {
      xc--;
    }
    xc++;
  }
  xc++;
  while (x[xc] != 0) {
    while (x[xc] != 0) {
      x[xc]--;
      xc += 4;
      x[xc]++;
      xc -= 4;
    }
    xc += 3;
    x[xc] += 3;
    xc++;
    x[xc]--;
  }
  xc--;
  while (x[xc] != 0) {
    xc -= 4;
  }
  xc -= 8;
  x[xc]++;
  while (x[xc] != 0) {
    x[xc]--;
    xc += 12;
    while (x[xc] != 0) {
      xc--;
      x[xc]++;
      while (x[xc] != 0) {
        x[xc]--;
        xc += 4;
        x[xc]++;
        xc -= 4;
      }
      xc++;
      xc += 4;
    }
    xc -= 4;
    while (x[xc] != 0) {
      xc += 5;
      while (x[xc] != 0) {
        xc -= 4;
        x[xc]++;
        xc += 4;
        x[xc]--;
      }
      xc -= 5;
      x[xc]--;
      while (x[xc] != 0) {
        xc -= 2;
        x[xc] += 10;
        xc += 2;
        x[xc]--;
      }
      xc += 3;
      while (x[xc] != 0) {
        xc -= 2;
        while (x[xc] != 0) {
          xc--;
          x[xc]++;
          xc -= 2;
          x[xc]++;
          xc += 3;
          x[xc]--;
        }
        xc--;
        while (x[xc] != 0) {
          xc++;
          x[xc]++;
          xc--;
          x[xc]--;
        }
        xc--;
        x[xc] += 2;
        xc -= 2;
        x[xc]++;
        xc += 6;
        x[xc]--;
      }
      xc -= 2;
      while (x[xc] != 0) {
        x[xc]--;
      }
      xc -= 2;
      x[xc]--;
      xc--;
      while (x[xc] != 0) {
        x[xc]--;
        xc += 2;
        x[xc]++;
        xc--;
        x[xc]--;
        while (x[xc] != 0) {
          xc += 3;
        }
        xc++;
        while (x[xc] != 0) {
          while (x[xc] != 0) {
            xc--;
            x[xc]++;
            xc++;
            x[xc]--;
          }
          xc++;
          x[xc]++;
          xc += 2;
        }
        xc -= 5;
      }
      xc++;
      while (x[xc] != 0) {
        x[xc]--;
      }
      xc++;
      x[xc]++;
      xc -= 3;
      x[xc]--;
      while (x[xc] != 0) {
        xc += 2;
        x[xc]++;
        xc -= 2;
        x[xc]--;
      }
      xc--;
    }
    xc -= 4;
    x[xc]++;
    xc += 7;
    xc++;
    while (x[xc] != 0) {
      x[xc]--;
    }
    xc++;
    while (x[xc] != 0) {
      xc -= 3;
      x[xc]++;
      xc += 3;
      x[xc]--;
    }
    xc -= 2;
    x[xc] += 10;
    xc--;
    while (x[xc] != 0) {
      x[xc]--;
      xc += 2;
      x[xc]++;
      xc--;
      x[xc]--;
      while (x[xc] != 0) {
        xc += 3;
      }
      xc++;
      while (x[xc] != 0) {
        while (x[xc] != 0) {
          xc--;
          x[xc]++;
          xc++;
          x[xc]--;
        }
        xc++;
        x[xc]++;
        xc += 2;
      }
      xc -= 5;
    }
    xc++;
    while (x[xc] != 0) {
      x[xc]--;
    }
    xc++;
    x[xc]++;
    xc++;
    while (x[xc] != 0) {
      xc -= 2;
      x[xc]++;
      xc--;
      x[xc]++;
      xc += 3;
      x[xc]--;
    }
    xc -= 3;
    xc--;
    x[xc]++;
    xc--;
    x[xc]++;
    xc += 2;
    while (x[xc] != 0) {
      x[xc]--;
      while (x[xc] != 0) {
        x[xc]--;
        while (x[xc] != 0) {
          x[xc]--;
          while (x[xc] != 0) {
            x[xc]--;
            while (x[xc] != 0) {
              x[xc]--;
              while (x[xc] != 0) {
                x[xc]--;
                while (x[xc] != 0) {
                  x[xc]--;
                  while (x[xc] != 0) {
                    x[xc]--;
                    while (x[xc] != 0) {
                      x[xc]--;
                      xc--;
                      x[xc]--;
                      xc++;
                      while (x[xc] != 0) {
                        x[xc]--;
                        xc--;
                        x[xc]++;
                        xc--;
                        x[xc]--;
                        xc += 2;
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    xc--;
    while (x[xc] != 0) {
      x[xc] += 5;
      while (x[xc] != 0) {
        xc -= 3;
        x[xc] += 8;
        xc--;
        x[xc] += 8;
        xc += 4;
        x[xc]--;
      }
      xc--;
      xc -= 3;
      x[xc]++;
      xc--;
      x[xc]--;
      xc += 4;
      while (x[xc] != 0) {
        xc++;
        x[xc]++;
        xc -= 3;
        x[xc] += 9;
        xc--;
        x[xc]--;
        xc += 3;
        x[xc]--;
      }
      xc -= 5;
      while (x[xc] != 0) {
        xc += 2;
        x[xc]++;
        xc -= 2;
        x[xc]--;
      }
      x[xc]++;
      xc--;
      while (x[xc] != 0) {
        x[xc]--;
        xc++;
        x[xc]--;
        xc--;
      }
      xc++;
      while (x[xc] != 0) {
        xc += 2;
        out[nout++] = (char)x[xc];
        xc -= 4;
        while (x[xc] != 0) {
          x[xc]++;
          out[nout++] = (char)x[xc];
          while (x[xc] != 0) {
            x[xc]--;
          }
        }
        xc += 2;
        x[xc]--;
      }
      xc++;
      while (x[xc] != 0) {
        xc += 2;
        out[nout++] = (char)x[xc];
        xc -= 2;
        x[xc]--;
      }
      xc++;
      while (x[xc] != 0) {
        x[xc]--;
      }
      xc++;
      while (x[xc] != 0) {
        x[xc]--;
      }
      xc += 3;
      while (x[xc] != 0) {
        xc += 2;
        while (x[xc] != 0) {
          xc -= 8;
          x[xc]++;
          xc += 8;
          x[xc]--;
        }
        xc -= 2;
        x[xc]--;
      }
    }
    xc += 2;
    while (x[xc] != 0) {
      x[xc]--;
    }
    xc -= 3;
    while (x[xc] != 0) {
      x[xc]--;
    }
    xc -= 8;
  }
  x[xc] += 10;
  out[nout++] = (char)x[xc];
  out[nout] = '\0';
  return out;
}

int main(void) {
  long long t = bench_ms();
  const char *r = pi_digits(30);
  bench_report("pi", t, strcmp(r, "3.1415926535897932384626433832\n") == 0);
  return 0;
}
