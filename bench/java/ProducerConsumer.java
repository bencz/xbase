// bench/producer_consumer.prg in Java: 4 producer and 4 consumer threads over a bounded queue of 64 guarded by a lock
// and two conditions (not full, not empty): the producers put 50,000 numbers each, the consumers take them until each
// finds the end mark (null) put once the producers are joined.
import java.util.ArrayDeque;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;

public class ProducerConsumer {
  static final ReentrantLock M = new ReentrantLock();
  static final Condition NOT_FULL = M.newCondition(), NOT_EMPTY = M.newCondition();
  static final ArrayDeque<Long> Q = new ArrayDeque<>();
  static final Long END = null;

  static void put(Long x) throws InterruptedException {
    M.lock();
    try {
      while (Q.size() >= 64) NOT_FULL.await();
      Q.add(x == null ? Long.MIN_VALUE : x); // ArrayDeque holds no null: MIN_VALUE stands for the end mark
      NOT_EMPTY.signal();
    } finally {
      M.unlock();
    }
  }

  // Takes numbers until the end mark; returns { their sum, their count }.
  static long[] consume() throws InterruptedException {
    long sum = 0, count = 0;
    while (true) {
      long x;
      M.lock();
      try {
        while (Q.isEmpty()) NOT_EMPTY.await();
        x = Q.poll();
        NOT_FULL.signal();
      } finally {
        M.unlock();
      }
      if (x == Long.MIN_VALUE) break;
      sum += x;
      count++;
    }
    return new long[] {sum, count};
  }

  public static void main(String[] args) throws InterruptedException {
    long t = System.nanoTime(), nSum = 0, nCount = 0;
    int nItems = 50000;
    long[][] results = new long[4][];
    Thread[] aC = new Thread[4], aP = new Thread[4];
    for (int k = 0; k < 4; k++) {
      int c = k;
      aC[k] = new Thread(() -> {
        try {
          results[c] = consume();
        } catch (InterruptedException e) {
          Thread.currentThread().interrupt();
        }
      });
      aC[k].start();
    }
    for (int k = 0; k < 4; k++) {
      long p = k + 1;
      aP[k] = new Thread(() -> {
        try {
          for (long i = 1; i <= nItems; i++) put(p * 1000000 + i);
        } catch (InterruptedException e) {
          Thread.currentThread().interrupt();
        }
      });
      aP[k].start();
    }
    for (Thread p : aP) p.join();
    for (int k = 0; k < 4; k++) put(END);
    for (int k = 0; k < 4; k++) {
      aC[k].join();
      nSum += results[k][0];
      nCount += results[k][1];
    }
    Bench.report("producer_consumer", t,
        nCount == 4L * nItems && nSum == 10000000L * nItems + 2L * nItems * (nItems + 1));
  }
}
