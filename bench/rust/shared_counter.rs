// bench/shared_counter.prg in Rust: a counter shared by 8 threads, contended: each thread adds 1 to an AtomicI64
// 250,000 times with fetch_add, then 50,000 times with a compare_exchange loop, then 50,000 times to a number under
// a Mutex. No increment may be lost, whatever the scheduling.
use std::sync::atomic::{AtomicI64, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Instant;

struct Shared {
    atomic: AtomicI64,
    cas: AtomicI64,
    boxed: Mutex<i64>,
}

fn work(s: &Shared) {
    for _ in 0..250000 {
        s.atomic.fetch_add(1, Ordering::SeqCst);
    }
    for _ in 0..50000 {
        loop {
            let n = s.cas.load(Ordering::SeqCst);
            if s.cas.compare_exchange(n, n + 1, Ordering::SeqCst, Ordering::SeqCst).is_ok() {
                break;
            }
        }
    }
    for _ in 0..50000 {
        *s.boxed.lock().unwrap() += 1;
    }
}

fn main() {
    let t = Instant::now();
    let s = Arc::new(Shared { atomic: AtomicI64::new(0), cas: AtomicI64::new(0), boxed: Mutex::new(0) });
    let threads: Vec<_> = (0..8)
        .map(|_| {
            let s = Arc::clone(&s);
            thread::spawn(move || work(&s))
        })
        .collect();
    for h in threads {
        h.join().unwrap();
    }
    let ok = s.atomic.load(Ordering::SeqCst) == 8 * 250000 && s.cas.load(Ordering::SeqCst) == 8 * 50000
        && *s.boxed.lock().unwrap() == 8 * 50000;
    println!("shared_counter {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
