// bench/julia_set.prg in Rust: the Julia set of c = -0.8 + 0.156i, escape times over a 1,600 x 1,200 grid of
// [-1.5, 1.5] x [-1, 1], at most 255 iterations per point, counted in a histogram of 256 bins folded into a checksum.
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let (w, h, max_iter): (i64, i64, i64) = std::hint::black_box((1600, 1200, 255));
    let (cr, ci) = std::hint::black_box((-0.8f64, 0.156f64));
    let mut hist = vec![0i64; (max_iter + 1) as usize];
    let mut total: i64 = 0;
    for y in 0..h {
        for x in 0..w {
            let mut zr = x as f64 * 3.0 / w as f64 - 1.5;
            let mut zi = y as f64 * 2.0 / h as f64 - 1.0;
            let mut k = 0;
            while k < max_iter {
                let zr2 = zr * zr;
                let zi2 = zi * zi;
                if zr2 + zi2 > 4.0 {
                    break;
                }
                zi = 2.0 * zr * zi + ci;
                zr = zr2 - zi2 + cr;
                k += 1;
            }
            hist[k as usize] += 1;
            total += k;
        }
    }
    let mut cs: i64 = 0;
    for v in &hist {
        cs = (cs * 31 + v) % 2147483648;
    }
    let ok = total == 69243973 && cs == 2105097618;
    println!("julia_set {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
