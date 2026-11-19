// bench/trie.prg in Java: a trie of HashMaps, one per node, from a letter to the node: 100,000 words of 3 to 10
// letters from an 8-letter alphabet inserted, then 100,000 other words of the same kind looked up, as words and as
// prefixes of the words stored.
import java.util.HashMap;

public class Trie {
  static class Node {
    final HashMap<Character, Node> children = new HashMap<>();
    long words;
  }

  static long seed = 2718;

  static int rand(int n) {
    seed = (seed * 16807) % 2147483647;
    return (int) (seed % n);
  }

  static String newWord() {
    int n = rand(8) + 3;
    StringBuilder w = new StringBuilder();
    for (int i = 1; i <= n; i++) w.append("etaoinsr".charAt(rand(rand(8) + 1)));
    return w.toString();
  }

  public static void main(String[] args) {
    String[] aWords = new String[100000], aProbe = new String[100000];
    for (int i = 0; i < aWords.length; i++) {
      aWords[i] = newWord();
      aProbe[i] = newWord();
    }
    long t = System.nanoTime(), nNodes = 1, nFound = 0, nPrefix = 0;
    Node root = new Node();
    for (String w : aWords) {
      Node node = root;
      for (int j = 0; j < w.length(); j++) {
        char c = w.charAt(j);
        Node next = node.children.get(c);
        if (next == null) {
          next = new Node();
          node.children.put(c, next);
          nNodes++;
        }
        node = next;
      }
      node.words++;
    }
    for (String w : aProbe) {
      Node node = root;
      for (int j = 0; j < w.length() && node != null; j++) node = node.children.get(w.charAt(j));
      if (node != null) {
        nPrefix++;
        if (node.words > 0) nFound++;
      }
    }
    Bench.report("trie", t, nNodes == 159557 && nFound == 40767 && nPrefix == 50841);
  }
}
