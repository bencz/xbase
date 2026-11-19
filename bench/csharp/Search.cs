// bench/tak, ackermann, n_queens, sudoku_solver, knapsack, dijkstra, bfs_dfs, binary_search, binary_trees,
// fannkuch_redux, meteor_contest, prime_sieve_atkin and towers_of_hanoi .prg in C#: the same work and checks. Arrays
// keep the .prg's 1-based indices where they are indexed by them (element 0 unused).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.CompilerServices;

static class TakBench {
  [MethodImpl(MethodImplOptions.NoInlining)]
  static long Tak(long x, long y, long z) => y < x ? Tak(Tak(x - 1, y, z), Tak(y - 1, z, x), Tak(z - 1, x, y)) : z;

  public static void Run() {
    var sw = Stopwatch.StartNew();
    bool ok = true;
    for (int rep = 1; rep <= 10; rep++) ok = ok && Tak(24, 16, 8) == 9;
    Program.Report("tak", sw, ok);
  }
}

static class AckermannBench {
  [MethodImpl(MethodImplOptions.NoInlining)]
  static long Ack(long m, long n) {
    if (m == 0) return n + 1;
    if (n == 0) return Ack(m - 1, 1);
    return Ack(m - 1, Ack(m, n - 1));
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    bool ok = true;
    for (int rep = 1; rep <= 3; rep++) ok = ok && Ack(3, 9) == 4093;
    Program.Report("ackermann", sw, ok);
  }
}

static class NQueensBench {
  // The solutions with queens on rows 1 to row - 1 as col and the diagonals d1 and d2 say.
  static long Place(int row, int n, bool[] col, bool[] d1, bool[] d2) {
    if (row > n) return 1;
    long count = 0;
    for (int c = 1; c <= n; c++)
      if (!col[c] && !d1[row + c - 1] && !d2[row - c + n]) {
        col[c] = true;
        d1[row + c - 1] = true;
        d2[row - c + n] = true;
        count += Place(row + 1, n, col, d1, d2);
        col[c] = false;
        d1[row + c - 1] = false;
        d2[row - c + n] = false;
      }
    return count;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 12;
    bool ok = true;
    for (int rep = 1; rep <= 3; rep++)
      ok = ok && Place(1, n, new bool[n + 1], new bool[2 * n], new bool[2 * n]) == 14200;
    Program.Report("n_queens", sw, ok);
  }
}

static class SudokuSolverBench {
  // Marks digit v as used, or free, in the row, column and box of cell p (0 to 80).
  static void Mark(int p, int v, bool on, bool[] row, bool[] col, bool[] box) {
    int r = p / 9, c = p - p / 9 * 9;
    row[r * 9 + v] = on;
    col[c * 9 + v] = on;
    box[(r / 3 * 3 + c / 3) * 9 + v] = on;
  }

  // Fills the empty cells from cell p (0 to 80) on; true when the grid is solved.
  static bool Solve(int p, int[] grid, bool[] row, bool[] col, bool[] box) {
    while (p < 81 && grid[p + 1] != 0) p++;
    if (p == 81) return true;
    int r = p / 9, c = p - r * 9, b = r / 3 * 3 + c / 3;
    for (int v = 1; v <= 9; v++) {
      if (row[r * 9 + v] || col[c * 9 + v] || box[b * 9 + v]) continue;
      grid[p + 1] = v;
      Mark(p, v, true, row, col, box);
      if (Solve(p + 1, grid, row, col, box)) return true;
      Mark(p, v, false, row, col, box);
      grid[p + 1] = 0;
    }
    return false;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    string[] puzzles = {
      "8..........36......7..9.2...5...7.......457.....1...3...1....68..85...1..9....4..",
      "6.....8.3.4.7.................5.4.7.3..2.....1.6.......2.....5.....8.6......1....",
      "48.3............71.2.......7.5....6....2..8.............1.76...3.....4......5....",
      "..53.....8......2..7..1.5..4....53...1..7...6..32...8..6.5....9..4....3......97..",
      "1....7.9..3..2...8..96..5....53..9...1..8...26....4...3......1..4......7..7...3.."
    };
    bool ok = true;
    for (int rep = 1; rep <= 3; rep++) {
      long sum = 0;
      foreach (string puzzle in puzzles) {
        var grid = new int[82];
        var row = new bool[82];
        var col = new bool[82];
        var box = new bool[82];
        for (int i = 1; i <= 81; i++) {
          grid[i] = puzzle[i - 1] == '.' ? 0 : puzzle[i - 1] - '0';
          if (grid[i] != 0) Mark(i - 1, grid[i], true, row, col, box);
        }
        ok = ok && Solve(0, grid, row, col, box);
        for (int i = 1; i <= 81; i++) sum = (sum * 31 + grid[i]) & 0xFFFFFFF;
      }
      ok = ok && sum == 252679381;
    }
    Program.Report("sudoku_solver", sw, ok);
  }
}

static class KnapsackBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 200, cap = 10000;
    var best = new long[cap + 2];
    var w = new int[n + 1];
    var v = new long[n + 1];
    long total = 0;
    for (int rep = 1; rep <= 10; rep++) {
      long seed = rep;
      for (int i = 1; i <= n; i++) {
        seed = seed * 16807 % 2147483647;
        w[i] = (int)(seed / 65536 % 1000) + 1;
        seed = seed * 16807 % 2147483647;
        v[i] = seed / 65536 % 1000 + 1;
      }
      Array.Fill(best, 0);
      for (int i = 1; i <= n; i++) {
        int wi = w[i];
        long vi = v[i];
        for (int c = cap + 1; c >= wi + 1; c--) {
          long x = best[c - wi] + vi;
          if (x > best[c]) best[c] = x;
        }
      }
      total += best[cap + 1];
    }
    Program.Report("knapsack", sw, total == 366516);
  }
}

static class DijkstraBench {
  // The sum of the shortest distances from vertex src to every vertex.
  static long Dijkstra(int src, int nV, int[] first, int[] to, long[] w) {
    var dist = new long[nV + 1];
    Array.Fill(dist, -1);
    var done = new bool[nV + 1];
    var hD = new long[to.Length + 1];
    var hV = new int[to.Length + 1];
    long sum = 0;
    dist[src] = 0;
    int nH = 1;
    hD[1] = 0;
    hV[1] = src;
    while (nH > 0) {
      long d = hD[1];
      int u = hV[1];
      // pop: the last entry sifted down from the root
      long x = hD[nH];
      int y = hV[nH];
      nH--;
      int i = 1, c;
      while ((c = i * 2) <= nH) {
        if (c < nH && hD[c + 1] < hD[c]) c++;
        if (hD[c] >= x) break;
        hD[i] = hD[c];
        hV[i] = hV[c];
        i = c;
      }
      hD[i] = x;
      hV[i] = y;
      if (done[u]) continue;
      done[u] = true;
      sum += d;
      for (int e = first[u]; e <= first[u + 1] - 1; e++) {
        int v = to[e];
        long nd = d + w[e];
        if (!done[v] && (dist[v] < 0 || nd < dist[v])) {
          dist[v] = nd;
          // push: sifted up from a new leaf
          nH++;
          i = nH;
          while (i > 1 && hD[c = i / 2] > nd) {
            hD[i] = hD[c];
            hV[i] = hV[c];
            i = c;
          }
          hD[i] = nd;
          hV[i] = v;
        }
      }
    }
    return sum;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int nV = 20000, nOut = 6, nE = nV * nOut, e = 0;
    var first = new int[nV + 2];
    var to = new int[nE + 1];
    var w = new long[nE + 1];
    long seed = 7, total = 0;
    for (int i = 1; i <= nV; i++) {
      first[i] = e + 1;
      e++;
      to[e] = i == nV ? 1 : i + 1;
      seed = seed * 16807 % 2147483647;
      w[e] = seed / 256 % 100 + 1;
      for (int k = 2; k <= nOut; k++) {
        e++;
        seed = seed * 16807 % 2147483647;
        to[e] = (int)(seed / 16 % nV) + 1;
        seed = seed * 16807 % 2147483647;
        w[e] = seed / 256 % 100 + 1;
      }
    }
    first[nV + 1] = e + 1;
    for (int src = 1; src <= 12; src++) total += Dijkstra((src - 1) * 1600 + 1, nV, first, to, w);
    Program.Report("dijkstra", sw, total == 41762116);
  }
}

static class BfsDfsBench {
  // The sum of the levels of the vertices reached from src, plus their number.
  static long Bfs(int src, int nV, int[] first, int[] adj) {
    var level = new long[nV + 1];
    Array.Fill(level, -1);
    var queue = new int[nV + 1];
    int head = 1, tail = 1;
    long sum = 0;
    level[src] = 0;
    queue[1] = src;
    while (head <= tail) {
      int u = queue[head++];
      long d = level[u];
      sum += d + 1;
      for (int e = first[u]; e <= first[u + 1] - 1; e++) {
        int v = adj[e];
        if (level[v] < 0) {
          level[v] = d + 1;
          queue[++tail] = v;
        }
      }
    }
    return sum;
  }

  // The number of components times 1,000,000,000 plus the sum of each vertex times its place in the visiting order.
  static long Dfs(int nV, int[] first, int[] adj) {
    var seen = new bool[nV + 1];
    var stack = new int[2 * (adj.Length - 1) + nV + 1];
    long order = 0, comps = 0, sum = 0;
    for (int s = 1; s <= nV; s++) {
      if (seen[s]) continue;
      comps++;
      int top = 1;
      stack[1] = s;
      while (top > 0) {
        int u = stack[top--];
        if (seen[u]) continue;
        seen[u] = true;
        order++;
        sum += u * order;
        for (int e = first[u + 1] - 1; e >= first[u]; e--)
          if (!seen[adj[e]]) stack[++top] = adj[e];
      }
    }
    return comps * 1000000000 + sum;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int nV = 50000, nE = 150000;
    var eu = new int[nE + 1];
    var ev = new int[nE + 1];
    var first = new int[nV + 2];
    long seed = 3, total = 0;
    for (int i = 1; i <= nE; i++) {
      seed = seed * 16807 % 2147483647;
      eu[i] = (int)(seed / 16 % nV) + 1;
      seed = seed * 16807 % 2147483647;
      ev[i] = (int)(seed / 16 % nV) + 1;
      first[eu[i]]++;
      first[ev[i]]++;
    }
    // degrees to the first slot of each vertex; first[ nV + 1 ] ends the last one
    int fillAt = 1;
    for (int i = 1; i <= nV + 1; i++) {
      int d = first[i];
      first[i] = fillAt;
      fillAt += d;
    }
    var fill = (int[])first.Clone();
    var adj = new int[2 * nE + 1];
    for (int i = 1; i <= nE; i++) {
      adj[fill[eu[i]]++] = ev[i];
      adj[fill[ev[i]]++] = eu[i];
    }
    for (int rep = 1; rep <= 2; rep++) {
      for (int src = 1; src <= 6; src++) total += Bfs((src - 1) * 8000 + 1, nV, first, adj);
      total += Dfs(nV, first, adj);
    }
    Program.Report("bfs_dfs", sw, total == 62828511998050);
  }
}

static class BinarySearchBench {
  static readonly MinStd random = new MinStd(4711);
  static long Rand(long n) => random.Next(n);

  public static void Run() {
    var a = new long[1000001];
    long found = 0, posSum = 0;
    for (int i = 1; i < a.Length; i++) a[i] = i * 3L;
    var sw = Stopwatch.StartNew();
    for (int i = 1; i <= 1000000; i++) {
      long k = Rand(3000010);
      int lo = 1, hi = a.Length - 1;
      while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (a[mid] < k)
          lo = mid + 1;
        else if (a[mid] > k)
          hi = mid - 1;
        else {
          found++;
          posSum += mid;
          break;
        }
      }
    }
    Program.Report("binary_search", sw, found == 333181 && posSum == 166512579113);
  }
}

// The nodes are objects of two references, as the .prg's two-element arrays; the collector frees them.
static class BinaryTreesBench {
  sealed class Node {
    public Node Left, Right;
  }

  static Node Make(int d) => d == 0 ? new Node() : new Node { Left = Make(d - 1), Right = Make(d - 1) };

  // The nodes of the tree.
  static long Check(Node n) => n.Left == null ? 1 : 1 + Check(n.Left) + Check(n.Right);

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int maxDepth = 14;
    long total = Check(Make(maxDepth + 1));
    Node longLived = Make(maxDepth);
    for (int d = 4; d <= maxDepth; d += 2) {
      long iters = 1L << (maxDepth - d + 4);
      for (long i = 1; i <= iters; i++) total += Check(Make(d));
    }
    total += Check(longLived);
    Program.Report("binary_trees", sw, total == 3222190);
  }
}

static class FannkuchReduxBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 9;
    bool ok = true;
    for (int rep = 1; rep <= 4; rep++) {
      var perm = new int[n + 1];
      var perm1 = new int[n + 2];
      var count = new int[n + 1];
      for (int i = 1; i <= n; i++) perm1[i] = i - 1;
      long checksum = 0, permCount = 0;
      int maxFlips = 0, r = n;
      bool done = false;
      while (!done) {
        while (r != 1) {
          count[r] = r;
          r--;
        }
        for (int i = 1; i <= n; i++) perm[i] = perm1[i];
        int flips = 0, k = perm[1];
        while (k != 0) {
          int i = 1, j = k + 1;
          while (i < j) {
            (perm[i], perm[j]) = (perm[j], perm[i]);
            i++;
            j--;
          }
          flips++;
          k = perm[1];
        }
        maxFlips = Math.Max(maxFlips, flips);
        checksum += (permCount & 1) == 0 ? flips : -flips;
        while (true) {
          if (r == n) {
            done = true;
            break;
          }
          int perm0 = perm1[1];
          for (int i = 1; i <= r; i++) perm1[i] = perm1[i + 1];
          perm1[r + 1] = perm0;
          count[r + 1]--;
          if (count[r + 1] > 0) break;
          r++;
        }
        permCount++;
      }
      ok = ok && checksum == 8629 && maxFlips == 30;
    }
    Program.Report("fannkuch_redux", sw, ok);
  }
}

static class MeteorContestBench {
  const int Cols = 5, Rows = 10, Cells = 50, Halved = 3, Window = 10;

  // By piece (0 to 9): its placements' masks and cells, and the placements whose lowest cell is c.
  static List<long>[] masks;
  static List<int[]>[] cells;
  static List<int>[][] by;
  static long[] neighbors;
  static int[] placed;
  static long solutions;
  static string best, worst;

  // Offset coordinates (odd rows shifted right) of a cell (from 0) to axial ones { q, r }, and back; -1 off the board.
  static int[] ToAxial(int cell) {
    int row = cell / Cols;
    return new[] { cell - row * Cols - (row - (row & 1)) / 2, row };
  }

  static int ToCell(int q, int r) {
    int col = q + (r - (r & 1)) / 2;
    if (r < 0 || r >= Rows || col < 0 || col >= Cols) return -1;
    return r * Cols + col;
  }

  static void MakePlacements(string shape) {
    int[] dq = { 1, -1, 0, 0, 1, -1 }, dr = { 0, 0, 1, -1, -1, 1 };
    neighbors = new long[Cells];
    for (int c = 0; c < Cells; c++) {
      int[] at = ToAxial(c);
      for (int i = 0; i < 6; i++) {
        int x = ToCell(at[0] + dq[i], at[1] + dr[i]);
        if (x >= 0) neighbors[c] |= 1L << x;
      }
    }
    masks = new List<long>[10];
    cells = new List<int[]>[10];
    by = new List<int>[10][];
    for (int p = 0; p < 10; p++) {
      masks[p] = new List<long>();
      cells[p] = new List<int[]>();
      by[p] = new List<int>[Cells];
      for (int c = 0; c < Cells; c++) by[p][c] = new List<int>();
      var pts = new List<int[]>();
      for (int c = 0; c < Cells; c++)
        if (shape[c] - 48 == p) pts.Add(ToAxial(c));
      var seen = new HashSet<long>();
      for (int o = 0; o <= 11; o++) {
        for (int nAt = 0; nAt < Cells; nAt++) {
          if (p == Halved && o % 6 >= 3) break;
          int[] at = ToAxial(nAt);
          var place = new int[5];
          bool ok = true;
          for (int i = 0; i < 5; i++) {
            int x = ToCell(pts[i][0] - pts[0][0] + at[0], pts[i][1] - pts[0][1] + at[1]);
            place[i] = x;
            if (x < 0) {
              ok = false;
              break;
            }
          }
          if (ok) {
            long mask = 0;
            int low = Cells;
            for (int i = 0; i < 5; i++) {
              mask |= 1L << place[i];
              low = Math.Min(low, place[i]);
            }
            if (seen.Add(mask)) {
              masks[p].Add(mask);
              cells[p].Add(place);
              by[p][low].Add(masks[p].Count - 1);
            }
          }
        }
        // The next orientation: a turn of 60 degrees, and after the sixth a mirror image.
        for (int i = 0; i < 5; i++) {
          int q = pts[i][0];
          if (o == 5) {
            pts[i][0] = pts[i][1];
            pts[i][1] = q;
          } else {
            pts[i][0] = -pts[i][1];
            pts[i][1] = q + pts[i][1];
          }
        }
      }
    }
  }

  // Whether an empty cell among the Window from cell has no empty neighbor.
  static bool WalledIn(long board, int cell) {
    for (int c = cell; c < Math.Min(cell + Window, Cells); c++)
      if (((board >> c) & 1) == 0 && (board & neighbors[c]) == neighbors[c]) return true;
    return false;
  }

  static void Record(string sol) {
    if (solutions == 0 || string.CompareOrdinal(sol, best) < 0) best = sol;
    if (solutions == 0 || string.CompareOrdinal(sol, worst) > 0) worst = sol;
    solutions++;
  }

  static void Solve(long board, int cell, int depth) {
    while (((board >> cell) & 1) == 1) cell++;
    if (depth == 10) {
      var sol = new char[Cells];
      for (int p = 0; p < 10; p++)
        foreach (int i in cells[p][placed[p]]) sol[i] = (char)(48 + p);
      var turned = new char[Cells];
      for (int i = 0; i < Cells; i++) turned[i] = sol[Cells - 1 - i];
      Record(new string(sol));
      Record(new string(turned));
      return;
    }
    for (int p = 0; p < 10; p++)
      if (placed[p] < 0) {
        foreach (int k in by[p][cell]) {
          long next = board | masks[p][k];
          if ((board & masks[p][k]) == 0 && !WalledIn(next, cell)) {
            placed[p] = k;
            Solve(next, cell, depth + 1);
          }
        }
        placed[p] = -1;
      }
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    MakePlacements("99998966856688568255777257472014220144031400311333");
    placed = new int[10];
    Array.Fill(placed, -1);
    Solve(0, 0, 0);
    bool ok = solutions == 2098 && best == "00001222012661126155865558633348893448934747977799" &&
              worst == "99998966856688568255777257472014220144031400311333";
    Program.Report("meteor_contest", sw, ok);
  }
}

static class PrimeSieveAtkinBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    int limit = 2000000;
    var a = new bool[limit + 1];
    bool ok = true;
    for (int rep = 1; rep <= 12; rep++) {
      Array.Fill(a, false);
      for (long x = 1; x * x <= limit; x++)
        for (long y = 1; y * y <= limit; y++) {
          long n = 4 * x * x + y * y;
          if (n <= limit) {
            long r = n % 12;
            if (r == 1 || r == 5) a[n] = !a[n];
          }
          n = 3 * x * x + y * y;
          if (n <= limit && n % 12 == 7) a[n] = !a[n];
          if (x > y) {
            n = 3 * x * x - y * y;
            if (n <= limit && n % 12 == 11) a[n] = !a[n];
          }
        }
      for (long i = 5; i * i <= limit; i++)
        if (a[i]) {
          long k = i * i;
          for (long j = k; j <= limit; j += k) a[j] = false;
        }
      long count = 2;
      for (int i = 5; i <= limit; i++)
        if (a[i]) count++;
      ok = ok && count == 148933;
    }
    Program.Report("prime_sieve_atkin", sw, ok);
  }
}

static class TowersOfHanoiBench {
  // Moves n disks from peg from to peg to through via; returns the number of moves.
  static long Hanoi(int n, int from, int to, int via, int[][] peg, int[] top, long[] bad) {
    long moves = 0;
    if (n > 0) {
      moves = Hanoi(n - 1, from, via, to, peg, top, bad);
      int d = peg[from][top[from]];
      top[from]--;
      if (top[to] > 0 && peg[to][top[to]] < d) bad[0]++;
      top[to]++;
      peg[to][top[to]] = d;
      moves += 1 + Hanoi(n - 1, via, to, from, peg, top, bad);
    }
    return moves;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int disks = 20;
    bool ok = true;
    for (int rep = 1; rep <= 4; rep++) {
      var peg = new int[4][];
      for (int p = 1; p <= 3; p++) peg[p] = new int[disks + 1];
      for (int i = 1; i <= disks; i++) peg[1][i] = disks - i + 1;
      int[] top = { 0, disks, 0, 0 };
      var bad = new long[1];
      long moves = Hanoi(disks, 1, 3, 2, peg, top, bad);
      ok = ok && moves == (1L << disks) - 1 && bad[0] == 0 && top[3] == disks;
      for (int i = 1; i <= disks; i++) ok = ok && peg[3][i] == disks - i + 1;
    }
    Program.Report("towers_of_hanoi", sw, ok);
  }
}
