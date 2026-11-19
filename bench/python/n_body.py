# bench/n_body.prg in Python: the n-body simulation (the benchmarks game's), the Sun and the four giant planets,
# 500,000 steps of 0.01 days with floats in lists (one per coordinate), the system's energy before and after.
import math

from bench import report, start


def energy(x, y, z, vx, vy, vz, m):
    e = 0.0
    for i in range(len(m)):
        e += 0.5 * m[i] * (vx[i] * vx[i] + vy[i] * vy[i] + vz[i] * vz[i])
        for j in range(i + 1, len(m)):
            dx = x[i] - x[j]
            dy = y[i] - y[j]
            dz = z[i] - z[j]
            e -= m[i] * m[j] / math.sqrt(dx * dx + dy * dy + dz * dz)
    return e


def run():
    steps, nb, pi, days, dt = 500000, 5, 3.141592653589793, 365.24, 0.01
    px = py = pz = 0.0
    solar_mass = 4.0 * pi * pi
    x = [0.0, 4.84143144246472090, 8.34336671824457987, 12.8943695621391310, 15.3796971148509165]
    y = [0.0, -1.16032004402742839, 4.12479856412430479, -15.1111514016986312, -25.9193146099879641]
    z = [0.0, -0.103622044471123109, -0.403523417114321381, -0.223307578892655734, 0.179258772950371181]
    vx = [0.0, 0.00166007664274403694 * days, -0.00276742510726862411 * days, 0.00296460137564761618 * days,
          0.00268067772490389322 * days]
    vy = [0.0, 0.00769901118419740425 * days, 0.00499852801234917238 * days, 0.00237847173959480950 * days,
          0.00162824170038242295 * days]
    vz = [0.0, -0.0000690460016972063023 * days, 0.0000230417297573763929 * days, -0.0000296589568540237556 * days,
          -0.0000951592254519715870 * days]
    m = [solar_mass, 0.000954791938424326609 * solar_mass, 0.000285885980666130812 * solar_mass,
         0.0000436624404335156298 * solar_mass, 0.0000515138902046611451 * solar_mass]
    for i in range(nb):
        px += vx[i] * m[i]
        py += vy[i] * m[i]
        pz += vz[i] * m[i]
    vx[0] = -px / solar_mass
    vy[0] = -py / solar_mass
    vz[0] = -pz / solar_mass
    e0 = energy(x, y, z, vx, vy, vz, m)
    for _ in range(steps):
        for i in range(nb):
            mi = m[i]
            for j in range(i + 1, nb):
                dx = x[i] - x[j]
                dy = y[i] - y[j]
                dz = z[i] - z[j]
                d2 = dx * dx + dy * dy + dz * dz
                mag = dt / (d2 * math.sqrt(d2))
                mj = m[j]
                vx[i] -= dx * mj * mag
                vy[i] -= dy * mj * mag
                vz[i] -= dz * mj * mag
                vx[j] += dx * mi * mag
                vy[j] += dy * mi * mag
                vz[j] += dz * mi * mag
        for i in range(nb):
            x[i] += dt * vx[i]
            y[i] += dt * vy[i]
            z[i] += dt * vz[i]
    e1 = energy(x, y, z, vx, vy, vz, m)
    return abs(e0 + 0.169075164) < 0.000000001 and abs(e1 + 0.169096567) < 0.000000001


t = start()
report("n_body", t, run())
