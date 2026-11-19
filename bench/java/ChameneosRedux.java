// bench/chameneos_redux.prg in Java: threads and queues: creatures on threads of their own ask for meetings on one
// LinkedBlockingQueue; a broker pairs them two by two and answers each on its own queue with the other's name and
// color, after which both change color; 30,000 meetings with 3 creatures, then 30,000 with 10. Checked: the color
// table, and per run 2N meetings counted, none of a creature with itself.
import java.util.ArrayList;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

public class ChameneosRedux {
  static final int BLUE = 0, RED = 1, YELLOW = 2;

  // A request carries the reply queue; an answer has none, and DONE says that no meeting is left.
  record Message(int id, int color, BlockingQueue<Message> reply) {}

  static final Message DONE = new Message(0, -1, null);

  static int complement(int a, int b) { return a == b ? a : 3 - a - b; }

  // A creature: asks for meetings until none is left; gives { meetings, meetings with itself }.
  static final class Creature extends Thread {
    final BlockingQueue<Message> meet, reply = new LinkedBlockingQueue<>();
    final int id;
    int color;
    long met, same;

    Creature(BlockingQueue<Message> meet, int id, int color) {
      this.meet = meet;
      this.id = id;
      this.color = color;
    }

    @Override public void run() {
      try {
        while (true) {
          meet.put(new Message(id, color, reply));
          Message other = reply.take();
          if (other == DONE) break;
          color = complement(color, other.color);
          met++;
          if (other.id == id) same++;
        }
      } catch (InterruptedException e) {
        Thread.currentThread().interrupt();
      }
    }
  }

  // Whether n meetings of creatures of these colors count 2n meetings, none with itself; the caller is the broker.
  static boolean run(long n, int[] colors) throws InterruptedException {
    BlockingQueue<Message> meet = new LinkedBlockingQueue<>();
    ArrayList<Creature> threads = new ArrayList<>();
    long left = n, met = 0, same = 0;
    int done = 0;
    Message first = null;
    for (int i = 0; i < colors.length; i++) {
      Creature c = new Creature(meet, i + 1, colors[i]);
      c.start();
      threads.add(c);
    }
    while (done < colors.length) {
      Message req = meet.take();
      if (left == 0) {
        req.reply.put(DONE);
        done++;
      } else if (first == null) {
        first = req;
      } else {
        first.reply.put(new Message(req.id, req.color, null));
        req.reply.put(new Message(first.id, first.color, null));
        first = null;
        left--;
      }
    }
    for (Creature c : threads) {
      c.join();
      met += c.met;
      same += c.same;
    }
    return met == 2 * n && same == 0;
  }

  public static void main(String[] args) throws InterruptedException {
    long t = System.nanoTime();
    StringBuilder table = new StringBuilder();
    for (int a = 0; a <= 2; a++)
      for (int b = 0; b <= 2; b++) table.append(complement(a, b));
    boolean ok = table.toString().equals("021210102");
    ok = run(30000, new int[] {BLUE, RED, YELLOW}) && ok;
    ok = run(30000, new int[] {BLUE, RED, YELLOW, RED, YELLOW, BLUE, RED, YELLOW, RED, BLUE}) && ok;
    Bench.report("chameneos_redux", t, ok);
  }
}
