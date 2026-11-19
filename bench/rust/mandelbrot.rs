// bench/mandelbrot.prg in Rust: the Mandelbrot set as a bitmap, 1,600 x 1,600 points, at most 50 iterations each,
// one bit per point packed into bytes (a row padded to a whole byte), the bytes folded into a checksum.
use std::hint::black_box;
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let n: i64 = black_box(1600);
    let (mut cs, mut n_bytes) = (0i64, 0i64);
    for y in 0..n {
        let ci = 2.0 * y as f64 / n as f64 - 1.0;
        let (mut bits, mut n_bits) = (0i64, 0i64);
        for x in 0..n {
            let cr = 2.0 * x as f64 / n as f64 - 1.5;
            let (mut zr, mut zi, mut tr, mut ti) = (0.0f64, 0.0f64, 0.0f64, 0.0f64);
            let mut k = 0;
            while k < 50 && tr + ti <= 4.0 {
                zi = 2.0 * zr * zi + ci;
                zr = tr - ti + cr;
                tr = zr * zr;
                ti = zi * zi;
                k += 1;
            }
            bits = (bits << 1) | if tr + ti <= 4.0 { 1 } else { 0 };
            n_bits += 1;
            if n_bits == 8 || x == n - 1 {
                bits <<= 8 - n_bits;
                cs = (cs * 31 + bits) & 0x7FFFFFFF;
                n_bytes += 1;
                bits = 0;
                n_bits = 0;
            }
        }
    }
    let ok = cs == 366763831 && n_bytes == n * ((n + 7) / 8);
    println!("mandelbrot {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
