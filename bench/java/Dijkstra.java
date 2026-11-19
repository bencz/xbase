// bench/dijkstra.prg in Java: Dijkstra's shortest paths with a binary heap of (distance, vertex) pairs (written out,
// stale entries skipped) on a graph of 20,000 vertices and 120,000 edges from an LCG, from 12 sources.
public class Dijkstra {
  // The sum of the shortest distances from vertex src to every vertex.
  static long dijkstra(int src, int nV, int nE, int[] first, int[] aTo, long[] aW) {
    long[] dist = new long[nV + 1], hD = new long[nE + 2];
    boolean[] done = new boolean[nV + 1];
    int[] hV = new int[nE + 2];
    java.util.Arrays.fill(dist, -1);
    int nH = 1;
    long sum = 0;
    dist[src] = 0;
    hD[1] = 0;
    hV[1] = src;
    while (nH > 0) {
      long d = hD[1], x = hD[nH];
      int u = hV[1], y = hV[nH], i = 1, c;
      --nH;
      while ((c = i * 2) <= nH) {
        if (c < nH && hD[c + 1] < hD[c]) ++c;
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
      for (int e = first[u]; e <= first[u + 1] - 1; ++e) {
        int v = aTo[e];
        long nd = d + aW[e];
        if (!done[v] && (dist[v] < 0 || nd < dist[v])) {
          dist[v] = nd;
          i = ++nH;
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

  public static void main(String[] args) {
    long t = System.nanoTime(), seed = 7, total = 0;
    int nV = 20000, nOut = 6, nE = nV * nOut, e = 0;
    int[] first = new int[nV + 2], aTo = new int[nE + 1];
    long[] aW = new long[nE + 1];
    for (int i = 1; i <= nV; ++i) {
      first[i] = e + 1;
      ++e;
      aTo[e] = i == nV ? 1 : i + 1;
      seed = seed * 16807 % 2147483647;
      aW[e] = seed / 256 % 100 + 1;
      for (int k = 2; k <= nOut; ++k) {
        ++e;
        seed = seed * 16807 % 2147483647;
        aTo[e] = (int) (seed / 16 % nV) + 1;
        seed = seed * 16807 % 2147483647;
        aW[e] = seed / 256 % 100 + 1;
      }
    }
    first[nV + 1] = e + 1;
    for (int src = 1; src <= 12; ++src) total += dijkstra((src - 1) * 1600 + 1, nV, nE, first, aTo, aW);
    Bench.report("dijkstra", t, total == 41762116);
  }
}
