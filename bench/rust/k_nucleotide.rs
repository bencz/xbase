// bench/k_nucleotide.prg in Rust: hashes keyed by strings: the Homo sapiens section of fasta for n = 100,000 (500,000
// bases, upper case, made with the benchmark's LCG before the clock starts), every k-mer of it counted in a HashMap
// for k = 1, 2, 3, 4, 6, 12 and 18 (the key a slice of the text at each position, not a new string), the 1- and
// 2-mers sorted by count with the library's sort, and the counts of five given sequences.
use std::collections::HashMap;
use std::time::Instant;

const IM: i64 = 139968;
const IA: i64 = 3877;
const IC: i64 = 29573;

// Every k-mer of s with its count.
fn frames(s: &str, k: usize) -> HashMap<&str, i64> {
    let mut h: HashMap<&str, i64> = HashMap::new();
    for i in 0..=s.len() - k {
        *h.entry(&s[i..i + k]).or_insert(0) += 1;
    }
    h
}

fn count_of(s: &str, seq: &str) -> i64 {
    *frames(s, seq.len()).get(seq).unwrap_or(&0)
}

// The k-mers by count, then by name: "A 151655 T 150452 ...".
fn sorted(s: &str, k: usize) -> String {
    let mut pairs: Vec<(&str, i64)> = frames(s, k).into_iter().collect();
    pairs.sort_by(|x, y| y.1.cmp(&x.1).then(x.0.cmp(y.0)));
    let mut out = String::new();
    for (key, n) in pairs {
        if !out.is_empty() {
            out.push(' ');
        }
        out += &format!("{} {}", key, n);
    }
    out
}

fn main() {
    let n = 100000;
    let mut seed: i64 = 42;
    let prob = [0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008];
    let mut cum = [0.0f64; 4];
    let mut acc = 0.0;
    for i in 0..4 {
        acc += prob[i];
        cum[i] = acc;
    }
    for _ in 0..3 * n {
        seed = (seed * IA + IC) % IM;
    }
    let mut s = String::new();
    for _ in 0..5 * n {
        seed = (seed * IA + IC) % IM;
        let r = seed as f64 / IM as f64;
        let mut j = 0;
        while j < 3 && r >= cum[j] {
            j += 1;
        }
        s.push(b"ACGT"[j] as char);
    }
    let t = Instant::now();
    let ok = sorted(&s, 1) == "A 151655 T 150452 C 99156 G 98737"
        && sorted(&s, 2)
            == "AA 45992 TA 45599 AT 45574 TT 45299 CA 30129 AC 30096 AG 29993 GA 29935 CT 29857 \
                TC 29832 GT 29722 TG 29721 GC 19617 CC 19611 CG 19559 GG 19463"
        && count_of(&s, "GGT") == 5871 && count_of(&s, "GGTA") == 1769 && count_of(&s, "GGTATT") == 185
        && count_of(&s, "GGTATTTTAATT") == 4 && count_of(&s, "GGTATTTTAATTTATAGT") == 4;
    println!("k_nucleotide {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
