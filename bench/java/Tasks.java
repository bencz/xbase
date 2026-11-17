// bench/tasks.prg in Java: 200,000 short tasks on the common pool joined, a 50,000-link chain of continuations, and
// 20,000 tasks that each start and wait for another (CompletableFuture).
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CompletableFuture;

public class Tasks {
  public static void main(String[] args) {
    long t = System.nanoTime(), s = 0;
    List<CompletableFuture<Long>> a = new ArrayList<>();
    for (int k = 0; k < 200000; k++) {
      long i = k + 1;
      a.add(CompletableFuture.supplyAsync(() -> i % 7));
    }
    CompletableFuture.allOf(a.toArray(new CompletableFuture<?>[0])).join();
    for (CompletableFuture<Long> f : a) s += f.join();
    CompletableFuture<Long> c = CompletableFuture.completedFuture(0L);
    for (int k = 1; k <= 50000; k++) c = c.thenApplyAsync(p -> p + 1);
    s += c.join();
    for (int k = 1; k <= 20000; k++)
      s += CompletableFuture.supplyAsync(() -> CompletableFuture.supplyAsync(() -> 1L).join()).join();
    Bench.report("tasks", t, s == 599997 + 50000 + 20000);
  }
}
