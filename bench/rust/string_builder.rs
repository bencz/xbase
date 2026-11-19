// bench/string_builder.prg in Rust: 3 rounds of 400,000 table rows of about 37 bytes appended to one String (about
// 15 MB each), its length and every 1,000th byte checked.
use std::hint::black_box;
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let names = black_box(["ant", "bee", "cat", "dog", "eel", "fox", "gnu", "hen", "ibis", "jay"]);
    let mut n: i64 = 0;
    for r in 1..=3i64 {
        let mut c = String::new();
        for i in 1..=400_000i64 {
            c += &("<tr><td>".to_string() + names[(i % 10) as usize] + "</td><td>" + &(i + r).to_string()
                + "</td></tr>" + "\n");
        }
        n += c.len() as i64;
        n += c.as_bytes().iter().step_by(1000).map(|&b| i64::from(b)).sum::<i64>();
    }
    let ok = n == 47588053;
    println!("string_builder {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
