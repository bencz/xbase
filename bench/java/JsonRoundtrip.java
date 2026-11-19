// bench/json_roundtrip.prg in Java: 20,000 records as maps (an id, a name with quotes, backslashes, tabs and line
// feeds, a list of tags, a boolean, a number, a null, a nested map) from a fixed seed, serialized to JSON text, parsed
// back by a recursive descent parser and serialized again, all written out (Java's standard library has no JSON).
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public class JsonRoundtrip {
  static long seed = 1618;
  static int pos;

  static void toJson(Object x, StringBuilder c) {
    if (x instanceof Map<?, ?> m) {
      String sep = "";
      c.append('{');
      for (Map.Entry<?, ?> e : m.entrySet()) {
        c.append(sep);
        quote((String) e.getKey(), c);
        c.append(':');
        toJson(e.getValue(), c);
        sep = ",";
      }
      c.append('}');
    } else if (x instanceof List<?> a) {
      String sep = "";
      c.append('[');
      for (Object v : a) {
        c.append(sep);
        toJson(v, c);
        sep = ",";
      }
      c.append(']');
    } else if (x instanceof String s) {
      quote(s, c);
    } else if (x instanceof Long n) {
      c.append(n.longValue());
    } else if (x instanceof Boolean b) {
      c.append(b ? "true" : "false");
    } else {
      c.append("null");
    }
  }

  static void quote(String s, StringBuilder c) {
    c.append('"').append(s.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\t", "\\t"))
        .append('"');
  }

  static String toJson(Object x) {
    StringBuilder c = new StringBuilder();
    toJson(x, c);
    return c.toString();
  }

  // The value that starts at pos, which it leaves after the value.
  static Object parse(String c) {
    char ch = c.charAt(pos);
    switch (ch) {
      case '{': {
        Map<String, Object> x = new LinkedHashMap<>();
        if (c.charAt(++pos) == '}') {
          pos++;
          return x;
        }
        while (true) {
          String k = parseString(c);
          pos++;
          x.put(k, parse(c));
          if (c.charAt(pos++) == '}') return x;
        }
      }
      case '[': {
        List<Object> x = new ArrayList<>();
        if (c.charAt(++pos) == ']') {
          pos++;
          return x;
        }
        while (true) {
          x.add(parse(c));
          if (c.charAt(pos++) == ']') return x;
        }
      }
      case '"':
        return parseString(c);
      case 't':
        pos += 4;
        return Boolean.TRUE;
      case 'f':
        pos += 5;
        return Boolean.FALSE;
      case 'n':
        pos += 4;
        return null;
      default: {
        int k = pos;
        while (pos < c.length() && "-0123456789".indexOf(c.charAt(pos)) >= 0) pos++;
        return Long.parseLong(c.substring(k, pos));
      }
    }
  }

  static String parseString(String c) {
    StringBuilder out = new StringBuilder();
    int start = ++pos;
    char ch;
    while ((ch = c.charAt(pos)) != '"') {
      if (ch == '\\') {
        out.append(c, start, pos);
        ch = c.charAt(pos + 1);
        out.append(ch == 'n' ? '\n' : ch == 't' ? '\t' : ch);
        pos += 2;
        start = pos;
      } else {
        pos++;
      }
    }
    out.append(c, start, pos);
    pos++;
    return out.toString();
  }

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  // A record, drawn from the seed in the order the checks' constants were computed.
  static Map<String, Object> makeRecord(long i) {
    String chars = "abcdefghijk ABC\"\\\n\txyz";
    StringBuilder name = new StringBuilder();
    long n = rand(11) + 5;
    for (long k = 1; k <= n; k++) name.append(chars.charAt((int) rand(22)));
    n = rand(4);
    boolean active = rand(2) == 1;
    long qty = rand(2001) - 1000;
    Long parent = null;
    if (rand(3) != 0) parent = rand(i) + 1;
    String city = "City " + rand(50);
    long zip = rand(90000) + 10000;
    List<Object> tags = new ArrayList<>();
    for (long k = 1; k <= n; k++) tags.add("tag" + rand(100));
    Map<String, Object> address = new LinkedHashMap<>();
    address.put("city", city);
    address.put("zip", zip);
    Map<String, Object> h = new LinkedHashMap<>();
    h.put("id", i);
    h.put("name", name.toString());
    h.put("tags", tags);
    h.put("active", active);
    h.put("qty", qty);
    h.put("parent", parent);
    h.put("address", address);
    return h;
  }

  public static void main(String[] args) {
    List<Object> data = new ArrayList<>();
    for (long i = 1; i <= 20000; i++) data.add(makeRecord(i));
    long t = System.nanoTime(), qty = 0, active = 0, tags = 0, nulls = 0, zip = 0;
    String c1 = toJson(data);
    pos = 0;
    List<?> back = (List<?>) parse(c1);
    String c2 = toJson(back);
    for (Object o : back) {
      Map<?, ?> h = (Map<?, ?>) o;
      qty += (Long) h.get("qty");
      active += (Boolean) h.get("active") ? 1 : 0;
      tags += ((List<?>) h.get("tags")).size();
      nulls += h.get("parent") == null ? 1 : 0;
      zip += (Long) ((Map<?, ?>) h.get("address")).get("zip");
    }
    boolean ok = c1.equals(c2) && c1.length() == 2697804 && pos == c1.length() && back.size() == 20000
        && qty == -88759 && active == 9960 && tags == 29729 && nulls == 6525 && zip == 1098378340
        && ((Map<?, ?>) back.get(0)).get("name").equals("ykAeACczkccifkx")
        && ((Map<?, ?>) back.get(1)).get("name").equals("Cdkk\"\\Ae\nx\nz")
        && c1.startsWith("[{\"id\":1,\"name\":\"ykAeACczkccifkx\"");
    Bench.report("json_roundtrip", t, ok);
  }
}
