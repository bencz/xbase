// bench/ini_parse.prg in Java: INI files: a text of 400 sections of 25 keys (comments, blank lines, spaces around "=",
// quoted values, section names in either case) made from a fixed seed, then timed: parsed 80 times into a HashMap of
// sections, each a HashMap of keys (lines split with split, trimmed, names lowered, quotes taken off); the numbers
// summed and values looked up.
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Locale;

public class IniParse {
  static long seed = 7777;
  static long quoted;

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  // The sections of the text, each a map of its keys; quoted gets the count of quoted values.
  static HashMap<String, HashMap<String, String>> parseIni(String c) {
    HashMap<String, HashMap<String, String>> ini = new HashMap<>();
    HashMap<String, String> section = null;
    quoted = 0;
    for (String line : c.split("\n")) {
      line = line.strip();
      if (line.isEmpty() || line.charAt(0) == ';' || line.charAt(0) == '#') continue;
      int eq;
      if (line.charAt(0) == '[') {
        section = new HashMap<>();
        ini.put(line.substring(1, line.length() - 1).toLowerCase(Locale.ROOT), section);
      } else if ((eq = line.indexOf('=')) >= 0) {
        String key = line.substring(0, eq).stripTrailing().toLowerCase(Locale.ROOT);
        String val = line.substring(eq + 1).stripLeading();
        if (val.length() >= 2 && val.charAt(0) == '"' && val.charAt(val.length() - 1) == '"') {
          val = val.substring(1, val.length() - 1);
          quoted++;
        }
        if (section != null) section.put(key, val);
      }
    }
    return ini;
  }

  public static void main(String[] args) {
    ArrayList<String> lines = new ArrayList<>();
    for (long s = 1; s <= 400; s++) {
      lines.add("; section " + s);
      lines.add("[" + (rand(2) == 1 ? "Section" : "SECTION") + s + "]");
      for (long k = 1; k <= 25; k++) {
        long sp = rand(3);
        String n = Long.toString(rand(100000));
        String val = k % 5 == 0 ? n : k % 5 == 1 ? "\"text " + n + "\"" : "w" + n;
        lines.add((sp == 2 ? "  " : "") + "Key" + String.format("%02d", k) + (sp > 0 ? " = " : "=") + val);
        if (k % 7 == 0) lines.add("# comment");
      }
      lines.add("");
    }
    StringBuilder sb = new StringBuilder();
    for (String x : lines) sb.append(x).append('\n');
    String c = sb.toString();
    long t = System.nanoTime();
    boolean ok = true;
    for (int i = 1; i <= 80; i++) {
      HashMap<String, HashMap<String, String>> h = parseIni(c);
      long sum = 0;
      for (HashMap<String, String> x : h.values())
        for (int k = 5; k <= 25; k += 5) sum += Long.parseLong(x.get("key" + String.format("%02d", k)));
      ok = ok && h.size() == 400 && sum == 102092980 && quoted == 2000
          && h.get("section77").get("key11").equals("text 72557") && h.get("section400").size() == 25;
    }
    ok = ok && c.length() == 181694;
    Bench.report("ini_parse", t, ok);
  }
}
