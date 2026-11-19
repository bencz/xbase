/* bench/date_time_ops.prg in C: 200,000 dates between 1900 and 2099 (days since 1970, converted to and from the
 * civil calendar) and times of day from a fixed seed. For each: year, month and day against the YYYYMMDD text; the
 * weekday and its name (Sundays, Fridays the 13th); the dd/mm/yyyy text and back; 13 months added (the day kept
 * within the month, through a YYYYMMDD text read back and day arithmetic); the days to the year's end; a time of day
 * as HH:MM:SS text and back to seconds. */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const day_names[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                        "Thursday", "Friday", "Saturday"};

static int64_t seed = 1999;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

/* Days since 1970-01-01 of a date of the proleptic Gregorian calendar. */
static int64_t days_from_civil(int64_t y, int64_t m, int64_t d) {
  y -= m <= 2;
  int64_t era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400;
  int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

static void civil_from_days(int64_t z, int64_t *y, int64_t *m, int64_t *d) {
  z += 719468;
  int64_t era = (z >= 0 ? z : z - 146096) / 146097, doe = z - era * 146097;
  int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
  *d = doy - (153 * mp + 2) / 5 + 1;
  *m = mp < 10 ? mp + 3 : mp - 9;
  *y = yoe + era * 400 + (*m <= 2);
}

static int64_t days_in_month(int64_t y, int64_t m) {
  static const int64_t dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 29 : dim[m - 1];
}

static int64_t day_of(int64_t z) {
  int64_t y, m, d;
  civil_from_days(z, &y, &m, &d);
  return d;
}

/* SToD(): the date of a YYYYMMDD text; -1 for an empty or impossible one. */
static int64_t stod(const char *s) {
  int64_t y = 0, m = 0, d = 0;
  for (int i = 0; i < 4; ++i) y = y * 10 + (s[i] - '0');
  for (int i = 4; i < 6; ++i) m = m * 10 + (s[i] - '0');
  for (int i = 6; i < 8; ++i) d = d * 10 + (s[i] - '0');
  if (m < 1 || m > 12 || d < 1 || d > days_in_month(y, m)) return -1;
  return days_from_civil(y, m, d);
}

static void dtos(int64_t z, char *out) {
  int64_t y, m, d;
  civil_from_days(z, &y, &m, &d);
  snprintf(out, 9, "%04d%02d%02d", (int)y, (int)m, (int)d);
}

/* CToD() under SET DATE BRITISH and SET CENTURY ON: dd/mm/yyyy; -1 for an impossible date. */
static int64_t ctod(const char *s) {
  int d, m, y;
  if (sscanf(s, "%2d/%2d/%4d", &d, &m, &y) != 3 || m < 1 || m > 12 || d < 1 || d > days_in_month(y, m)) return -1;
  return days_from_civil(y, m, d);
}

static int64_t add_months(int64_t y, int64_t m, int64_t day, int64_t n) {
  char s[16];
  int64_t nm = m + n, ny = y + (nm - 1) / 12;
  nm = (nm - 1) % 12 + 1;
  snprintf(s, sizeof s, "%04d%02d01", (int)ny, (int)nm);
  int64_t first = stod(s), next = first + 31, last = day_of(next - day_of(next));
  return first + (day < last ? day : last) - 1;
}

int main(void) {
  int64_t base = days_from_civil(1900, 1, 1), ymd = 0, sun = 0, fri13 = 0, add = 0, to_end = 0, sec = 0, bad = 0;
  char s[32];
  long long t = bench_ms();
  for (int64_t i = 1; i <= 200000; ++i) {
    int64_t z = base + rnd(73000), y, m, d;
    civil_from_days(z, &y, &m, &d);
    int64_t n = y * 10000 + m * 100 + d;
    ymd += n;
    dtos(z, s);
    int64_t back = strtoll(s, NULL, 10);
    snprintf(s, sizeof s, "%02d/%02d/%04d", (int)d, (int)m, (int)y);
    if (back != n || ctod(s) != z) ++bad;
    int64_t dow = ((z + 4) % 7 + 7) % 7 + 1; /* 1970-01-01 was a Thursday; 1 is Sunday */
    if (dow == 1) ++sun;
    if (d == 13 && strcmp(day_names[dow - 1], "Friday") == 0) ++fri13;
    dtos(add_months(y, m, d, 13), s);
    add += strtoll(s, NULL, 10);
    snprintf(s, sizeof s, "%4d1231", (int)y);
    to_end += stod(s) - z;
    int64_t secs = rnd(86400);
    snprintf(s, sizeof s, "%02d:%02d:%02d", (int)(secs / 3600), (int)(secs / 60 % 60), (int)(secs % 60));
    int hh, mm, ss;
    if (sscanf(s, "%d:%d:%d", &hh, &mm, &ss) != 3 || hh * 3600 + mm * 60 + ss != secs) ++bad;
    sec += secs;
  }
  bench_report("date_time_ops", t,
               bad == 0 && ymd == 3998853334023LL && sun == 28567 && fri13 == 951 && add == 4001021793716LL &&
                   to_end == 36531947 && sec == 8632784536LL);
  return 0;
}
