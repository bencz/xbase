/* Shared by the C benchmarks: the monotonic clock in milliseconds, and the result line the xBase benchmarks print. */
#ifndef BENCH_H
#define BENCH_H
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static long long bench_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void bench_report(const char *name, long long start, int ok) {
  printf("%s %lld %s\n", name, bench_ms() - start, ok ? "ok" : "WRONG");
}
#endif
