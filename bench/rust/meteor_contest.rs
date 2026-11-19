// bench/meteor_contest.prg in Rust: bit masks and a recursive search: every way to fill the meteor contest's board
// (5 x 10 hexagonal cells, odd rows shifted half a cell right) with its 10 pieces of 5 cells. The pieces are read from
// a solution; their placements (12 orientations, every position, duplicates dropped through a HashSet) are 64-bit
// masks listed by piece and lowest cell. The search fills the lowest empty cell first and drops a placement that
// leaves an empty cell walled in among the next 10. Piece 3, which no half turn leaves the same, takes only 6 of its
// orientations, and every solution found counts for itself and its turned image (the reversed string). Checked: 2098
// solutions, and the smallest and largest as strings of 50 digits.
use std::collections::HashSet;
use std::time::Instant;

const COLS: i64 = 5;
const ROWS: i64 = 10;
const CELLS: usize = 50;
const HALVED: usize = 3;
const WINDOW: usize = 10;

// Offset coordinates (odd rows shifted right) of a cell (from 0) to axial ones (q, r), and back; -1 off the board.
fn to_axial(cell: i64) -> (i64, i64) {
    let row = cell / COLS;
    (cell - row * COLS - (row - (row & 1)) / 2, row)
}

fn to_cell(q: i64, r: i64) -> i64 {
    let col = q + (r - (r & 1)) / 2;
    if r < 0 || r >= ROWS || col < 0 || col >= COLS { -1 } else { r * COLS + col }
}

struct Search {
    mask: Vec<Vec<u64>>,
    cells: Vec<Vec<[usize; 5]>>,
    by: Vec<Vec<Vec<usize>>>,
    neighbors: Vec<u64>,
    placed: [usize; 10],
    solutions: i64,
    best: Vec<u8>,
    worst: Vec<u8>,
}

impl Search {
    fn new(shape: &[u8]) -> Search {
        let dq = [1, -1, 0, 0, 1, -1];
        let dr = [0, 0, 1, -1, -1, 1];
        let mut s = Search {
            mask: vec![Vec::new(); 10],
            cells: vec![Vec::new(); 10],
            by: vec![vec![Vec::new(); CELLS]; 10],
            neighbors: vec![0; CELLS],
            placed: [usize::MAX; 10],
            solutions: 0,
            best: Vec::new(),
            worst: Vec::new(),
        };
        for c in 0..CELLS {
            let at = to_axial(c as i64);
            for i in 0..6 {
                let x = to_cell(at.0 + dq[i], at.1 + dr[i]);
                if x >= 0 {
                    s.neighbors[c] |= 1 << x;
                }
            }
        }
        for p in 0..10 {
            let mut pts: Vec<(i64, i64)> = Vec::new();
            for c in 0..CELLS {
                if usize::from(shape[c] - b'0') == p {
                    pts.push(to_axial(c as i64));
                }
            }
            let mut seen: HashSet<u64> = HashSet::new();
            for o in 0..12 {
                for at_cell in 0..CELLS {
                    if p == HALVED && o % 6 >= 3 {
                        break;
                    }
                    let at = to_axial(at_cell as i64);
                    let mut place = [0usize; 5];
                    let mut ok = true;
                    for i in 0..5 {
                        let x = to_cell(pts[i].0 - pts[0].0 + at.0, pts[i].1 - pts[0].1 + at.1);
                        if x < 0 {
                            ok = false;
                            break;
                        }
                        place[i] = x as usize;
                    }
                    if ok {
                        let mut mask = 0u64;
                        let mut low = CELLS;
                        for &x in &place {
                            mask |= 1 << x;
                            low = low.min(x);
                        }
                        if seen.insert(mask) {
                            s.mask[p].push(mask);
                            s.cells[p].push(place);
                            s.by[p][low].push(s.mask[p].len() - 1);
                        }
                    }
                }
                // The next orientation: a turn of 60 degrees, and after the sixth a mirror image.
                for pt in pts.iter_mut() {
                    let q = pt.0;
                    if o == 5 {
                        *pt = (pt.1, q);
                    } else {
                        *pt = (-pt.1, q + pt.1);
                    }
                }
            }
        }
        s
    }

    // Whether an empty cell among the WINDOW from cell has no empty neighbor.
    fn walled_in(&self, board: u64, cell: usize) -> bool {
        for c in cell..(cell + WINDOW).min(CELLS) {
            if (board >> c) & 1 == 0 && board & self.neighbors[c] == self.neighbors[c] {
                return true;
            }
        }
        false
    }

    fn record(&mut self, sol: &[u8]) {
        if self.solutions == 0 || sol < &self.best[..] {
            self.best = sol.to_vec();
        }
        if self.solutions == 0 || sol > &self.worst[..] {
            self.worst = sol.to_vec();
        }
        self.solutions += 1;
    }

    fn solve(&mut self, board: u64, mut cell: usize, depth: usize) {
        while (board >> cell) & 1 == 1 {
            cell += 1;
        }
        if depth == 10 {
            let mut sol = vec![b' '; CELLS];
            for p in 0..10 {
                for &i in &self.cells[p][self.placed[p]] {
                    sol[i] = b'0' + p as u8;
                }
            }
            let turned: Vec<u8> = sol.iter().rev().copied().collect();
            self.record(&sol);
            self.record(&turned);
            return;
        }
        for p in 0..10 {
            if self.placed[p] == usize::MAX {
                for idx in 0..self.by[p][cell].len() {
                    let k = self.by[p][cell][idx];
                    let m = self.mask[p][k];
                    if board & m == 0 && !self.walled_in(board | m, cell) {
                        self.placed[p] = k;
                        self.solve(board | m, cell, depth + 1);
                    }
                }
                self.placed[p] = usize::MAX;
            }
        }
    }
}

fn main() {
    let t = Instant::now();
    let mut s = Search::new(std::hint::black_box(b"99998966856688568255777257472014220144031400311333"));
    s.solve(0, 0, 0);
    let ok = s.solutions == 2098 && s.best == b"00001222012661126155865558633348893448934747977799"
        && s.worst == b"99998966856688568255777257472014220144031400311333";
    println!("meteor_contest {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
