// bench/meteor_contest.prg in JavaScript: bit masks and a recursive search: every way to fill the meteor contest's
// board (5 x 10 hexagonal cells, odd rows shifted half a cell right) with its 10 pieces of 5 cells. The pieces are read
// from a solution; their placements (12 orientations, every position, duplicates dropped) are 64-bit masks (BigInt:
// JavaScript's bit operators on numbers work on 32 bits) listed by piece and lowest cell. The search fills the lowest
// empty cell first and drops a placement that leaves an empty cell walled in among the next 10. The board turned half
// a turn is the same board, with cell i at 49 - i: piece 3 takes only 6 of its orientations, and every solution found
// counts for itself and its turned image (the reversed string). Checked: 2098 solutions, and the smallest and largest
// as strings of 50 digits.
const { report } = require("./bench.js");
const COLS = 5, ROWS = 10, CELLS = 50, HALVED = 3, WINDOW = 10;
const BIT = [];
for (let c = 0; c < 64; c++) BIT.push(1n << BigInt(c)); // past the board too: a full board ends its scan at 50

let masks, cells, by, neighbors, placed, solutions = 0, best, worst;

// Offset coordinates (odd rows shifted right) of a cell (from 0) to axial ones [ q, r ], and back; -1 off the board.
function toAxial(cell) {
  const row = Math.trunc(cell / COLS);
  return [cell - row * COLS - (row - (row & 1)) / 2, row];
}

function toCell(q, r) {
  const col = q + (r - (r & 1)) / 2;
  if (r < 0 || r >= ROWS || col < 0 || col >= COLS) return -1;
  return r * COLS + col;
}

function makePlacements(shape) {
  const dq = [1, -1, 0, 0, 1, -1], dr = [0, 0, 1, -1, -1, 1];
  neighbors = new Array(CELLS).fill(0n);
  for (let c = 0; c < CELLS; c++) {
    const at = toAxial(c);
    for (let i = 0; i < 6; i++) {
      const x = toCell(at[0] + dq[i], at[1] + dr[i]);
      if (x >= 0) neighbors[c] |= BIT[x];
    }
  }
  masks = [];
  cells = [];
  by = [];
  for (let p = 0; p < 10; p++) {
    masks.push([]);
    cells.push([]);
    by.push(Array.from({ length: CELLS }, () => []));
    const pts = [];
    for (let c = 0; c < CELLS; c++) if (shape.charCodeAt(c) - 48 === p) pts.push(toAxial(c));
    const seen = new Set();
    for (let o = 0; o <= 11; o++) {
      for (let at = 0; at < CELLS; at++) {
        if (p === HALVED && o % 6 >= 3) break;
        const a = toAxial(at), place = new Array(5);
        let ok = true;
        for (let i = 0; i < 5; i++) {
          const x = toCell(pts[i][0] - pts[0][0] + a[0], pts[i][1] - pts[0][1] + a[1]);
          place[i] = x;
          if (x < 0) {
            ok = false;
            break;
          }
        }
        if (ok) {
          let mask = 0n, lowest = CELLS;
          for (let i = 0; i < 5; i++) {
            mask |= BIT[place[i]];
            lowest = Math.min(lowest, place[i]);
          }
          if (!seen.has(mask)) {
            seen.add(mask);
            masks[p].push(mask);
            cells[p].push(place);
            by[p][lowest].push(masks[p].length - 1);
          }
        }
      }
      // The next orientation: a turn of 60 degrees, and after the sixth a mirror image.
      for (let i = 0; i < 5; i++) {
        const q = pts[i][0];
        if (o === 5) {
          pts[i][0] = pts[i][1];
          pts[i][1] = q;
        } else {
          pts[i][0] = -pts[i][1];
          pts[i][1] = q + pts[i][1];
        }
      }
    }
  }
}

// Whether an empty cell among the WINDOW from cell has no empty neighbor.
function walledIn(board, cell) {
  for (let c = cell; c < Math.min(cell + WINDOW, CELLS); c++) {
    if ((board & BIT[c]) === 0n && (board & neighbors[c]) === neighbors[c]) return true;
  }
  return false;
}

function record(sol) {
  if (solutions === 0 || sol < best) best = sol;
  if (solutions === 0 || sol > worst) worst = sol;
  solutions++;
}

function solve(board, cell, depth) {
  while ((board & BIT[cell]) !== 0n) cell++;
  if (depth === 10) {
    let sol = " ".repeat(CELLS);
    for (let p = 0; p < 10; p++) {
      for (const i of cells[p][placed[p]]) sol = sol.slice(0, i) + String.fromCharCode(48 + p) + sol.slice(i + 1);
    }
    let turned = "";
    for (let i = CELLS - 1; i >= 0; i--) turned += sol[i];
    record(sol);
    record(turned);
    return;
  }
  for (let p = 0; p < 10; p++) {
    if (placed[p] < 0) {
      for (const k of by[p][cell]) {
        const next = board | masks[p][k];
        if ((board & masks[p][k]) === 0n && !walledIn(next, cell)) {
          placed[p] = k;
          solve(next, cell, depth + 1);
        }
      }
      placed[p] = -1;
    }
  }
}

const t = performance.now();
makePlacements("99998966856688568255777257472014220144031400311333");
placed = new Array(10).fill(-1);
solve(0n, 0, 0);
const ok = solutions === 2098 && best === "00001222012661126155865558633348893448934747977799" &&
  worst === "99998966856688568255777257472014220144031400311333";
report("meteor_contest", t, ok);
