// bench/ray_tracer.prg in Java: small arrays of doubles made and dropped, method calls: a 320 x 240 image of five
// spheres ray traced (diffuse light, shadows, reflections three deep), vectors as arrays of three doubles, checked by
// the sum of the pixels' gray levels, 2 times.
public class RayTracer {
  // A sphere: its center, its radius, how much it reflects and its albedo.
  record Sphere(double[] center, double radius, double reflect, double albedo) {}

  static double[] vAdd(double[] a, double[] b) { return new double[] {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
  static double[] vSub(double[] a, double[] b) { return new double[] {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
  static double[] vScale(double[] a, double s) { return new double[] {a[0] * s, a[1] * s, a[2] * s}; }
  static double vDot(double[] a, double[] b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
  static double[] vNorm(double[] a) { return vScale(a, 1.0 / Math.sqrt(vDot(a, a))); }

  // The distance along dir from origin to sphere s, or 0 when the ray misses it.
  static double intersect(Sphere s, double[] origin, double[] dir) {
    double[] oc = vSub(origin, s.center());
    double b = vDot(oc, dir), disc = b * b - (vDot(oc, oc) - s.radius() * s.radius());
    if (disc > 0.0) {
      double q = Math.sqrt(disc), t = -b - q;
      if (t < 0.0001) t = -b + q;
      if (t > 0.0001) return t;
    }
    return 0.0;
  }

  // The gray level seen along dir from origin.
  static double trace(Sphere[] spheres, double[] light, double[] origin, double[] dir, int depth) {
    double best = 0.0;
    Sphere hit = null;
    for (Sphere s : spheres) {
      double t = intersect(s, origin, dir);
      if (t > 0.0 && (hit == null || t < best)) {
        best = t;
        hit = s;
      }
    }
    if (hit == null) return 0.2 + 0.5 * Math.max(0.0, dir[1]);
    double[] p = vAdd(origin, vScale(dir, best));
    double[] n = vScale(vSub(p, hit.center()), 1.0 / hit.radius());
    double[] toLight = vSub(light, p);
    double dist = Math.sqrt(vDot(toLight, toLight));
    double[] l = vScale(toLight, 1.0 / dist);
    double diff = vDot(n, l);
    for (Sphere s : spheres) {
      if (diff <= 0.0) break;
      double t = intersect(s, p, l);
      if (t > 0.0 && t < dist) diff = 0.0;
    }
    double col = hit.albedo() * (0.1 + 0.9 * Math.max(diff, 0.0));
    if (depth < 3 && hit.reflect() > 0.0)
      col = col * (1.0 - hit.reflect())
          + hit.reflect() * trace(spheres, light, p, vSub(dir, vScale(n, 2.0 * vDot(dir, n))), depth + 1);
    return col;
  }

  public static void main(String[] args) {
    long t = System.nanoTime();
    int width = 320, height = 240;
    boolean ok = true;
    Sphere[] spheres = {new Sphere(new double[] {0.0, -1000.0, 0.0}, 999.0, 0.2, 0.6),
        new Sphere(new double[] {0.0, 0.0, 5.0}, 1.0, 0.5, 0.9),
        new Sphere(new double[] {-2.0, 0.0, 6.0}, 1.0, 0.3, 0.7),
        new Sphere(new double[] {2.0, 0.0, 6.0}, 1.0, 0.3, 0.8),
        new Sphere(new double[] {0.0, 2.2, 7.0}, 1.0, 0.6, 0.5)};
    double[] light = {5.0, 10.0, -5.0}, eye = {0.0, 0.5, -1.0};
    for (int rep = 1; rep <= 2; rep++) {
      long total = 0;
      for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
          total += (long) (Math.min(trace(spheres, light, eye, vNorm(new double[] {(x - width / 2.0 + 0.5) / height,
              (height / 2.0 - y - 0.5) / height, 1.0}), 0), 1.0) * 255.0);
      ok = ok && total == 7100994;
    }
    Bench.report("ray_tracer", t, ok);
  }
}
