// bench/virtual_dispatch.prg in Java: 1,000 shapes of 6 classes that override area() and perimeter() of their parent
// TShape (from a fixed seed, in random order), gone through 1,000 times: area(), perimeter() and measure(), which
// TShape alone has and which calls the overriding area() through this.
import java.util.List;
import java.util.function.Supplier;

public class VirtualDispatch {
  static long seed = 6502;

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  static class TShape {
    long x, y;
    TShape init(long x, long y) {
      this.x = x;
      this.y = y;
      return this;
    }
    long area() { return 0; }
    long perimeter() { return 0; }
    long measure() { return area() + 1; }
  }

  static class TCircle extends TShape {
    @Override long area() { return 3 * x * x; }
    @Override long perimeter() { return 6 * x; }
  }

  static class TSquare extends TShape {
    @Override long area() { return x * x; }
    @Override long perimeter() { return 4 * x; }
  }

  static class TRect extends TShape {
    @Override long area() { return x * y; }
    @Override long perimeter() { return 2 * (x + y); }
  }

  static class TTriangle extends TShape {
    @Override long area() { return x * y * 2 / 2; }
    @Override long perimeter() { return 3 * x; }
  }

  static class THexagon extends TShape {
    @Override long area() { return 6 * x * x; }
    @Override long perimeter() { return 6 * x; }
  }

  static class TEllipse extends TShape {
    @Override long area() { return 3 * x * y; }
    @Override long perimeter() { return 3 * (x + y); }
  }

  public static void main(String[] args) {
    TShape[] shapes = new TShape[1000];
    List<Supplier<TShape>> classes = List.of(TCircle::new, TSquare::new, TRect::new, TTriangle::new, THexagon::new,
        TEllipse::new);
    for (int i = 0; i < shapes.length; i++) {
      TShape o = classes.get((int) rand(6)).get();
      shapes[i] = o.init(rand(100) + 1, rand(100) + 1);
    }
    long t = System.nanoTime(), area = 0, per = 0, m = 0;
    for (int i = 1; i <= 1000; i++)
      for (TShape o : shapes) {
        area += o.area();
        per += o.perimeter();
        m += o.measure();
      }
    Bench.report("virtual_dispatch", t, area == 1000L * 7844067 && per == 1000L * 245962
        && m == 1000L * (7844067 + 1000));
  }
}
