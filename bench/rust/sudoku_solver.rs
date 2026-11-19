// bench/sudoku_solver.prg in Rust: recursion and arrays of booleans: five hard sudoku puzzles solved by backtracking
// (the first empty cell tries each digit its row, column and box allow), checked by a hash of the solved grids, 3
// times.
use std::time::Instant;

struct Board {
    grid: Vec<i64>,
    row: Vec<bool>,
    col: Vec<bool>,
    bx: Vec<bool>,
}

impl Board {
    // Marks digit v as used, or free, in the row, column and box of cell p (0 to 80).
    fn mark(&mut self, p: usize, v: usize, on: bool) {
        let r = p / 9;
        let c = p - r * 9;
        self.row[r * 9 + v - 1] = on;
        self.col[c * 9 + v - 1] = on;
        self.bx[(r / 3 * 3 + c / 3) * 9 + v - 1] = on;
    }

    // Fills the empty cells from cell p (0 to 80) on; true when the grid is solved.
    fn solve(&mut self, mut p: usize) -> bool {
        while p < 81 && self.grid[p] != 0 {
            p += 1;
        }
        if p == 81 {
            return true;
        }
        let r = p / 9;
        let c = p - r * 9;
        let b = r / 3 * 3 + c / 3;
        for v in 1..=9usize {
            if self.row[r * 9 + v - 1] || self.col[c * 9 + v - 1] || self.bx[b * 9 + v - 1] {
                continue;
            }
            self.grid[p] = v as i64;
            self.mark(p, v, true);
            if self.solve(p + 1) {
                return true;
            }
            self.mark(p, v, false);
            self.grid[p] = 0;
        }
        false
    }
}

fn main() {
    let t = Instant::now();
    let puzzles = std::hint::black_box([
        "8..........36......7..9.2...5...7.......457.....1...3...1....68..85...1..9....4..",
        "6.....8.3.4.7.................5.4.7.3..2.....1.6.......2.....5.....8.6......1....",
        "48.3............71.2.......7.5....6....2..8.............1.76...3.....4......5....",
        "..53.....8......2..7..1.5..4....53...1..7...6..32...8..6.5....9..4....3......97..",
        "1....7.9..3..2...8..96..5....53..9...1..8...26....4...3......1..4......7..7...3..",
    ]);
    let mut ok = true;
    for _ in 0..3 {
        let mut sum: i64 = 0;
        for puzzle in &puzzles {
            let mut b = Board { grid: vec![0; 81], row: vec![false; 81], col: vec![false; 81], bx: vec![false; 81] };
            for (i, ch) in puzzle.bytes().enumerate() {
                b.grid[i] = if ch == b'.' { 0 } else { i64::from(ch - b'0') };
                if b.grid[i] != 0 {
                    b.mark(i, b.grid[i] as usize, true);
                }
            }
            let solved = b.solve(0);
            ok = ok && solved;
            for &v in &b.grid {
                sum = (sum * 31 + v) & 0xFFFFFFF;
            }
        }
        ok = ok && sum == 252679381;
    }
    println!("sudoku_solver {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
