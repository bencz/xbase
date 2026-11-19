// bench/chameneos_redux.prg in Rust: threads and channels: creatures on threads of their own ask for meetings on one
// channel; a broker pairs them two by two and answers each on its own channel with the other's name and color, after
// which both change color; 30,000 meetings with 3 creatures, then 30,000 with 10. Checked: the color table, and per
// run 2N meetings counted, none of a creature with itself.
use std::sync::mpsc::{channel, Sender};
use std::thread;
use std::time::Instant;

const BLUE: i64 = 0;
const RED: i64 = 1;
const YELLOW: i64 = 2;

fn complement(a: i64, b: i64) -> i64 {
    if a == b { a } else { 3 - a - b }
}

// A request: the creature's name, its color and its reply channel; the answer is the other's (name, color), or None
// once no meeting is left.
type Request = (i64, i64, Sender<Option<(i64, i64)>>);

// Whether n meetings of creatures of these colors count 2n meetings, none with itself. The caller is the broker.
fn run(n: i64, colors: &[i64]) -> bool {
    let (meet_tx, meet_rx) = channel::<Request>();
    let threads: Vec<_> = colors
        .iter()
        .enumerate()
        .map(|(i, &c)| {
            let meet = meet_tx.clone();
            let id = i as i64 + 1;
            thread::spawn(move || {
                let (reply_tx, reply_rx) = channel::<Option<(i64, i64)>>();
                let (mut color, mut met, mut same) = (c, 0i64, 0i64);
                loop {
                    meet.send((id, color, reply_tx.clone())).unwrap();
                    match reply_rx.recv().unwrap() {
                        None => break,
                        Some((other_id, other_color)) => {
                            color = complement(color, other_color);
                            met += 1;
                            if other_id == id {
                                same += 1;
                            }
                        }
                    }
                }
                (met, same)
            })
        })
        .collect();
    let (mut left, mut done) = (n, 0usize);
    let mut first: Option<Request> = None;
    while done < colors.len() {
        let req = meet_rx.recv().unwrap();
        if left == 0 {
            req.2.send(None).unwrap();
            done += 1;
        } else if let Some(f) = first.take() {
            f.2.send(Some((req.0, req.1))).unwrap();
            req.2.send(Some((f.0, f.1))).unwrap();
            left -= 1;
        } else {
            first = Some(req);
        }
    }
    let (mut met, mut same) = (0i64, 0i64);
    for h in threads {
        let (m, s) = h.join().unwrap();
        met += m;
        same += s;
    }
    met == 2 * n && same == 0
}

fn main() {
    let t = Instant::now();
    let mut table = String::new();
    for a in 0..=2 {
        for b in 0..=2 {
            table += &complement(a, b).to_string();
        }
    }
    let mut ok = table == "021210102";
    ok = run(30000, &[BLUE, RED, YELLOW]) && ok;
    ok = run(30000, &[BLUE, RED, YELLOW, RED, YELLOW, BLUE, RED, YELLOW, RED, BLUE]) && ok;
    println!("chameneos_redux {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
