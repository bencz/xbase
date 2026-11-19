// bench/upper_lower_alltrim.prg in Rust: to_uppercase, to_lowercase and the trims of blanks (both sides, start, end)
// of 1,000 mixed-case strings padded with blanks on both sides, 1,500,000 times (the trims are slices, the case
// changes new Strings).
use std::time::Instant;

struct Rng(i64);
impl Rng {
    fn rand(&mut self, n: i64) -> i64 {
        self.0 = (self.0 * 16807) % 2147483647;
        self.0 % n
    }
}

fn main() {
    let mut g = Rng(8);
    let chars = b"aBcDeFgHiJkLmNoPqRsTuVwXyZ 0123456789";
    let mut a: Vec<String> = Vec::with_capacity(1000);
    for _ in 0..1000 {
        let mut w = String::new();
        // FOR j := 1 TO 5 + Rand( 30 ): the limit is evaluated again before every iteration.
        let mut j = 1;
        while j <= 5 + g.rand(30) {
            w.push(chars[g.rand(37) as usize] as char);
            j += 1;
        }
        let left = " ".repeat(g.rand(6) as usize);
        let right = " ".repeat(g.rand(6) as usize);
        a.push(left + "x" + &w + "Q" + &right);
    }
    let a = std::hint::black_box(a);
    let t = Instant::now();
    let mut n: i64 = 0;
    for i in 1..=1500000usize {
        let x = &a[i % 1000];
        let w = x.trim_matches(' ');
        let up = w.to_uppercase();
        let low = w.to_lowercase();
        n += w.len() as i64 + i64::from(up.as_bytes()[0]) + i64::from(*low.as_bytes().last().unwrap())
            + x.trim_end_matches(' ').len() as i64
            - x.trim_start_matches(' ').len() as i64;
    }
    let ok = n == 320344500;
    println!("upper_lower_alltrim {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
