// bench/gc.prg in Rust: 40,000 slots holding strings of 16 to 2,000 bytes and small records of them, replaced at
// random 3,000,000 times (each replaced value dropped), while 20,000 records that live to the end are kept beside them.
use std::time::Instant;

enum Slot { None, Str(String), Rec(String, i64, String) }

struct Rand { seed: u64 }
impl Rand {
    fn next(&mut self, n: i64) -> i64 {
        self.seed = (self.seed * 1103515245 + 12345) % 2147483648;
        (self.seed / 65536) as i64 % n
    }
}

fn main() {
    let t = Instant::now();
    let mut r = Rand { seed: 7 };
    let mut slots: Vec<Slot> = (0..40000).map(|_| Slot::None).collect();
    let mut kept: Vec<(i64, String, String)> = Vec::new();
    for i in 1..=3_000_000i64 {
        let n = r.next(40000) as usize;
        let x = if r.next(4) == 0 {
            let a = ((65 + i % 26) as u8 as char).to_string().repeat((16 + r.next(200)) as usize);
            Slot::Rec(a, i, " ".repeat(r.next(100) as usize))
        } else {
            Slot::Str(((97 + i % 26) as u8 as char).to_string().repeat((16 + r.next(2000)) as usize))
        };
        slots[n] = x;
        if i % 150 == 0 { kept.push((i, format!("{:10}", i), "k".repeat(64))); }
    }
    let mut check = 0usize;
    for s in &slots {
        match s {
            Slot::Str(x) => check += x.len(),
            Slot::Rec(a, i, c) => check += a.len() + c.len() + usize::from(*i > 0),
            Slot::None => {}
        }
    }
    println!("gc {} {}", t.elapsed().as_millis(), if kept.len() == 20000 && check > 0 { "ok" } else { "WRONG" });
}
