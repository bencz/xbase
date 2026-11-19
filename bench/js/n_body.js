// bench/n_body.prg in JavaScript: the n-body simulation (the benchmarks game's), the Sun and the four giant planets,
// 500,000 steps of 0.01 days with doubles in arrays (one per coordinate), the system's energy before and after.
const { report } = require("./bench.js");

function energy(x, y, z, vx, vy, vz, m) {
  let e = 0.0;
  for (let i = 0; i < m.length; i++) {
    e += 0.5 * m[i] * (vx[i] * vx[i] + vy[i] * vy[i] + vz[i] * vz[i]);
    for (let j = i + 1; j < m.length; j++) {
      const dx = x[i] - x[j], dy = y[i] - y[j], dz = z[i] - z[j];
      e -= (m[i] * m[j]) / Math.sqrt(dx * dx + dy * dy + dz * dz);
    }
  }
  return e;
}

const t = performance.now();
const steps = 500000, nb = 5, pi = 3.141592653589793, days = 365.24, dt = 0.01;
const solarMass = 4.0 * pi * pi;
const x = [0.0, 4.84143144246472090, 8.34336671824457987, 12.8943695621391310, 15.3796971148509165];
const y = [0.0, -1.16032004402742839, 4.12479856412430479, -15.1111514016986312, -25.9193146099879641];
const z = [0.0, -0.103622044471123109, -0.403523417114321381, -0.223307578892655734, 0.179258772950371181];
const vx = [0.0, 0.00166007664274403694 * days, -0.00276742510726862411 * days, 0.00296460137564761618 * days,
  0.00268067772490389322 * days];
const vy = [0.0, 0.00769901118419740425 * days, 0.00499852801234917238 * days, 0.00237847173959480950 * days,
  0.00162824170038242295 * days];
const vz = [0.0, -0.0000690460016972063023 * days, 0.0000230417297573763929 * days,
  -0.0000296589568540237556 * days, -0.0000951592254519715870 * days];
const m = [solarMass, 0.000954791938424326609 * solarMass, 0.000285885980666130812 * solarMass,
  0.0000436624404335156298 * solarMass, 0.0000515138902046611451 * solarMass];
let px = 0.0, py = 0.0, pz = 0.0;
for (let i = 0; i < nb; i++) {
  px += vx[i] * m[i];
  py += vy[i] * m[i];
  pz += vz[i] * m[i];
}
vx[0] = -px / solarMass;
vy[0] = -py / solarMass;
vz[0] = -pz / solarMass;
const e0 = energy(x, y, z, vx, vy, vz, m);
for (let k = 1; k <= steps; k++) {
  for (let i = 0; i < nb; i++) {
    const mi = m[i];
    for (let j = i + 1; j < nb; j++) {
      const dx = x[i] - x[j], dy = y[i] - y[j], dz = z[i] - z[j];
      const d2 = dx * dx + dy * dy + dz * dz;
      const mag = dt / (d2 * Math.sqrt(d2));
      const mj = m[j];
      vx[i] -= dx * mj * mag;
      vy[i] -= dy * mj * mag;
      vz[i] -= dz * mj * mag;
      vx[j] += dx * mi * mag;
      vy[j] += dy * mi * mag;
      vz[j] += dz * mi * mag;
    }
  }
  for (let i = 0; i < nb; i++) {
    x[i] += dt * vx[i];
    y[i] += dt * vy[i];
    z[i] += dt * vz[i];
  }
}
const e1 = energy(x, y, z, vx, vy, vz, m);
report("n_body", t, Math.abs(e0 + 0.169075164) < 0.000000001 && Math.abs(e1 + 0.169096567) < 0.000000001);
