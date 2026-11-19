/* bench/meteor_contest.prg in C: every way to fill the meteor contest's board (5 x 10 hexagonal cells, odd rows
 * shifted half a cell right) with its 10 pieces of 5 cells. The pieces are read from a solution; their placements
 * (12 orientations, every position, duplicates dropped) are 64-bit masks listed by piece and lowest cell. A search
 * fills the lowest empty cell first and drops a placement that leaves an empty cell walled in among the next 10.
 * The board turned half a turn is the same board, with cell i at 49 - i: piece 3, which no half turn leaves the same,
 * takes only 6 of its orientations, one of each such pair, and every solution found counts for itself and its turned
 * image (the reversed string). Checked: 2098 solutions, and the smallest and largest as strings. */
#include "bench.h"
#include <string.h>

#define COLS 5
#define ROWS 10
#define CELLS 50
#define MAXP 400
#define HALVED 3
#define WINDOW 10

static const char shape[] = "99998966856688568255777257472014220144031400311333";
static int64_t place_mask[10][MAXP], neighbors[CELLS];
static int place_cells[10][MAXP][5], place_count[10];
static int by_cell[10][CELLS][MAXP], by_cell_count[10][CELLS], placed[10];
static int64_t solutions;
static char best[CELLS + 1], worst[CELLS + 1];

/* Offset coordinates (odd rows shifted right) to axial ones and back. */
static void to_axial(int cell, int *q, int *r) {
  int row = cell / COLS;
  *q = cell % COLS - (row - (row & 1)) / 2;
  *r = row;
}

static int to_cell(int q, int r) {
  int col = q + (r - (r & 1)) / 2;
  return r < 0 || r >= ROWS || col < 0 || col >= COLS ? -1 : r * COLS + col;
}

static void add_placement(int p, const int *cells) {
  int64_t mask = 0;
  int low = CELLS;
  for (int i = 0; i < 5; ++i) {
    mask |= (int64_t)1 << cells[i];
    if (cells[i] < low) low = cells[i];
  }
  for (int i = 0; i < place_count[p]; ++i)
    if (place_mask[p][i] == mask) return;
  int k = place_count[p]++;
  place_mask[p][k] = mask;
  memcpy(place_cells[p][k], cells, sizeof place_cells[p][k]);
  by_cell[p][low][by_cell_count[p][low]++] = k;
}

static void make_placements(void) {
  static const int dq[6] = {1, -1, 0, 0, 1, -1}, dr[6] = {0, 0, 1, -1, -1, 1};
  for (int c = 0; c < CELLS; ++c) {
    int q, r;
    to_axial(c, &q, &r);
    for (int i = 0; i < 6; ++i) {
      int x = to_cell(q + dq[i], r + dr[i]);
      if (x >= 0) neighbors[c] |= (int64_t)1 << x;
    }
  }
  for (int p = 0; p < 10; ++p) {
    int q[5], r[5], n = 0;
    for (int c = 0; c < CELLS; ++c)
      if (shape[c] - '0' == p) {
        to_axial(c, &q[n], &r[n]);
        ++n;
      }
    for (int o = 0; o < 12; ++o) {
      for (int at = 0; at < CELLS && (p != HALVED || o % 6 < 3); ++at) {
        int q0, r0, cells[5], ok = 1;
        to_axial(at, &q0, &r0);
        for (int i = 0; i < 5 && ok; ++i) {
          cells[i] = to_cell(q[i] - q[0] + q0, r[i] - r[0] + r0);
          ok = cells[i] >= 0;
        }
        if (ok) add_placement(p, cells);
      }
      /* The next orientation: a turn of 60 degrees, and after the sixth a mirror image. */
      for (int i = 0; i < 5; ++i) {
        int nq = o == 5 ? r[i] : -r[i], nr = o == 5 ? q[i] : q[i] + r[i];
        q[i] = nq;
        r[i] = nr;
      }
    }
  }
}

/* Whether an empty cell among the WINDOW from cell has no empty neighbor. */
static int walled_in(int64_t board, int cell) {
  int end = cell + WINDOW < CELLS ? cell + WINDOW : CELLS;
  for (int c = cell; c < end; ++c)
    if (!(board >> c & 1) && (board & neighbors[c]) == neighbors[c]) return 1;
  return 0;
}

static void record(const char *s) {
  if (solutions == 0 || strcmp(s, best) < 0) strcpy(best, s);
  if (solutions == 0 || strcmp(s, worst) > 0) strcpy(worst, s);
  ++solutions;
}

static void solve(int64_t board, int cell, int depth) {
  while (board >> cell & 1) ++cell;
  if (depth == 10) {
    char s[CELLS + 1], turned[CELLS + 1];
    for (int p = 0; p < 10; ++p)
      for (int i = 0; i < 5; ++i) s[place_cells[p][placed[p]][i]] = (char)('0' + p);
    for (int i = 0; i < CELLS; ++i) turned[i] = s[CELLS - 1 - i];
    s[CELLS] = turned[CELLS] = 0;
    record(s);
    record(turned);
    return;
  }
  for (int p = 0; p < 10; ++p) {
    if (placed[p] >= 0) continue;
    for (int i = 0; i < by_cell_count[p][cell]; ++i) {
      int k = by_cell[p][cell][i];
      int64_t next = board | place_mask[p][k];
      if ((board & place_mask[p][k]) || walled_in(next, cell)) continue;
      placed[p] = k;
      solve(next, cell, depth + 1);
    }
    placed[p] = -1;
  }
}

int main(void) {
  long long t = bench_ms();
  make_placements();
  memset(placed, -1, sizeof placed);
  solve(0, 0, 0);
  int ok = solutions == 2098 && strcmp(best, "00001222012661126155865558633348893448934747977799") == 0 &&
           strcmp(worst, "99998966856688568255777257472014220144031400311333") == 0;
  bench_report("meteor_contest", t, ok);
  return 0;
}
