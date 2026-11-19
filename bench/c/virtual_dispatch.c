/* bench/virtual_dispatch.prg in C: 1,000 shapes of 6 classes (structs with a table of methods each) that override
 * Area() and Perimeter() of their parent TShape, made from a fixed seed in random order, gone through 1,000 times:
 * each shape's Area() and Perimeter() through its table from one call site, and Measure(), which TShape alone has and
 * which calls the overriding Area() through self. */
#include "bench.h"
#include <stdlib.h>

typedef struct Shape Shape;
typedef struct {
  int64_t (*area)(const Shape *self);
  int64_t (*perimeter)(const Shape *self);
  int64_t (*measure)(const Shape *self);
} Methods;

struct Shape {
  const Methods *m;
  int64_t x, y;
};

static int64_t seed = 6502;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static int64_t measure(const Shape *s) { return s->m->area(s) + 1; }
static int64_t circle_area(const Shape *s) { return 3 * s->x * s->x; }
static int64_t circle_perimeter(const Shape *s) { return 6 * s->x; }
static int64_t square_area(const Shape *s) { return s->x * s->x; }
static int64_t square_perimeter(const Shape *s) { return 4 * s->x; }
static int64_t rect_area(const Shape *s) { return s->x * s->y; }
static int64_t rect_perimeter(const Shape *s) { return 2 * (s->x + s->y); }
static int64_t triangle_area(const Shape *s) { return s->x * s->y * 2 / 2; }
static int64_t triangle_perimeter(const Shape *s) { return 3 * s->x; }
static int64_t hexagon_area(const Shape *s) { return 6 * s->x * s->x; }
static int64_t hexagon_perimeter(const Shape *s) { return 6 * s->x; }
static int64_t ellipse_area(const Shape *s) { return 3 * s->x * s->y; }
static int64_t ellipse_perimeter(const Shape *s) { return 3 * (s->x + s->y); }

static const Methods classes[] = {
    {circle_area, circle_perimeter, measure},   {square_area, square_perimeter, measure},
    {rect_area, rect_perimeter, measure},       {triangle_area, triangle_perimeter, measure},
    {hexagon_area, hexagon_perimeter, measure}, {ellipse_area, ellipse_perimeter, measure},
};

int main(void) {
  Shape **shapes = malloc(1000 * sizeof *shapes);
  int64_t area = 0, per = 0, m = 0;
  for (int i = 0; i < 1000; ++i) {
    Shape *s = malloc(sizeof *s);
    s->m = &classes[rnd(6)];
    s->x = rnd(100) + 1;
    s->y = rnd(100) + 1;
    shapes[i] = s;
  }
  Shape **volatile ps = shapes;
  long long t = bench_ms();
  for (int i = 1; i <= 1000; ++i) {
    Shape **a = ps;
    for (int k = 0; k < 1000; ++k) {
      const Shape *o = a[k];
      area += o->m->area(o);
      per += o->m->perimeter(o);
      m += o->m->measure(o);
    }
  }
  bench_report("virtual_dispatch", t,
               area == 1000 * 7844067LL && per == 1000 * 245962LL && m == 1000 * (7844067LL + 1000));
  for (int i = 0; i < 1000; ++i) free(shapes[i]);
  free(shapes);
  return 0;
}
