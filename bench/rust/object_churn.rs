// bench/object_churn.prg in Rust: 50,000 slots holding reference-counted objects of three structs behind a trait
// (Rc<dyn Obj>) that point to other slots' objects; 700,000 times a slot's object is replaced by a new one, then the
// graph is walked. (A new object points only to objects made before it, so the references never form a cycle.)
use std::rc::Rc;
use std::time::Instant;

trait Obj {
    fn value(&self) -> i64;
    fn weight(&self) -> i64;
}

struct TLeaf {
    value: i64,
}

struct TPair {
    value: i64,
    left: Rc<dyn Obj>,
    right: Rc<dyn Obj>,
}

struct TList {
    value: i64,
    items: (Rc<dyn Obj>, i64, String),
}

impl Obj for TLeaf {
    fn value(&self) -> i64 { self.value }
    fn weight(&self) -> i64 { self.value }
}

impl Obj for TPair {
    fn value(&self) -> i64 { self.value }
    fn weight(&self) -> i64 { self.value + self.left.value() + self.right.value() }
}

impl Obj for TList {
    fn value(&self) -> i64 { self.value }
    fn weight(&self) -> i64 { self.value + self.items.0.value() + self.items.2.len() as i64 }
}

fn rand(seed: &mut i64, n: i64) -> i64 {
    *seed = (*seed * 16807) % 2147483647;
    *seed % n
}

fn main() {
    let t = Instant::now();
    let mut seed: i64 = 9;
    let mut slots: Vec<Rc<dyn Obj>> = (1..=50000i64).map(|i| Rc::new(TLeaf { value: i }) as Rc<dyn Obj>).collect();
    let len = slots.len() as i64;
    for i in 1..=700000i64 {
        let k = rand(&mut seed, len) as usize;
        let o: Rc<dyn Obj> = match rand(&mut seed, 3) {
            0 => Rc::new(TLeaf { value: i % 100 }),
            1 => {
                let l = Rc::clone(&slots[rand(&mut seed, len) as usize]);
                let r = Rc::clone(&slots[rand(&mut seed, len) as usize]);
                Rc::new(TPair { value: i % 100, left: l, right: r })
            }
            _ => {
                let o = Rc::clone(&slots[rand(&mut seed, len) as usize]);
                // Str( n ) of a remainder: xBase's % gives a number shown with SET DECIMALS (2) places, 13 columns.
                Rc::new(TList { value: i % 100, items: (o, i % 100, format!("{:13.2}", (i % 100) as f64)) })
            }
        };
        slots[k] = o;
    }
    let mut n: i64 = 0;
    for o in &slots {
        n += o.weight();
    }
    let ok = n == 5162688 && slots.len() == 50000;
    println!("object_churn {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
