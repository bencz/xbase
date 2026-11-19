// bench/sequential_scan.prg in Java: sequential scans of a table: 200,000 sales records (region, amount, quantity,
// customer, date) made from a fixed seed in a file of fixed-width text records as a DBF holds them (a deletion flag,
// numbers right-aligned, the date as YYYYMMDD; "bench_seqscan.dat", no header), then timed, 4 rounds of six passes
// over every record read through a buffer of records: a loop with skip, a sum, a count with a condition, an
// evaluation with a condition, a locate with continue, and a sum over a date range. The file is deleted at the end.
import java.io.BufferedOutputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.LocalDate;
import java.time.format.DateTimeFormatter;

public class SequentialScan {
  static final String FILE = "bench_seqscan.dat";
  // Field offsets and widths in a record: the deletion flag, REGION N 1, AMOUNT N 10, QTY N 3, CUST C 10, SALEDATE D 8.
  static final int REGION = 1, AMOUNT = 2, QTY = 12, CUST = 15, SALEDATE = 25, RECLEN = 33, BUFRECS = 512;

  static long seed = 31337;

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  // A table of fixed-width records read through a buffer of BUFRECS records; record numbers from 1.
  static final class Table implements AutoCloseable {
    final RandomAccessFile file;
    final long recCount;
    final byte[] buf = new byte[RECLEN * BUFRECS];
    long recNo, bufFirst = -1;
    int bufCount;

    Table(String name) throws IOException {
      file = new RandomAccessFile(name, "r");
      recCount = file.length() / RECLEN;
    }

    boolean eof() { return recNo > recCount; }

    void goTo(long rec) throws IOException {
      recNo = rec;
      if (rec <= recCount && (rec < bufFirst || rec >= bufFirst + bufCount)) {
        bufFirst = rec;
        bufCount = (int) Math.min(BUFRECS, recCount - rec + 1);
        file.seek((rec - 1) * RECLEN);
        file.readFully(buf, 0, bufCount * RECLEN);
      }
    }

    void goTop() throws IOException { goTo(1); }

    void skip() throws IOException { goTo(recNo + 1); }

    // The number in a numeric field: digits after leading blanks.
    long num(int off, int width) {
      int p = (int) (recNo - bufFirst) * RECLEN + off;
      long v = 0;
      for (int i = 0; i < width; i++) {
        byte b = buf[p + i];
        if (b != ' ') v = v * 10 + (b - '0');
      }
      return v;
    }

    String chars(int off, int width) {
      return new String(buf, (int) (recNo - bufFirst) * RECLEN + off, width, StandardCharsets.ISO_8859_1);
    }

    @Override public void close() throws IOException { file.close(); }
  }

  static void put(byte[] rec, int off, String s) {
    byte[] b = s.getBytes(StandardCharsets.ISO_8859_1);
    System.arraycopy(b, 0, rec, off, b.length);
  }

  static boolean scans(Table db) throws IOException {
    long sumR3 = 0, cntR3 = 0, all = 0, big = 0, qty = 0, loc = 0, locIds = 0, jan = 0;
    for (db.goTop(); !db.eof(); db.skip()) {
      if (db.num(REGION, 1) == 3) {
        sumR3 += db.num(AMOUNT, 10);
        cntR3++;
      }
    }
    for (db.goTop(); !db.eof(); db.skip()) all += db.num(AMOUNT, 10);
    for (db.goTop(); !db.eof(); db.skip())
      if (db.num(AMOUNT, 10) > 900000) big++;
    for (db.goTop(); !db.eof(); db.skip())
      if (db.num(REGION, 1) <= 2) qty += db.num(QTY, 3);
    for (db.goTop(); !db.eof(); db.skip()) {
      if (db.chars(CUST, 10).substring(0, 8).equals("CUST0001")) {
        loc++;
        locIds += db.recNo;
      }
    }
    for (db.goTop(); !db.eof(); db.skip())
      if (db.num(SALEDATE, 8) < 20260201) jan += db.num(AMOUNT, 10);
    return sumR3 == 11099768125L && cntR3 == 22094 && all == 99898922935L && big == 19973 && qty == 1135702
        && loc == 396 && locIds == 40072280 && jan == 8435099403L;
  }

  public static void main(String[] args) throws IOException {
    LocalDate base = LocalDate.of(2026, 1, 1);
    try (OutputStream out = new BufferedOutputStream(new FileOutputStream(FILE))) {
      byte[] rec = new byte[RECLEN];
      for (int i = 1; i <= 200000; i++) {
        long region = rand(9) + 1, amount = rand(1000000), qty = rand(50) + 1;
        LocalDate date = base.plusDays(rand(365));
        String cust = "CUST" + String.format("%06d", rand(50000));
        put(rec, 0, " ");
        put(rec, REGION, String.format("%1d", region));
        put(rec, AMOUNT, String.format("%10d", amount));
        put(rec, QTY, String.format("%3d", qty));
        put(rec, CUST, cust);
        put(rec, SALEDATE, date.format(DateTimeFormatter.BASIC_ISO_DATE));
        out.write(rec);
      }
    }
    long t = System.nanoTime();
    boolean ok = true;
    try (Table db = new Table(FILE)) {
      for (int i = 1; i <= 4; i++) ok = ok && scans(db);
    }
    Bench.report("sequential_scan", t, ok);
    Files.delete(Path.of(FILE));
  }
}
