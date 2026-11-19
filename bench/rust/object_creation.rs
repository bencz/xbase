// bench/object_creation.prg in Rust: creating objects: 600,000 boxed objects of a struct with five fields (numbers, a
// string and a vector of its own) made by new() and an init() that sets two of them, half of them of a "subclass"
// (the struct embedded in one with two more fields), behind a trait; one in 100 kept in a vector, the rest dropped.
// The due date is a day number (20261231 as a Julian day).
use std::hint::black_box;
use std::time::Instant;

struct TOrder {
    id: i64,
    customer: String,
    total: i64,
    status: &'static str,
    lines: Vec<i64>,
}

impl TOrder {
    fn blank() -> TOrder {
        TOrder { id: 0, customer: String::new(), total: 0, status: "new", lines: Vec::new() }
    }
    fn init(&mut self, id: i64, customer: String) {
        self.id = id;
        self.customer = customer;
    }
}

struct TBigOrder {
    base: TOrder,
    discount: i64,
    due: i64,
}

trait Order {
    fn order(&mut self) -> &mut TOrder;
    fn order_ref(&self) -> &TOrder;
}

impl Order for TOrder {
    fn order(&mut self) -> &mut TOrder { self }
    fn order_ref(&self) -> &TOrder { self }
}

impl Order for TBigOrder {
    fn order(&mut self) -> &mut TOrder { &mut self.base }
    fn order_ref(&self) -> &TOrder { &self.base }
}

#[inline(never)]
fn new_order(id: i64, customer: String) -> Box<dyn Order> {
    let mut o = Box::new(TOrder::blank());
    o.init(id, customer);
    o
}

#[inline(never)]
fn new_big_order(id: i64, customer: String) -> Box<TBigOrder> {
    let mut o = Box::new(TBigOrder { base: TOrder::blank(), discount: 0, due: 2_461_406 });
    o.base.init(id, customer);
    o
}

fn main() {
    let t = Instant::now();
    let mut keep: Vec<Box<dyn Order>> = Vec::new();
    for i in 1..=600000i64 {
        let mut o: Box<dyn Order> = if i % 2 == 0 {
            new_order(i, format!("c{:03}", i % 1000))
        } else {
            let mut b = new_big_order(i, String::from("c000"));
            b.discount = 5;
            black_box(b.due);
            b
        };
        o.order().lines.push(i % 7);
        let o = black_box(o);
        if i % 100 == 0 {
            keep.push(o);
        }
    }
    let mut n: i64 = 0;
    for o in &keep {
        let r = o.order_ref();
        n += r.id + r.total + r.lines.len() as i64 + r.lines[0] + r.customer.len() as i64;
        black_box(r.status);
    }
    let ok = keep.len() == 6000 && n == 1800300000 + 6000 * 5 + 17999;
    println!("object_creation {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
