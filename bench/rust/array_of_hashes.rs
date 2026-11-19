// bench/array_of_hashes.prg in Rust: records as HashMaps: 120,000 maps of four fields (a value enum: number or
// string) made from a fixed-seed generator, totals per group read by field name, a count of the records that pass a
// test, and the Vec sorted by a field with a comparison closure.
use std::collections::HashMap;
use std::time::Instant;

enum Value {
    N(i64),
    S(String),
}

type Record = HashMap<&'static str, Value>;

fn num(r: &Record, k: &str) -> i64 {
    match r[k] {
        Value::N(v) => v,
        Value::S(_) => panic!("not a number"),
    }
}

fn text<'a>(r: &'a Record, k: &str) -> &'a str {
    match &r[k] {
        Value::S(s) => s,
        Value::N(_) => panic!("not a string"),
    }
}

struct Rng(i64);
impl Rng {
    fn rand(&mut self, n: i64) -> i64 {
        self.0 = (self.0 * 16807) % 2147483647;
        self.0 % n
    }
}

fn main() {
    let t = Instant::now();
    let mut g = Rng(31337);
    let mut total = [0i64; 50];
    let mut a: Vec<Record> = Vec::with_capacity(120000);
    for i in 1..=120000i64 {
        let mut h: Record = HashMap::new();
        h.insert("id", Value::N(i));
        h.insert("name", Value::S(format!("N{:05}", g.rand(100000))));
        h.insert("score", Value::N(g.rand(1000)));
        h.insert("group", Value::N(g.rand(50) + 1));
        a.push(h);
    }
    let mut pass: i64 = 0;
    for r in &a {
        total[(num(r, "group") - 1) as usize] += num(r, "score");
        if num(r, "score") >= 500 && text(r, "name").starts_with("N1") {
            pass += 1;
        }
    }
    let cmp: &dyn Fn(&Record, &Record) -> bool = &|x, y| num(x, "score") < num(y, "score");
    a.sort_by(|x, y| {
        if cmp(x, y) {
            std::cmp::Ordering::Less
        } else if cmp(y, x) {
            std::cmp::Ordering::Greater
        } else {
            std::cmp::Ordering::Equal
        }
    });
    let sum: i64 = a.iter().step_by(1000).map(|r| num(r, "score")).sum();
    let ok = total[0] == 1239678 && total[49] == 1170281 && pass == 5950 && sum == 59429
        && num(&a[0], "score") == 0 && num(&a[119999], "score") == 999;
    println!("array_of_hashes {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
