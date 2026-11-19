// bench/bfs_dfs.prg in Java: BFS from 6 sources and an iterative DFS with an explicit stack over every component, on
// an undirected graph of 50,000 vertices and 150,000 edges from an LCG kept as arrays of adjacency; 2 times.
public class BfsDfs {
  // The sum of the levels of the vertices reached from src, plus their number.
  static long bfs(int src, int nV, int[] first, int[] adj) {
    long[] level = new long[nV + 1];
    int[] queue = new int[nV + 1];
    java.util.Arrays.fill(level, -1);
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
  static long dfs(int nV, int[] first, int[] adj) {
    boolean[] seen = new boolean[nV + 1];
    int[] stack = new int[2 * (adj.length - 1) + nV + 1];
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
    return comps * 1000000000L + sum;
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), seed = 3, total = 0;
    int nV = 50000, nE = 150000;
    int[] eu = new int[nE + 1], ev = new int[nE + 1], first = new int[nV + 2];
    for (int i = 1; i <= nE; i++) {
      seed = seed * 16807 % 2147483647;
      eu[i] = (int) (seed / 16 % nV) + 1;
      seed = seed * 16807 % 2147483647;
      ev[i] = (int) (seed / 16 % nV) + 1;
      first[eu[i]]++;
      first[ev[i]]++;
    }
    // degrees to the first slot of each vertex; first[nV + 1] ends the last one
    int fill = 1;
    for (int i = 1; i <= nV + 1; i++) {
      int d = first[i];
      first[i] = fill;
      fill += d;
    }
    int[] next = first.clone(), adj = new int[2 * nE + 1];
    for (int i = 1; i <= nE; i++) {
      adj[next[eu[i]]++] = ev[i];
      adj[next[ev[i]]++] = eu[i];
    }
    for (int rep = 1; rep <= 2; rep++) {
      for (int src = 1; src <= 6; src++) total += bfs((src - 1) * 8000 + 1, nV, first, adj);
      total += dfs(nV, first, adj);
    }
    Bench.report("bfs_dfs", t, total == 62828511998050L);
  }
}
