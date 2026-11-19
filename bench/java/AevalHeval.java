// bench/aeval_heval.prg in Java: the library's walks with lambdas, forEach over a list of 1,000,000 numbers (with
// their positions) and over a HashMap of 200,000 entries, the lambda adding to a captured counter; 20 rounds.
import java.util.ArrayList;
import java.util.HashMap;

public class AevalHeval {
  static long n;

  public static void main(String[] args) {
    ArrayList<Long> a = new ArrayList<>();
    HashMap<Long, Long> h = new HashMap<>();
    for (long i = 1; i <= 1000000; i++) a.add(i % 777);
    for (long i = 1; i <= 200000; i++) h.put(i * 3, i % 555);
    long t = System.nanoTime();
    for (long r = 1; r <= 20; r++) {
      long round = r;
      long[] pos = {0};
      a.forEach(x -> n += x + (++pos[0] % 1000 == 0 ? round : 0));
      h.forEach((k, v) -> n += v + (k % 3000 == 0 ? 1 : 0));
    }
    Bench.report("aeval_heval", t, n == 8867500260L);
  }
}
