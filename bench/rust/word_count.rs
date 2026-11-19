// bench/word_count.prg in Rust: word frequencies: 200,000 lines of 1 to 30 words (about 1,300,000 words, of a
// 5,000-word vocabulary in mixed case and a skewed distribution) cut at blanks, lower-cased and counted in a HashMap;
// the most frequent word found. The loops' limits are drawn again at every step, as the .prg's FOR evaluates its TO
// expression. The rest of a line after a word is a slice of it, not a new string.
use std::collections::HashMap;
use std::time::Instant;

fn main() {
    let mut seed: i64 = 4242;
    let mut rand = |n: i64| {
        seed = (seed * 16807) % 2147483647;
        seed % n
    };
    let letters = b"eEtaAoinNsrhldcumfpgwybvkxjqz";
    let mut vocab: Vec<String> = Vec::with_capacity(5000);
    for _ in 0..5000 {
        let mut w = String::new();
        let mut j = 1;
        while j <= 2 + rand(9) {
            j += 1;
            let k = rand(29) + 1;
            w.push(letters[rand(k) as usize] as char);
        }
        vocab.push(w);
    }
    let mut lines: Vec<String> = Vec::with_capacity(200000);
    for _ in 0..200000 {
        let mut c = String::new();
        let mut j = 1;
        while j <= 1 + rand(30) {
            j += 1;
            let k = rand(5000) + 1;
            c += &vocab[rand(k) as usize];
            c.push(' ');
        }
        lines.push(c);
    }
    let t = Instant::now();
    let mut h: HashMap<String, i64> = HashMap::new();
    let mut words: i64 = 0;
    for line in &lines {
        let mut c: &str = line;
        while let Some(n) = c.find(' ') {
            let w = c[..n].to_ascii_lowercase();
            c = &c[n + 1..];
            words += 1;
            *h.entry(w).or_insert(0) += 1;
        }
    }
    let (mut check, mut top) = (0i64, 0i64);
    for n in h.values() {
        check += n * n;
        if *n > top {
            top = *n;
        }
    }
    let mut c_top = String::new();
    for w in &vocab {
        let w = w.to_ascii_lowercase();
        if h[&w] == top {
            c_top = w;
            break;
        }
    }
    let ok = words == 1312183 && h.len() == 4328 && top == 7336 && c_top == "ee" && check == 971695061;
    println!("word_count {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
