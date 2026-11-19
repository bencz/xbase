// bench/producer_consumer.prg in Rust: producers and consumers on threads over a bounded queue of 64 guarded by a
// Mutex and two Condvars (not full, not empty): 4 producers put 50,000 numbers each, 4 consumers take them until each
// finds the end mark (None) the program puts once the producers are joined.
use std::collections::VecDeque;
use std::sync::{Arc, Condvar, Mutex};
use std::thread;
use std::time::Instant;

struct Queue {
    q: Mutex<VecDeque<Option<i64>>>,
    not_full: Condvar,
    not_empty: Condvar,
}

fn put(s: &Queue, x: Option<i64>) {
    let mut q = s.q.lock().unwrap();
    while q.len() >= 64 {
        q = s.not_full.wait(q).unwrap();
    }
    q.push_back(x);
    s.not_empty.notify_one();
}

// Takes numbers until the end mark; returns (their sum, their count).
fn consume(s: &Queue) -> (i64, i64) {
    let (mut sum, mut count) = (0i64, 0i64);
    loop {
        let x = {
            let mut q = s.q.lock().unwrap();
            while q.is_empty() {
                q = s.not_empty.wait(q).unwrap();
            }
            let x = q.pop_front().unwrap();
            s.not_full.notify_one();
            x
        };
        match x {
            Some(v) => {
                sum += v;
                count += 1;
            }
            None => break,
        }
    }
    (sum, count)
}

fn main() {
    let t = Instant::now();
    let items: i64 = 50000;
    let s = Arc::new(Queue { q: Mutex::new(VecDeque::new()), not_full: Condvar::new(), not_empty: Condvar::new() });
    let consumers: Vec<_> = (0..4)
        .map(|_| {
            let s = Arc::clone(&s);
            thread::spawn(move || consume(&s))
        })
        .collect();
    let producers: Vec<_> = (1..=4i64)
        .map(|p| {
            let s = Arc::clone(&s);
            // Producer p puts p * 1000000 + i for i from 1 to items.
            thread::spawn(move || {
                for i in 1..=items {
                    put(&s, Some(p * 1000000 + i));
                }
            })
        })
        .collect();
    for p in producers {
        p.join().unwrap();
    }
    for _ in 0..4 {
        put(&s, None);
    }
    let (mut sum, mut count) = (0i64, 0i64);
    for c in consumers {
        let (a, b) = c.join().unwrap();
        sum += a;
        count += b;
    }
    let ok = count == 4 * items && sum == 10000000 * items + 2 * items * (items + 1);
    println!("producer_consumer {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
