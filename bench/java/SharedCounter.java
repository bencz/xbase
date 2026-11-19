// bench/shared_counter.prg in Java: a counter shared by 8 threads, contended: each thread adds 1 to an AtomicLong
// 250,000 times with addAndGet, then 50,000 times with a compareAndSet loop, then 50,000 times to an array element
// under a ReentrantLock. No increment may be lost, whatever the scheduling.
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.locks.ReentrantLock;

public class SharedCounter {
  static void work(AtomicLong atomic, AtomicLong cas, ReentrantLock m, long[] box) {
    for (int i = 1; i <= 250000; i++) atomic.addAndGet(1);
    for (int i = 1; i <= 50000; i++) {
      while (true) {
        long n = cas.get();
        if (cas.compareAndSet(n, n + 1)) break;
      }
    }
    for (int i = 1; i <= 50000; i++) {
      m.lock();
      try {
        box[0]++;
      } finally {
        m.unlock();
      }
    }
  }

  public static void main(String[] args) throws InterruptedException {
    long t = System.nanoTime();
    AtomicLong atomic = new AtomicLong(), cas = new AtomicLong();
    ReentrantLock m = new ReentrantLock();
    long[] box = {0};
    ArrayList<Thread> threads = new ArrayList<>();
    for (int k = 1; k <= 8; k++) {
      Thread th = new Thread(() -> work(atomic, cas, m, box));
      th.start();
      threads.add(th);
    }
    for (Thread th : threads) th.join();
    Bench.report("shared_counter", t, atomic.get() == 8 * 250000 && cas.get() == 8 * 50000 && box[0] == 8 * 50000);
  }
}
