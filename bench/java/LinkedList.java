// bench/linked_list.prg in Java: a singly linked list of nodes { value, next } (a small class for the .prg's
// two-element arrays): 1,000,000 nodes pushed at the head, walked, reversed in place, walked again, and every third
// node unlinked; 3 rounds.
public class LinkedList {
  static final class Node {
    long value;
    Node next;
    Node(long value, Node next) {
      this.value = value;
      this.next = next;
    }
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), sum = 0, count = 0, first = 0;
    for (int r = 1; r <= 3; r++) {
      Node head = null, node, prev, next;
      for (long i = 1; i <= 1000000; i++) head = new Node(i, head);
      for (node = head; node != null; node = node.next) sum += node.value;
      prev = null;
      node = head;
      while (node != null) {
        next = node.next;
        node.next = prev;
        prev = node;
        node = next;
      }
      head = prev;
      first += head.value;
      node = head;
      while (node != null && node.next != null) {
        node.next = node.next.next;
        node = node.next;
      }
      for (node = head; node != null; node = node.next) count++;
    }
    Bench.report("linked_list", t, sum == 1500001500000L && first == 3 && count == 1500000);
  }
}
