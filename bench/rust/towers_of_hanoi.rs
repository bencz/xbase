// bench/towers_of_hanoi.prg in Rust: 20 disks moved between three pegs (arrays with a height each), every move
// checked to put a smaller disk on a larger one, 4 times.
use std::hint::black_box;
use std::time::Instant;

struct Pegs {
    peg: Vec<Vec<i64>>,
    top: [usize; 3],
    bad: i64,
}

// Moves n disks from peg from to peg to through via; returns the number of moves.
fn hanoi(n: i64, from: usize, to: usize, via: usize, p: &mut Pegs) -> i64 {
    let mut moves = 0;
    if n > 0 {
        moves = hanoi(n - 1, from, via, to, p);
        let d = p.peg[from][p.top[from] - 1];
        p.top[from] -= 1;
        if p.top[to] > 0 && p.peg[to][p.top[to] - 1] < d {
            p.bad += 1;
        }
        p.top[to] += 1;
        p.peg[to][p.top[to] - 1] = d;
        moves += 1 + hanoi(n - 1, via, to, from, p);
    }
    moves
}

fn main() {
    let t = Instant::now();
    let disks: usize = black_box(20);
    let mut ok = true;
    for _ in 0..4 {
        let mut p = Pegs { peg: vec![vec![0i64; disks]; 3], top: [disks, 0, 0], bad: 0 };
        for i in 0..disks {
            p.peg[0][i] = (disks - i) as i64;
        }
        let moves = hanoi(disks as i64, 0, 2, 1, &mut p);
        ok = ok && moves == (1i64 << disks) - 1 && p.bad == 0 && p.top[2] == disks;
        for i in 0..disks {
            ok = ok && p.peg[2][i] == (disks - i) as i64;
        }
    }
    println!("towers_of_hanoi {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
