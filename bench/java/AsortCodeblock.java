// bench/asort_codeblock.prg in Java: the library's stable sort with a comparison over records: 150,000 employees
// { name, department, salary, id } made from a fixed seed, ordered by department, then salary descending, then name.
// The order is checked with a running hash of the ids.
import java.util.Arrays;

public class AsortCodeblock {
  record Employee(String name, long dept, long salary, long id) {}

  static long seed = 4242;

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  public static void main(String[] args) {
    Employee[] a = new Employee[150000];
    for (int i = 0; i < a.length; i++) {
      StringBuilder c = new StringBuilder();
      for (int k = 1; k <= 8; k++) c.append((char) (65 + rand(26)));
      long dept = rand(40) + 1, salary = rand(90000) + 1000;
      a[i] = new Employee(c.toString(), dept, salary, i + 1);
    }
    long t = System.nanoTime(), chk = 0;
    Arrays.sort(a, (p, q) -> p.dept != q.dept ? Long.compare(p.dept, q.dept)
        : p.salary != q.salary ? Long.compare(q.salary, p.salary) : p.name.compareTo(q.name));
    for (Employee e : a) chk = (chk * 31 + e.id) % 1000000007;
    boolean ok = chk == 773935306 && a[0].id == 84060 && a[0].name.equals("SOFHDANH") && a[75000].id == 10853
        && a[149999].id == 11664;
    Bench.report("asort_codeblock", t, ok);
  }
}
