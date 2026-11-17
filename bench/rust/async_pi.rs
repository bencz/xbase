// bench/async_pi.prg in Rust: the Basel sum over 32 threads of 50,000,000 terms each, joined in order; pi is
// Sqrt( 6 * sum ).
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let terms: i64 = 50_000_000;
    let handles: Vec<_> = (0..32i64)
        .map(|i| std::thread::spawn(move || {
            let mut s = 0.0f64;
            for n in i * terms + 1..=(i + 1) * terms { let inv = 1.0 / n as f64; s += inv * inv; }
            s
        }))
        .collect();
    let total: f64 = handles.into_iter().map(|h| h.join().unwrap()).sum();
    let pi = (total * 6.0).sqrt();
    let ok = (std::f64::consts::PI - pi).abs() < 1e-9;
    println!("async_pi {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
