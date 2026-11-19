// bench/random_massivo.prg in Rust: 30,000,000 rounds of four random-number generators in integer arithmetic (a
// 31-bit linear congruential generator, a 64-bit xorshift with masks for the logical right shift, two 16-bit
// multiply-with-carry streams and a 32-bit xorshift), their sums, a histogram of 256 buckets of their top bytes and
// the sum of the congruential numbers scaled to [0, 1).
use std::hint::black_box;
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let n: i64 = black_box(30_000_000);
    let mut hist = [0i64; 257];
    let (mut lcg, mut xs64, mut z, mut w, mut xs32) = (12345i64, 88172645463325252i64, 362436069i64, 521288629i64,
        2463534242i64);
    let (mut s1, mut s2, mut s3, mut s4, mut fsum, mut check) = (0i64, 0i64, 0i64, 0i64, 0.0f64, 0i64);
    for _ in 0..n {
        lcg = (lcg.wrapping_mul(1103515245) + 12345) & 0x7FFFFFFF;
        s1 += lcg;
        hist[((lcg >> 23) + 1) as usize] += 1;
        fsum += lcg as f64 / 2147483648.0;

        xs64 ^= xs64 << 13;
        xs64 ^= (xs64 >> 7) & 0x01FFFFFFFFFFFFFF;
        xs64 ^= xs64 << 17;
        let r = (xs64 >> 32) & 0xFFFFFFFF;
        s2 += r;
        hist[((r >> 24) + 1) as usize] += 1;

        z = 36969 * (z & 0xFFFF) + (z >> 16);
        w = 18000 * (w & 0xFFFF) + (w >> 16);
        let r = ((z << 16) + w) & 0xFFFFFFFF;
        s3 += r;
        hist[((r >> 24) + 1) as usize] += 1;

        xs32 ^= (xs32 << 13) & 0xFFFFFFFF;
        xs32 ^= xs32 >> 17;
        xs32 ^= (xs32 << 5) & 0xFFFFFFFF;
        s4 += xs32;
        hist[((xs32 >> 24) + 1) as usize] += 1;
    }
    for i in 1..=256 {
        check += hist[i] * i as i64;
    }
    let ok = s1 == 32217797789519680 && s2 == 64428429046975605 && s3 == 64423232821249815
        && s4 == 64428119329011698 && check == 15421035017 && (fsum - 15002581.187856009).abs() < 0.000001;
    println!("random_massivo {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
