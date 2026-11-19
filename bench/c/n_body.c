/* bench/n_body.prg in C: the Sun and the four giant planets, 500,000 steps of 0.01 days, the energy before and
 * after. */
#include "bench.h"
#include <math.h>

#define NB 5

static double energy(const double *x, const double *y, const double *z, const double *vx, const double *vy,
                     const double *vz, const double *m) {
  double e = 0.0;
  for (int i = 0; i < NB; ++i) {
    e += 0.5 * m[i] * (vx[i] * vx[i] + vy[i] * vy[i] + vz[i] * vz[i]);
    for (int j = i + 1; j < NB; ++j) {
      double dx = x[i] - x[j], dy = y[i] - y[j], dz = z[i] - z[j];
      e -= m[i] * m[j] / sqrt(dx * dx + dy * dy + dz * dz);
    }
  }
  return e;
}

int main(void) {
  long long t = bench_ms();
  int64_t steps = 500000;
  double pi = 3.141592653589793, days = 365.24, dt = 0.01, px = 0.0, py = 0.0, pz = 0.0;
  double solarMass = 4.0 * pi * pi;
  double x[NB] = {0.0, 4.84143144246472090, 8.34336671824457987, 12.8943695621391310, 15.3796971148509165};
  double y[NB] = {0.0, -1.16032004402742839, 4.12479856412430479, -15.1111514016986312, -25.9193146099879641};
  double z[NB] = {0.0, -0.103622044471123109, -0.403523417114321381, -0.223307578892655734, 0.179258772950371181};
  double vx[NB] = {0.0, 0.00166007664274403694 * days, -0.00276742510726862411 * days,
                   0.00296460137564761618 * days, 0.00268067772490389322 * days};
  double vy[NB] = {0.0, 0.00769901118419740425 * days, 0.00499852801234917238 * days,
                   0.00237847173959480950 * days, 0.00162824170038242295 * days};
  double vz[NB] = {0.0, -0.0000690460016972063023 * days, 0.0000230417297573763929 * days,
                   -0.0000296589568540237556 * days, -0.0000951592254519715870 * days};
  double m[NB] = {solarMass, 0.000954791938424326609 * solarMass, 0.000285885980666130812 * solarMass,
                  0.0000436624404335156298 * solarMass, 0.0000515138902046611451 * solarMass};
  for (int i = 0; i < NB; ++i) {
    px += vx[i] * m[i];
    py += vy[i] * m[i];
    pz += vz[i] * m[i];
  }
  vx[0] = -px / solarMass;
  vy[0] = -py / solarMass;
  vz[0] = -pz / solarMass;
  double e0 = energy(x, y, z, vx, vy, vz, m);
  for (int64_t k = 1; k <= steps; ++k) {
    for (int i = 0; i < NB; ++i) {
      double mi = m[i];
      for (int j = i + 1; j < NB; ++j) {
        double dx = x[i] - x[j], dy = y[i] - y[j], dz = z[i] - z[j];
        double d2 = dx * dx + dy * dy + dz * dz;
        double mag = dt / (d2 * sqrt(d2));
        double mj = m[j];
        vx[i] -= dx * mj * mag;
        vy[i] -= dy * mj * mag;
        vz[i] -= dz * mj * mag;
        vx[j] += dx * mi * mag;
        vy[j] += dy * mi * mag;
        vz[j] += dz * mi * mag;
      }
    }
    for (int i = 0; i < NB; ++i) {
      x[i] += dt * vx[i];
      y[i] += dt * vy[i];
      z[i] += dt * vz[i];
    }
  }
  double e1 = energy(x, y, z, vx, vy, vz, m);
  bench_report("n_body", t, fabs(e0 + 0.169075164) < 0.000000001 && fabs(e1 + 0.169096567) < 0.000000001);
  return 0;
}
