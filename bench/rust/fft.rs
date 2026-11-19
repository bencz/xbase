// bench/fft.prg in Rust: the fast Fourier transform (iterative radix 2, in place, on vectors of real and imaginary
// parts): 65,536 complex points from a linear congruential generator, transformed forward and back 10 times. The
// twiddle factors come from cos and sin of 2 pi / 2^k and complex multiplication; the check is the spectrum's sum of
// magnitudes and the round trip's largest error.
use std::hint::black_box;
use std::time::Instant;

// The transform of re + i im in place; sign -1 forward, 1 backward (unscaled).
fn fft(re: &mut [f64], im: &mut [f64], n: usize, cos_t: &[f64], sin_t: &[f64], sign: f64) {
    let mut j = 0usize;
    for i in 0..=n - 2 {
        if i < j {
            re.swap(i, j);
            im.swap(i, j);
        }
        let mut k = n >> 1;
        while k <= j {
            j -= k;
            k >>= 1;
        }
        j += k;
    }
    let mut len = 2;
    let mut st = 0;
    while len <= n {
        let half = len >> 1;
        let wr0 = cos_t[st];
        let wi0 = sign * sin_t[st];
        for i in (0..n).step_by(len) {
            let (mut wr, mut wi) = (1.0f64, 0.0f64);
            for jj in i..i + half {
                let k = jj + half;
                let xr = re[k] * wr - im[k] * wi;
                let xi = re[k] * wi + im[k] * wr;
                re[k] = re[jj] - xr;
                im[k] = im[jj] - xi;
                re[jj] += xr;
                im[jj] += xi;
                let tmp = wr * wr0 - wi * wi0;
                wi = wr * wi0 + wi * wr0;
                wr = tmp;
            }
        }
        len <<= 1;
        st += 1;
    }
}

fn main() {
    let t = Instant::now();
    let log_n = black_box(16usize);
    let n = 1usize << log_n;
    let mut seed: i64 = 2024;
    let mut re = vec![0.0f64; n];
    let mut im = vec![0.0f64; n];
    for i in 0..n {
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        re[i] = seed as f64 / 1073741824.0 - 1.0;
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        im[i] = seed as f64 / 1073741824.0 - 1.0;
    }
    let re0 = re.clone();
    let im0 = im.clone();
    // cos and sin of 2 pi / 2^k, for k from 1
    let mut cos_t = vec![0.0f64; log_n];
    let mut sin_t = vec![0.0f64; log_n];
    for i in 0..log_n {
        cos_t[i] = (2.0 * std::f64::consts::PI / (1u64 << (i + 1)) as f64).cos();
        sin_t[i] = (2.0 * std::f64::consts::PI / (1u64 << (i + 1)) as f64).sin();
    }
    let (mut spec, mut max_err) = (0.0f64, 0.0f64);
    for rep in 1..=10 {
        fft(&mut re, &mut im, n, &cos_t, &sin_t, -1.0);
        if rep == 1 {
            for i in 0..n {
                spec += re[i].abs() + im[i].abs();
            }
        }
        fft(&mut re, &mut im, n, &cos_t, &sin_t, 1.0);
        for i in 0..n {
            re[i] /= n as f64;
            im[i] /= n as f64;
        }
    }
    for i in 0..n {
        max_err = max_err.max((re[i] - re0[i]).abs().max((im[i] - im0[i]).abs()));
    }
    let ok = (spec / 15471706.35354992 - 1.0).abs() < 0.000000001 && max_err < 0.000000001;
    println!("fft {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
