// bench/fannkuch_redux.prg in Rust: small integer arrays permuted and reversed in place: the pancake flips of every
// permutation of 9 elements (the checksum and the most flips), 4 times.
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let n: usize = std::hint::black_box(9);
    let mut ok = true;
    for _ in 0..4 {
        let mut perm = vec![0i64; n];
        let mut perm1: Vec<i64> = (0..n as i64).collect();
        let mut count = vec![0i64; n];
        let mut checksum: i64 = 0;
        let mut max_flips: i64 = 0;
        let mut perm_count: i64 = 0;
        let mut r = n;
        'perms: loop {
            while r != 1 {
                count[r - 1] = r as i64;
                r -= 1;
            }
            perm.copy_from_slice(&perm1);
            let mut flips: i64 = 0;
            let mut k = perm[0] as usize;
            while k != 0 {
                let (mut i, mut j) = (0, k);
                while i < j {
                    perm.swap(i, j);
                    i += 1;
                    j -= 1;
                }
                flips += 1;
                k = perm[0] as usize;
            }
            max_flips = max_flips.max(flips);
            checksum += if perm_count & 1 == 0 { flips } else { -flips };
            loop {
                if r == n {
                    break 'perms;
                }
                let perm0 = perm1[0];
                for i in 0..r {
                    perm1[i] = perm1[i + 1];
                }
                perm1[r] = perm0;
                count[r] -= 1;
                if count[r] > 0 {
                    break;
                }
                r += 1;
            }
            perm_count += 1;
        }
        ok = ok && checksum == 8629 && max_flips == 30;
    }
    println!("fannkuch_redux {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
