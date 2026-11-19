/* bench/ray_tracer.prg in C: a 320 x 240 image of five spheres ray traced (diffuse light, shadows, reflections three
 * deep), vectors as values of three doubles, checked by the sum of the pixels' gray levels, 2 times. */
#include "bench.h"
#include <math.h>

typedef struct { double x, y, z; } vec;
typedef struct { vec c; double r, refl, albedo; } sphere;

static const sphere spheres[] = {
  {{0.0, -1000.0, 0.0}, 999.0, 0.2, 0.6}, {{0.0, 0.0, 5.0}, 1.0, 0.5, 0.9}, {{-2.0, 0.0, 6.0}, 1.0, 0.3, 0.7},
  {{2.0, 0.0, 6.0}, 1.0, 0.3, 0.8},       {{0.0, 2.2, 7.0}, 1.0, 0.6, 0.5},
};
enum { NSPHERES = sizeof spheres / sizeof spheres[0], WIDTH = 320, HEIGHT = 240 };
static const vec light = {5.0, 10.0, -5.0};
static const double eps = 0.0001;

static vec add(vec a, vec b) { return (vec){a.x + b.x, a.y + b.y, a.z + b.z}; }
static vec sub(vec a, vec b) { return (vec){a.x - b.x, a.y - b.y, a.z - b.z}; }
static vec scale(vec a, double s) { return (vec){a.x * s, a.y * s, a.z * s}; }
static double dot(vec a, vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static vec norm(vec a) { return scale(a, 1.0 / sqrt(dot(a, a))); }

/* The distance along d from o to sphere s, or 0 when the ray misses it. */
static double intersect(const sphere *s, vec o, vec d) {
  vec oc = sub(o, s->c);
  double b = dot(oc, d), disc = b * b - (dot(oc, oc) - s->r * s->r);
  if (disc > 0.0) {
    double q = sqrt(disc), t = -b - q;
    if (t < eps) t = -b + q;
    if (t > eps) return t;
  }
  return 0.0;
}

static double trace(vec o, vec d, int depth) {
  double best = 0.0;
  const sphere *hit = NULL;
  for (int i = 0; i < NSPHERES; ++i) {
    double t = intersect(&spheres[i], o, d);
    if (t > 0.0 && (hit == NULL || t < best)) {
      best = t;
      hit = &spheres[i];
    }
  }
  if (hit == NULL) return 0.2 + 0.5 * fmax(0.0, d.y);
  vec p = add(o, scale(d, best)), n = scale(sub(p, hit->c), 1.0 / hit->r), toLight = sub(light, p);
  double dist = sqrt(dot(toLight, toLight)), diff;
  vec l = scale(toLight, 1.0 / dist);
  diff = dot(n, l);
  for (int i = 0; i < NSPHERES && diff > 0.0; ++i) {
    double t = intersect(&spheres[i], p, l);
    if (t > 0.0 && t < dist) diff = 0.0;
  }
  double col = hit->albedo * (0.1 + 0.9 * fmax(diff, 0.0));
  if (depth < 3 && hit->refl > 0.0) {
    vec r = sub(d, scale(n, 2.0 * dot(d, n)));
    col = col * (1.0 - hit->refl) + hit->refl * trace(p, r, depth + 1);
  }
  return col;
}

int main(void) {
  long long t = bench_ms();
  int ok = 1;
  for (int rep = 1; rep <= 2; ++rep) {
    int64_t total = 0;
    vec eye = {0.0, 0.5, -1.0};
    for (int y = 0; y < HEIGHT; ++y)
      for (int x = 0; x < WIDTH; ++x) {
        vec d = norm((vec){(x - WIDTH / 2.0 + 0.5) / HEIGHT, (HEIGHT / 2.0 - y - 0.5) / HEIGHT, 1.0});
        total += (int64_t)(fmin(trace(eye, d, 0), 1.0) * 255.0);
      }
    ok = ok && total == 7100994;
  }
  bench_report("ray_tracer", t, ok);
  return 0;
}
