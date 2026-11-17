// bench/crc32.prg in Java: CRC-32 of 20,000 bytes (the table made each time, the data built in chunks), 100 times.
public class Crc32 {
  static long of20K() {
    int[] table = new int[256];
    for (int i = 0; i < 256; i++) {
      int c = i;
      for (int k = 0; k < 8; k++) c = (c >>> 1) ^ (0xEDB88320 & -(c & 1));
      table[i] = c;
    }
    StringBuilder data = new StringBuilder();
    for (int i = 0; i < 20000; i += 100) {
      StringBuilder chunk = new StringBuilder();
      for (int k = i; k < i + 100; k++) chunk.append((char) ((k * 31 + 7) % 251));
      data.append(chunk);
    }
    int crc = 0xFFFFFFFF;
    for (int i = 0; i < data.length(); i++) crc = table[(crc ^ data.charAt(i)) & 0xFF] ^ (crc >>> 8);
    return (crc ^ 0xFFFFFFFF) & 0xFFFFFFFFL;
  }

  public static void main(String[] args) {
    long t = System.nanoTime();
    boolean ok = true;
    for (int rep = 1; rep <= 100; rep++) ok = ok && of20K() == 2954121016L;
    Bench.report("crc32", t, ok);
  }
}
