// bench/at_rat_strtran.prg in Rust: find and rfind (At() and RAt(), 1-based, 0 when absent) of 200 words in a
// 20,000-byte text of those words, and replace of each word by a longer one (StrTran()); 80 rounds.
use std::time::Instant;

fn rand(seed: &mut i64, n: i64) -> i64 {
    *seed = (*seed * 16807) % 2147483647;
    *seed % n
}

fn main() {
    let mut seed: i64 = 33;
    let mut words: Vec<String> = Vec::with_capacity(200);
    for _ in 0..200 {
        let mut w = String::new();
        // FOR j := 1 TO 3 + Rand( 6 ): the limit is evaluated again before every step, as xBase does.
        let mut j = 1;
        while j <= 3 + rand(&mut seed, 6) {
            w.push((97 + rand(&mut seed, 26)) as u8 as char);
            j += 1;
        }
        words.push(w);
    }
    let mut c = String::new();
    while c.len() < 20000 {
        let k = rand(&mut seed, 200) + 1;
        let k = rand(&mut seed, k) + 1;
        c += &words[(k - 1) as usize];
        c.push(' ');
    }
    let c = std::hint::black_box(c);
    let t = Instant::now();
    let mut n: i64 = 0;
    for _ in 0..80 {
        for w in &words {
            n += c.find(w.as_str()).map_or(0, |p| p as i64 + 1) + c.rfind(w.as_str()).map_or(0, |p| p as i64 + 1)
                + c.replace(w.as_str(), &format!("<{}>", w)).len() as i64;
        }
    }
    let ok = n == 620527280;
    println!("at_rat_strtran {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
