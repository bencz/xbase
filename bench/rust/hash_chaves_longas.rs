// bench/hash_chaves_longas.prg in Rust: a HashMap with long String keys: 300,000 keys of 82 to 131 bytes that share
// a 66-byte prefix, inserted, then looked up through other Strings of the same text, and 300,000 missing keys of the
// same shape looked up.
use std::collections::HashMap;
use std::time::Instant;

fn main() {
    let mut seed: i64 = 99;
    let prefix = "https://example.com/catalog/products/category/electronics/item?id=";
    let mut keys = Vec::with_capacity(300000);
    let mut same = Vec::with_capacity(300000);
    let mut miss = Vec::with_capacity(300000);
    for i in 1..=300000i64 {
        seed = (seed * 16807) % 2147483647;
        let n = seed % 1000000;
        let tail = "x".repeat((i % 50) as usize);
        keys.push(format!("{}{:07}&v={:06}{}", prefix, i, n, tail));
        same.push(format!("{}{:07}&v={:06}{}", prefix, i, n, tail));
        miss.push(format!("{}{:07}&w={:06}{}", prefix, i, n, tail));
    }
    let t = Instant::now();
    let mut h: HashMap<String, i64> = HashMap::new();
    for (i, k) in keys.into_iter().enumerate() {
        h.insert(k, i as i64 + 1);
    }
    let mut sum: i64 = 0;
    for k in same.iter().rev() {
        sum += h[k];
    }
    let mut n_miss: i64 = 0;
    for k in &miss {
        if !h.contains_key(k) {
            n_miss += 1;
        }
    }
    let ok = h.len() == 300000 && sum == 45000150000 && n_miss == 300000;
    println!("hash_chaves_longas {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
