// bench/long_short_mix.prg in Rust: long-lived and short-lived values together: 100,000 records that live to the end,
// 2,500,000 temporary records (a number and a string, each a new Rc), one in 8 of them stored into a long-lived record
// (an old value pointing to a new one), one record in 64 steps replaced by a new one.
use std::hint::black_box;
use std::rc::Rc;
use std::time::Instant;

struct Rec {
    n: i64,
    s: String,
    link: Option<Rc<Rec>>,
}

fn main() {
    let t = Instant::now();
    let mut seed: i64 = 17;
    let mut rand = |n: i64| {
        seed = (seed * 16807) % 2147483647;
        seed % n
    };
    let mut long: Vec<Rec> = (1..=100000i64).map(|i| Rec { n: i, s: format!("{:10}", i), link: None }).collect();
    let mut n: i64 = 0;
    for i in 1..=2500000i64 {
        let x = black_box(Rc::new(Rec {
            n: i % 1000,
            s: ((b'a' + (i % 26) as u8) as char).to_string().repeat((i % 40 + 1) as usize),
            link: None,
        }));
        n += x.s.len() as i64;
        if i % 8 == 0 {
            let k = rand(long.len() as i64) as usize;
            long[k].link = Some(Rc::clone(&x));
        }
        if i % 64 == 0 {
            let k = rand(long.len() as i64) as usize;
            long[k] = Rec { n: long[k].n + 1, s: format!("{:10}", i), link: long[k].link.take() };
        }
    }
    for r in &long {
        n += r.n + r.s.len() as i64;
        if let Some(l) = &r.link {
            n += l.n;
        }
    }
    let ok = n == 5099739142;
    println!("long_short_mix {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
