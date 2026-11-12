/* The checks of the platform tests: a failed one is reported with its line and makes the test exit with 1. */
#ifndef XB_TEST_CHECK_H
#define XB_TEST_CHECK_H

#include <stdio.h>
#include <stdlib.h>

#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
      exit(1);                                                            \
    }                                                                     \
  } while (0)

#endif
