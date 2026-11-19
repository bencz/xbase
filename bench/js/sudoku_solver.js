// bench/sudoku_solver.prg in JavaScript: recursion and arrays of booleans, five hard sudoku puzzles solved by
// backtracking (the first empty cell tries each digit its row, column and box allow), checked by a hash of the solved
// grids, 3 times. The arrays keep the .prg's 1-based indexes (element 0 unused).
const { report } = require("./bench.js");

// Marks digit v as used, or free, in the row, column and box of cell p (0 to 80).
function mark(p, v, on, row, col, box) {
  const r = Math.floor(p / 9), c = p - Math.floor(p / 9) * 9;
  row[r * 9 + v] = on;
  col[c * 9 + v] = on;
  box[(Math.floor(r / 3) * 3 + Math.floor(c / 3)) * 9 + v] = on;
}

// Fills the empty cells from cell p (0 to 80) on; true when the grid is solved.
function solve(p, grid, row, col, box) {
  while (p < 81 && grid[p + 1] !== 0) p++;
  if (p === 81) return true;
  const r = Math.floor(p / 9), c = p - r * 9, b = Math.floor(r / 3) * 3 + Math.floor(c / 3);
  for (let v = 1; v <= 9; v++) {
    if (row[r * 9 + v] || col[c * 9 + v] || box[b * 9 + v]) continue;
    grid[p + 1] = v;
    mark(p, v, true, row, col, box);
    if (solve(p + 1, grid, row, col, box)) return true;
    mark(p, v, false, row, col, box);
    grid[p + 1] = 0;
  }
  return false;
}

const t = performance.now();
const puzzles = [
  "8..........36......7..9.2...5...7.......457.....1...3...1....68..85...1..9....4..",
  "6.....8.3.4.7.................5.4.7.3..2.....1.6.......2.....5.....8.6......1....",
  "48.3............71.2.......7.5....6....2..8.............1.76...3.....4......5....",
  "..53.....8......2..7..1.5..4....53...1..7...6..32...8..6.5....9..4....3......97..",
  "1....7.9..3..2...8..96..5....53..9...1..8...26....4...3......1..4......7..7...3..",
];
let ok = true;
for (let rep = 1; rep <= 3; rep++) {
  let sum = 0;
  for (const puzzle of puzzles) {
    const grid = new Array(82).fill(null);
    const row = new Array(82).fill(false), col = new Array(82).fill(false), box = new Array(82).fill(false);
    for (let i = 1; i <= 81; i++) {
      const ch = puzzle.substring(i - 1, i);
      grid[i] = ch === "." ? 0 : ch.charCodeAt(0) - "0".charCodeAt(0);
      if (grid[i] !== 0) mark(i - 1, grid[i], true, row, col, box);
    }
    const solved = solve(0, grid, row, col, box);
    ok = ok && solved;
    for (let i = 1; i <= 81; i++) sum = (sum * 31 + grid[i]) & 0xfffffff;
  }
  ok = ok && sum === 252679381;
}
report("sudoku_solver", t, ok);
