// bench/indexacao_simulada.prg in Rust: an index kept in memory, as an order is: 150,000 entries (an 8-digit key and
// the record number, one String each) made from a fixed seed, then timed: sorted with sort, 100,000 seeks by binary
// search (the first entry not below the key; a hit when its key is the one sought), 3,000 entries inserted in place
// (binary search and Vec::insert), and the order checked.
use std::time::Instant;

fn rand(seed: &mut i64, n: i64) -> i64 {
    *seed = *seed * 16807 % 2147483647;
    *seed % n
}

// The first position whose entry is not below k, compared over k's length as xBase's < does with SET EXACT OFF
// (len when there is none); 0-based.
fn lower_bound(a: &[String], k: &str) -> usize {
    let (mut lo, mut hi) = (0usize, a.len());
    while lo < hi {
        let m = (lo + hi) / 2;
        let e = a[m].as_bytes();
        if &e[..k.len().min(e.len())] < k.as_bytes() {
            lo = m + 1;
        } else {
            hi = m;
        }
    }
    lo
}

fn val(s: &str) -> i64 { s.parse().unwrap_or(0) }

fn main() {
    let mut seed: i64 = 99991;
    let recs: i64 = 150000;
    let mut a: Vec<String> = (1..=recs).map(|i| format!("C{:08}{:06}", rand(&mut seed, 10000000), i)).collect();
    let t = Instant::now();
    a.sort();
    let (mut hits, mut sum, mut chk) = (0i64, 0i64, 0i64);
    for _ in 0..100000 {
        let k = format!("C{:08}", rand(&mut seed, 10000000));
        let p = lower_bound(&a, &k);
        if p < a.len() && a[p][..9] == k {
            hits += 1;
            sum += val(&a[p][a[p].len() - 6..]);
        }
    }
    for i in 1..=3000 {
        let k = format!("C{:08}{:06}", rand(&mut seed, 10000000), recs + i);
        let p = lower_bound(&a, &k);
        a.insert(p, k);
    }
    let mut ok = true;
    for i in 1..a.len() {
        ok = ok && a[i - 1] < a[i];
    }
    for e in a.iter().step_by(997) {
        chk = (chk * 31 + val(&e[e.len() - 6..])) % 1000000007;
    }
    ok = ok && hits == 1426 && sum == 107771969 && chk == 409559053 && a[0] == "C00000040027737"
        && a[a.len() - 1] == "C09999947069110";
    println!("indexacao_simulada {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
