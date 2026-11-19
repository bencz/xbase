// bench/inheritance_depth.prg in Rust: a line of 8 structs, each holding the one before (its parent): 4,000,000
// rounds, on an object of the last behind a trait (dynamic dispatch; the .prg makes it by a macro, its class unknown
// to the compiler), of a method each level overrides and that calls its parent's down to the first (8 calls, kept as
// calls: inline(never)), a method only the first has, and fields of the first, a middle and the last (read, and the
// first one assigned).
use std::hint::black_box;
use std::time::Instant;

trait Obj {
    fn depth(&self) -> i64;
    fn base(&self) -> i64;
    fn v0(&self) -> i64;
    fn set_v0(&mut self, v: i64);
    fn v4(&self) -> i64;
    fn v7(&self) -> i64;
}

struct L0 { v0: i64 }
impl L0 {
    #[inline(never)]
    fn depth(&self) -> i64 { 1 }
    fn base(&self) -> i64 { self.v0 + 100 }
}

// A level: its parent and the fields it declares (only the ones the loop reads; the .prg's v1, v2, v3, v5 and v6
// are never read and left out).
macro_rules! level {
    ($name:ident, $parent:ident, $add:expr $(, $field:ident)?) => {
        struct $name { sup: $parent $(, $field: i64)? }
        impl $name {
            #[inline(never)]
            fn depth(&self) -> i64 { black_box(&self.sup).depth() + $add }
        }
    };
}
level!(L1, L0, 2);
level!(L2, L1, 3);
level!(L3, L2, 4);
level!(L4, L3, 5, v4);
level!(L5, L4, 6);
level!(L6, L5, 7);
level!(L7, L6, 8, v7);

impl Obj for L7 {
    fn depth(&self) -> i64 { L7::depth(self) }
    fn base(&self) -> i64 { self.sup.sup.sup.sup.sup.sup.sup.base() }
    fn v0(&self) -> i64 { self.sup.sup.sup.sup.sup.sup.sup.v0 }
    fn set_v0(&mut self, v: i64) { self.sup.sup.sup.sup.sup.sup.sup.v0 = v; }
    fn v4(&self) -> i64 { self.sup.sup.sup.v4 }
    fn v7(&self) -> i64 { self.v7 }
}

fn make() -> Box<dyn Obj> {
    let l0 = L0 { v0: 0 };
    let l1 = L1 { sup: l0 };
    let l2 = L2 { sup: l1 };
    let l3 = L3 { sup: l2 };
    let l4 = L4 { sup: l3, v4: 4 };
    let l5 = L5 { sup: l4 };
    let l6 = L6 { sup: l5 };
    Box::new(L7 { sup: l6, v7: 7 })
}

fn main() {
    let t = Instant::now();
    let mut o: Box<dyn Obj> = black_box(make());
    let mut n: i64 = 0;
    for i in 1..=4000000i64 {
        n += o.depth() + o.base() + o.v0() + o.v4() + o.v7();
        o.set_v0(i % 3);
    }
    // o.depth() is 8 + 7 + ... + 1 = 36, o.base() is o.v0 + 100, o.v4 + o.v7 is 11; o.v0 is (i - 1) % 3, 0 at first
    let ok = n == 4000000 * (36 + 100 + 11) + 2 * 3999999;
    println!("inheritance_depth {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
