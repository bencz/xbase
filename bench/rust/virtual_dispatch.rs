// bench/virtual_dispatch.prg in Rust: 1,000 shapes of 6 structs behind a trait object (Box<dyn Shape>) that override
// area() and perimeter() (made from a fixed seed, in random order), gone through 1,000 times: each shape's area() and
// perimeter() from one call site, and measure(), a default method of the trait that calls the overriding area().
use std::time::Instant;

trait Shape {
    fn area(&self) -> i64 { 0 }
    fn perimeter(&self) -> i64 { 0 }
    fn measure(&self) -> i64 { self.area() + 1 }
}

struct TCircle { x: i64 }
struct TSquare { x: i64 }
struct TRect { x: i64, y: i64 }
struct TTriangle { x: i64, y: i64 }
struct THexagon { x: i64 }
struct TEllipse { x: i64, y: i64 }

impl Shape for TCircle {
    fn area(&self) -> i64 { 3 * self.x * self.x }
    fn perimeter(&self) -> i64 { 6 * self.x }
}
impl Shape for TSquare {
    fn area(&self) -> i64 { self.x * self.x }
    fn perimeter(&self) -> i64 { 4 * self.x }
}
impl Shape for TRect {
    fn area(&self) -> i64 { self.x * self.y }
    fn perimeter(&self) -> i64 { 2 * (self.x + self.y) }
}
impl Shape for TTriangle {
    fn area(&self) -> i64 { self.x * self.y * 2 / 2 }
    fn perimeter(&self) -> i64 { 3 * self.x }
}
impl Shape for THexagon {
    fn area(&self) -> i64 { 6 * self.x * self.x }
    fn perimeter(&self) -> i64 { 6 * self.x }
}
impl Shape for TEllipse {
    fn area(&self) -> i64 { 3 * self.x * self.y }
    fn perimeter(&self) -> i64 { 3 * (self.x + self.y) }
}

fn rand(seed: &mut i64, n: i64) -> i64 {
    *seed = *seed * 16807 % 2147483647;
    *seed % n
}

type Maker = Box<dyn Fn(i64, i64) -> Box<dyn Shape>>;

fn main() {
    let mut seed: i64 = 6502;
    let classes: Vec<Maker> = vec![
        Box::new(|x, _| Box::new(TCircle { x })),
        Box::new(|x, _| Box::new(TSquare { x })),
        Box::new(|x, y| Box::new(TRect { x, y })),
        Box::new(|x, y| Box::new(TTriangle { x, y })),
        Box::new(|x, _| Box::new(THexagon { x })),
        Box::new(|x, y| Box::new(TEllipse { x, y })),
    ];
    let mut shapes: Vec<Box<dyn Shape>> = Vec::with_capacity(1000);
    for _ in 0..1000 {
        let c = rand(&mut seed, 6) as usize;
        let x = rand(&mut seed, 100) + 1;
        let y = rand(&mut seed, 100) + 1;
        shapes.push(classes[c](x, y));
    }
    let shapes = std::hint::black_box(shapes);
    let t = Instant::now();
    let (mut area, mut per, mut m) = (0i64, 0i64, 0i64);
    for _ in 0..1000 {
        for o in &shapes {
            area += o.area();
            per += o.perimeter();
            m += o.measure();
        }
    }
    let ok = area == 1000 * 7844067 && per == 1000 * 245962 && m == 1000 * (7844067 + 1000);
    println!("virtual_dispatch {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
