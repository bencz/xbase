// bench/memory_footprint.prg in Rust: the heap's live bytes, counted by a #[global_allocator] around the system
// allocator (Rust has no collector: no count of live objects, no free blocks kept, nothing to collect): a Vec of
// 1,000,000 integers, 200,000 boxed structs of three fields, a HashMap of 300,000 String keys, 600,000 records
// (number, 11-character String), each made by a function and measured before it returns. Each must have its count
// and stay within the .prg's budget of heap bytes per element, and once they are all dropped the live bytes must
// come back near where they started. The time is that of making them.
use std::alloc::{GlobalAlloc, Layout, System};
use std::collections::HashMap;
use std::sync::atomic::{AtomicIsize, Ordering};
use std::time::Instant;

struct Counting;
static LIVE: AtomicIsize = AtomicIsize::new(0);

unsafe impl GlobalAlloc for Counting {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        LIVE.fetch_add(layout.size() as isize, Ordering::Relaxed);
        unsafe { System.alloc(layout) }
    }
    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        LIVE.fetch_sub(layout.size() as isize, Ordering::Relaxed);
        unsafe { System.dealloc(ptr, layout) }
    }
    unsafe fn alloc_zeroed(&self, layout: Layout) -> *mut u8 {
        LIVE.fetch_add(layout.size() as isize, Ordering::Relaxed);
        unsafe { System.alloc_zeroed(layout) }
    }
    unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, new_size: usize) -> *mut u8 {
        LIVE.fetch_add(new_size as isize - layout.size() as isize, Ordering::Relaxed);
        unsafe { System.realloc(ptr, layout, new_size) }
    }
}

#[global_allocator]
static GLOBAL: Counting = Counting;

fn live() -> isize { LIVE.load(Ordering::Relaxed) }

// Whether a structure of len elements (n expected) is within bytes heap bytes per element.
fn within(len: usize, n: usize, bytes: isize, heap: isize) -> bool { len == n && live() - heap <= n as isize * bytes }

struct Point { x: i64, y: i64, tag: &'static str }

fn integers(heap: isize) -> bool {
    let mut a = vec![0i64; 1000000];
    for (i, v) in a.iter_mut().enumerate() {
        *v = (i as i64 + 1) * 3;
    }
    let a = std::hint::black_box(a);
    within(a.len(), 1000000, 24, heap)
}

fn objects(heap: isize) -> bool {
    let mut a: Vec<Box<Point>> = Vec::with_capacity(200000);
    for i in 1..=200000i64 {
        a.push(Box::new(Point { x: i, y: -i, tag: "point" }));
    }
    let a = std::hint::black_box(a);
    within(a.len(), 200000, 128, heap) && a[199999].x + a[199999].y == 0 && a[0].tag == "point"
}

fn keys(heap: isize) -> bool {
    let mut h: HashMap<String, i64> = HashMap::new();
    for i in 1..=300000i64 {
        h.insert(format!("k{:7}", i), i);
    }
    let h = std::hint::black_box(h);
    within(h.len(), 300000, 224, heap)
}

fn records(heap: isize) -> bool {
    let mut a: Vec<(i64, String)> = Vec::with_capacity(600000);
    for i in 1..=600000i64 {
        a.push((i, format!("item{:7}", i)));
    }
    let a = std::hint::black_box(a);
    within(a.len(), 600000, 224, heap) && a[599999].1.len() == 11
}

fn main() {
    let t = Instant::now();
    let heap = live();
    let mut ok = integers(heap) && objects(heap) && keys(heap) && records(heap);
    ok = ok && live() <= heap + 16 * 1024 * 1024;
    println!("memory_footprint {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
