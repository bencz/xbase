// bench/jaro_winkler.prg in Rust: string similarity: the Jaro-Winkler similarity of 300,000 pairs of 6 to 14 letters
// (the second one the first with random changes, made with replace_range for Stuff()), the sum of the doubles checked
// to 6 decimals.
use std::time::Instant;

fn jaro_winkler(s1: &str, s2: &str) -> f64 {
    let (s1, s2) = (s1.as_bytes(), s2.as_bytes());
    let (l1, l2) = (s1.len(), s2.len());
    if l1 == 0 || l2 == 0 {
        return if l1 == l2 { 1.0 } else { 0.0 };
    }
    let range = (l1.max(l2) / 2).saturating_sub(1);
    let mut a1 = vec![false; l1];
    let mut a2 = vec![false; l2];
    let mut m: i64 = 0;
    for i in 0..l1 {
        for j in i.saturating_sub(range)..l2.min(i + range + 1) {
            if !a2[j] && s1[i] == s2[j] {
                a1[i] = true;
                a2[j] = true;
                m += 1;
                break;
            }
        }
    }
    if m == 0 {
        return 0.0;
    }
    let mut k = 0;
    let mut trans: i64 = 0;
    for i in 0..l1 {
        if a1[i] {
            while !a2[k] {
                k += 1;
            }
            if s1[i] != s2[k] {
                trans += 1;
            }
            k += 1;
        }
    }
    let m = m as f64;
    let jaro = (m / l1 as f64 + m / l2 as f64 + (m - trans as f64 / 2.0) / m) / 3.0;
    let mut prefix = 0;
    while prefix < 4 && prefix < l1.min(l2) && s1[prefix] == s2[prefix] {
        prefix += 1;
    }
    jaro + prefix as f64 * 0.1 * (1.0 - jaro)
}

struct Rng(i64);
impl Rng {
    fn rand(&mut self, n: i64) -> i64 {
        self.0 = (self.0 * 16807) % 2147483647;
        self.0 % n
    }
    fn letter(&mut self) -> char { (65 + self.rand(6) as u8) as char }
}

fn pair(g: &mut Rng, len: i64) -> (String, String) {
    let mut s = String::new();
    for _ in 0..len {
        s.push(g.letter());
    }
    let mut u = s.clone();
    for _ in 0..3 {
        let k = (g.rand(u.len() as i64) + 1) as usize;
        let del = g.rand(2) as usize;
        let c = g.letter();
        u.replace_range(k - 1..k - 1 + del, c.encode_utf8(&mut [0; 4]));
    }
    (s, u)
}

fn main() {
    let mut g = Rng(271);
    let mut pairs = Vec::with_capacity(300000);
    for _ in 0..300000 {
        let len = 6 + g.rand(9);
        pairs.push(pair(&mut g, len));
    }
    let pairs = std::hint::black_box(pairs);
    let t = Instant::now();
    let mut sum = 0.0f64;
    for p in &pairs {
        sum += jaro_winkler(&p.0, &p.1);
    }
    let ok = (sum - 260943.0356007).abs() < 0.000001
        && (jaro_winkler("MARTHA", "MARHTA") - 0.961111111).abs() < 0.000000001;
    println!("jaro_winkler {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
