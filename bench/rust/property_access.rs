// bench/property_access.prg in Rust: 3,000,000 rounds of variable reads and assignments on an object behind a trait
// (its type unknown at the call sites, each access a dynamic call), a computed accessor and an assigning method, and
// the same again through a second reference (WITH OBJECT).
use std::hint::black_box;
use std::time::Instant;

trait Account {
    fn balance(&self) -> i64;
    fn set_balance(&mut self, v: i64);
    fn count(&self) -> i64;
    fn set_count(&mut self, v: i64);
    fn limit(&self) -> i64;
    fn last(&self) -> &str;
    fn set_last(&mut self, v: &'static str);
    fn doubled(&self) -> i64;
    fn set_limit(&mut self, x: i64);
}

struct TAccount {
    balance: i64,
    count: i64,
    limit: i64,
    last: &'static str,
}

impl Account for TAccount {
    fn balance(&self) -> i64 { self.balance }
    fn set_balance(&mut self, v: i64) { self.balance = v; }
    fn count(&self) -> i64 { self.count }
    fn set_count(&mut self, v: i64) { self.count = v; }
    fn limit(&self) -> i64 { self.limit }
    fn last(&self) -> &str { self.last }
    fn set_last(&mut self, v: &'static str) { self.last = v; }
    fn doubled(&self) -> i64 { self.balance * 2 - self.count - 2 }
    fn set_limit(&mut self, x: i64) { self.limit = x * 10; }
}

fn main() {
    let t = Instant::now();
    let mut o: Box<dyn Account> = black_box(Box::new(TAccount { balance: 0, count: 0, limit: 0, last: "" }));
    let mut n: i64 = 0;
    for i in 1..=3_000_000i64 {
        let v = o.balance() + 2;
        o.set_balance(v);
        let v = o.count() + 1;
        o.set_count(v);
        o.set_last("d");
        n += o.balance() - o.count() + o.doubled();
        o.set_limit(i % 10);
        {
            let w = black_box(&mut o);
            let v = w.balance() - 1;
            w.set_balance(v);
            n += w.limit() + w.last().len() as i64;
        }
    }
    // round i reads balance i + 1, count i, doubled i, then limit 10 * ( i % 10 ) and the length of last 1: it adds
    // i + 1 + 10 * ( i % 10 ) + 1
    let ok = n == 4500001500000 + 10 * 13500000 + 2 * 3000000 && o.balance() == 3000000;
    println!("property_access {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
