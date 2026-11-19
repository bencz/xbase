// bench/pipeline_tasks.prg in Rust: a pipeline of four stages on threads joined by bounded channels (sync_channel of
// 256): a source sends 200,000 records as text ("id;value", the values from a fixed seed), a parser splits them into
// (id, value), a stage maps each value to its square modulo 1,000,003 and keeps the even ones, and a sink sums them.
// Each stage closes its output (drops its sender) when its input ends. The sums must be the same whatever the
// scheduling.
use std::sync::mpsc::sync_channel;
use std::thread;
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let (tx1, rx1) = sync_channel::<String>(256);
    let (tx2, rx2) = sync_channel::<(i64, i64)>(256);
    let (tx3, rx3) = sync_channel::<(i64, i64)>(256);
    let source = thread::spawn(move || {
        let mut x: i64 = 4321;
        for i in 1..=200000i64 {
            x = x * 16807 % 2147483647;
            tx1.send(format!("{};{}", i, x % 100000)).unwrap();
        }
    });
    let parser = thread::spawn(move || {
        for c in rx1 {
            let n = c.find(';').unwrap();
            tx2.send((c[..n].parse().unwrap(), c[n + 1..].parse().unwrap())).unwrap();
        }
    });
    let square = thread::spawn(move || {
        for (id, v) in rx2 {
            let y = v * v % 1000003;
            if y % 2 == 0 {
                tx3.send((id, y)).unwrap();
            }
        }
    });
    let sink = thread::spawn(move || {
        let (mut sum, mut count, mut ids) = (0i64, 0i64, 0i64);
        for (id, y) in rx3 {
            sum += y;
            count += 1;
            ids += id;
        }
        (sum, count, ids)
    });
    let (sum, count, ids) = sink.join().unwrap();
    source.join().unwrap();
    parser.join().unwrap();
    square.join().unwrap();
    let ok = sum == 49893566082 && count == 100073 && ids == 10032092994;
    println!("pipeline_tasks {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
