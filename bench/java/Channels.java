// bench/channels.prg in Java: 4 producer threads send 250,000 numbers each through a bounded queue (capacity 64) to 2
// consumer threads, then 1,000,000 offer/poll pairs on an unbounded queue from one thread.
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicLong;

public class Channels {
  static final long END = -1;

  public static void main(String[] args) throws InterruptedException {
    long t = System.nanoTime();
    ArrayBlockingQueue<Long> ch = new ArrayBlockingQueue<>(64);
    AtomicLong sum = new AtomicLong();
    Thread[] consumers = new Thread[2], producers = new Thread[4];
    for (int k = 0; k < consumers.length; k++) {
      consumers[k] = new Thread(() -> {
        long s = 0;
        try {
          for (long x; (x = ch.take()) != END;) s += x;
        } catch (InterruptedException e) {
          Thread.currentThread().interrupt();
        }
        sum.addAndGet(s);
      });
      consumers[k].start();
    }
    for (int k = 0; k < producers.length; k++) {
      producers[k] = new Thread(() -> {
        try {
          for (long i = 1; i <= 250000; i++) ch.put(i);
        } catch (InterruptedException e) {
          Thread.currentThread().interrupt();
        }
      });
      producers[k].start();
    }
    for (Thread p : producers) p.join();
    for (int k = 0; k < consumers.length; k++) ch.put(END); // the channel closed: one end mark for each consumer
    for (Thread c : consumers) c.join();
    long s = sum.get();
    ConcurrentLinkedQueue<Long> u = new ConcurrentLinkedQueue<>();
    for (long k = 1; k <= 1000000; k++) {
      u.offer(k);
      s += u.poll() % 2;
    }
    Bench.report("channels", t, s == 4 * 31250125000L + 500000);
  }
}
