// bench/lru_cache.prg in Java: a least-recently-used cache of 2,000 entries, a hash map from key to node and a doubly
// linked list of nodes { key, value, prev, next } in order of use (written out); 2,000,000 reads of keys skewed
// toward small numbers, a miss computing the value and evicting the oldest entry when the cache is full.
import java.util.HashMap;
import java.util.Map;

public class LruCache {
  static long seed = 1234;

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  static final class Node {
    long key, value;
    Node prev, next;
    Node(long key, long value) {
      this.key = key;
      this.value = value;
    }
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), hits = 0, sum = 0;
    int cap = 2000;
    Map<Long, Node> h = new HashMap<>();
    Node head = new Node(0, 0), tail = new Node(0, 0), node;
    head.next = tail;
    tail.prev = head;
    for (int i = 1; i <= 2000000; i++) {
      long k = rand(rand(20000) + 1);
      node = h.get(k);
      if (node != null) {
        hits++;
        node.prev.next = node.next;
        node.next.prev = node.prev;
      } else {
        if (h.size() >= cap) {
          node = tail.prev;
          node.prev.next = tail;
          tail.prev = node.prev;
          h.remove(node.key);
        }
        node = new Node(k, k * 3 + 1);
        h.put(k, node);
      }
      node.next = head.next;
      node.prev = head;
      head.next.prev = node;
      head.next = node;
      sum += node.value;
    }
    Bench.report("lru_cache", t, hits == 378837 && sum == 30033837356L && h.size() == cap);
  }
}
