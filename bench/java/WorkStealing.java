// bench/work_stealing.prg in Java: fork-join over an unbalanced tree (as the unbalanced tree search benchmark): the
// root and the next four levels have 8 children each, every node below has 5 children or none as its hash says
// (2,859,034 nodes in all), each node hashed 40 times. The four levels below the root are ForkJoinPool tasks (4,680);
// a task joins its children in turn (one not stolen yet runs inline), the levels below run inside the task, and idle
// workers steal the queued tasks.
import java.util.concurrent.ForkJoinPool;
import java.util.concurrent.RecursiveTask;

public class WorkStealing {
  // { nodes, sum } of the subtree of seed s at depth d: its children as tasks down to depth 4, then walked in place.
  static class Branch extends RecursiveTask<long[]> {
    private static final long serialVersionUID = 1L;
    final long s;
    final int d;

    Branch(long s, int d) {
      this.s = s;
      this.d = d;
    }

    @Override
    protected long[] compute() {
      long[] acc = {0, 0};
      if (d >= 4) {
        visit(s, d, acc);
        return acc;
      }
      long h = hash(s);
      Branch[] tasks = new Branch[8];
      for (int k = 1; k <= 8; k++) {
        tasks[k - 1] = new Branch((s * 16807 + k * 48271) % 2147483647, d + 1);
        tasks[k - 1].fork();
      }
      for (int k = 8; k >= 1; k--) {
        long[] a = tasks[k - 1].join();
        acc[0] += a[0];
        acc[1] += a[1];
      }
      return new long[] {acc[0] + 1, acc[1] + h % 1000};
    }
  }

  // acc is { nodes, sum }, the .prg's two parameters by reference.
  static void visit(long s, int d, long[] acc) {
    long h = hash(s);
    acc[0]++;
    acc[1] += h % 1000;
    if (d < 5 || h % 1000 < 198) {
      int n = d < 5 ? 8 : 5;
      for (int k = 1; k <= n; k++) visit((s * 16807 + k * 48271) % 2147483647, d + 1, acc);
    }
  }

  static long hash(long s) {
    for (int i = 1; i <= 40; i++) s = s * 16807 % 2147483647;
    return s;
  }

  public static void main(String[] args) {
    long t = System.nanoTime();
    long[] a = ForkJoinPool.commonPool().invoke(new Branch(19937, 0));
    Bench.report("work_stealing", t, a[0] == 2859034 && a[1] == 1428166988);
  }
}
