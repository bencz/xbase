// bench/n_queens.prg in Rust: the solutions of the 12 queens problem (backtracking over vectors of taken columns and
// diagonals), 3 times.
use std::hint::black_box;
use std::time::Instant;

fn place(row: usize, n: usize, col: &mut [bool], d1: &mut [bool], d2: &mut [bool]) -> i64 {
    if row > n {
        return 1;
    }
    let mut count = 0;
    for c in 1..=n {
        if !col[c - 1] && !d1[row + c - 2] && !d2[row + n - c - 1] {
            col[c - 1] = true;
            d1[row + c - 2] = true;
            d2[row + n - c - 1] = true;
            count += place(row + 1, n, col, d1, d2);
            col[c - 1] = false;
            d1[row + c - 2] = false;
            d2[row + n - c - 1] = false;
        }
    }
    count
}

fn main() {
    let t = Instant::now();
    let n = black_box(12usize);
    let mut ok = true;
    for _ in 0..3 {
        ok = ok && place(1, n, &mut vec![false; n], &mut vec![false; 2 * n - 1], &mut vec![false; 2 * n - 1]) == 14200;
    }
    println!("n_queens {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
