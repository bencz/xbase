// bench/array_append.prg in Java: 5 lists of 2,000,000 numbers built with add() from empty, then summed.
import java.util.ArrayList;

public class ArrayAppend {
  public static void main(String[] args) {
    long t = System.nanoTime(), sum = 0;
    ArrayList<Long> a = new ArrayList<>();
    for (long r = 1; r <= 5; r++) {
      a = new ArrayList<>();
      for (long i = 1; i <= 2000000; i++) a.add(i + r);
      for (long x : a) sum += x;
    }
    Bench.report("array_append", t, sum == 10000035000000L && a.size() == 2000000);
  }
}
