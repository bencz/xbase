// bench/float.prg in Rust: Mandelbrot iterations over a 250 x 250 grid, at most 100 per point, 6 times.
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let size = std::hint::black_box(250i64);
    let max_iter = 100;
    let mut total: i64 = 0;
    for _ in 0..6 {
        for y in 0..size {
            let ci = y as f64 * 2.0 / size as f64 - 1.0;
            for x in 0..size {
                let cr = x as f64 * 3.0 / size as f64 - 2.0;
                let (mut zr, mut zi) = (0.0f64, 0.0f64);
                let mut k = 0;
                while k < max_iter {
                    let (zr2, zi2) = (zr * zr, zi * zi);
                    if zr2 + zi2 > 4.0 { break; }
                    zi = 2.0 * zr * zi + ci;
                    zr = zr2 - zi2 + cr;
                    k += 1;
                }
                total += k;
            }
        }
    }
    println!("float {} {}", t.elapsed().as_millis(), if total == 11391384 { "ok" } else { "WRONG" });
}
