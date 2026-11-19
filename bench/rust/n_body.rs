// bench/n_body.prg in Rust: the n-body simulation (the benchmarks game's): the Sun and the four giant planets, 500,000
// steps of 0.01 days with doubles in arrays (one per coordinate), the system's energy before and after.
use std::hint::black_box;
use std::time::Instant;

fn energy(x: &[f64], y: &[f64], z: &[f64], vx: &[f64], vy: &[f64], vz: &[f64], m: &[f64]) -> f64 {
    let mut e = 0.0;
    for i in 0..m.len() {
        e += 0.5 * m[i] * (vx[i] * vx[i] + vy[i] * vy[i] + vz[i] * vz[i]);
        for j in i + 1..m.len() {
            let dx = x[i] - x[j];
            let dy = y[i] - y[j];
            let dz = z[i] - z[j];
            e -= m[i] * m[j] / (dx * dx + dy * dy + dz * dz).sqrt();
        }
    }
    e
}

fn main() {
    let t = Instant::now();
    let steps = black_box(500000);
    let nb = 5;
    let pi = 3.141592653589793;
    let days = 365.24;
    let dt = 0.01;
    let solar_mass = 4.0 * pi * pi;
    let mut x = vec![0.0, 4.84143144246472090, 8.34336671824457987, 12.8943695621391310, 15.3796971148509165];
    let mut y = vec![0.0, -1.16032004402742839, 4.12479856412430479, -15.1111514016986312, -25.9193146099879641];
    let mut z = vec![0.0, -0.103622044471123109, -0.403523417114321381, -0.223307578892655734, 0.179258772950371181];
    let mut vx = vec![0.0, 0.00166007664274403694 * days, -0.00276742510726862411 * days,
        0.00296460137564761618 * days, 0.00268067772490389322 * days];
    let mut vy = vec![0.0, 0.00769901118419740425 * days, 0.00499852801234917238 * days,
        0.00237847173959480950 * days, 0.00162824170038242295 * days];
    let mut vz = vec![0.0, -0.0000690460016972063023 * days, 0.0000230417297573763929 * days,
        -0.0000296589568540237556 * days, -0.0000951592254519715870 * days];
    let m = vec![solar_mass, 0.000954791938424326609 * solar_mass, 0.000285885980666130812 * solar_mass,
        0.0000436624404335156298 * solar_mass, 0.0000515138902046611451 * solar_mass];
    let (mut px, mut py, mut pz) = (0.0, 0.0, 0.0);
    for i in 0..nb {
        px += vx[i] * m[i];
        py += vy[i] * m[i];
        pz += vz[i] * m[i];
    }
    vx[0] = -px / solar_mass;
    vy[0] = -py / solar_mass;
    vz[0] = -pz / solar_mass;
    let e0 = energy(&x, &y, &z, &vx, &vy, &vz, &m);
    for _ in 0..steps {
        for i in 0..nb {
            let mi = m[i];
            for j in i + 1..nb {
                let dx = x[i] - x[j];
                let dy = y[i] - y[j];
                let dz = z[i] - z[j];
                let d2 = dx * dx + dy * dy + dz * dz;
                let mag = dt / (d2 * d2.sqrt());
                let mj = m[j];
                vx[i] -= dx * mj * mag;
                vy[i] -= dy * mj * mag;
                vz[i] -= dz * mj * mag;
                vx[j] += dx * mi * mag;
                vy[j] += dy * mi * mag;
                vz[j] += dz * mi * mag;
            }
        }
        for i in 0..nb {
            x[i] += dt * vx[i];
            y[i] += dt * vy[i];
            z[i] += dt * vz[i];
        }
    }
    let e1 = energy(&x, &y, &z, &vx, &vy, &vz, &m);
    let ok = (e0 + 0.169075164).abs() < 0.000000001 && (e1 + 0.169096567).abs() < 0.000000001;
    println!("n_body {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
