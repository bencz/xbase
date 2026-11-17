// bench/channels.prg in Rust: 4 producer threads send 250,000 numbers each through a bounded channel (capacity 64) to
// 2 consumer threads (std's channel has one receiver: the consumers share it under a mutex), then 1,000,000
// send/try_recv pairs on an unbounded channel from one thread.
use std::sync::mpsc::{channel, sync_channel};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let (tx, rx) = sync_channel::<i64>(64);
    let rx = Arc::new(Mutex::new(rx));
    let consumers: Vec<_> = (0..2)
        .map(|_| {
            let rx = Arc::clone(&rx);
            thread::spawn(move || {
                let mut s = 0i64;
                loop {
                    let x = rx.lock().unwrap().recv();
                    match x {
                        Ok(v) => s += v,
                        Err(_) => break s, // every sender gone: the channel closed
                    }
                }
            })
        })
        .collect();
    let producers: Vec<_> = (0..4)
        .map(|_| {
            let tx = tx.clone();
            thread::spawn(move || {
                for i in 1..=250000i64 {
                    tx.send(i).unwrap();
                }
            })
        })
        .collect();
    drop(tx);
    for p in producers {
        p.join().unwrap();
    }
    let mut s: i64 = consumers.into_iter().map(|c| c.join().unwrap()).sum();
    let (utx, urx) = channel::<i64>();
    for k in 1..=1000000i64 {
        utx.send(k).unwrap();
        s += urx.try_recv().unwrap() % 2;
    }
    let ok = s == 4 * 31250125000 + 500000;
    println!("channels {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
