// bench/async_pi.prg in Java: the Basel sum over 32 tasks of 50,000,000 terms each (CompletableFuture on the common
// pool), joined in order; pi is Sqrt( 6 * sum ).
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CompletableFuture;

public class AsyncPi {
  static final long TERMS = 50000000;

  public static void main(String[] args) {
    long t = System.nanoTime();
    List<CompletableFuture<Double>> parts = new ArrayList<>();
    for (int k = 0; k < 32; k++) {
      final long i = k;
      parts.add(CompletableFuture.supplyAsync(() -> {
        double s = 0;
        for (long n = i * TERMS + 1; n <= (i + 1) * TERMS; n++) {
          double inv = 1.0 / n;
          s += inv * inv;
        }
        return s;
      }));
    }
    double total = 0;
    for (CompletableFuture<Double> p : parts) total += p.join();
    double pi = Math.sqrt(total * 6);
    Bench.report("async_pi", t, Math.abs(Math.PI - pi) < 1e-9);
  }
}
