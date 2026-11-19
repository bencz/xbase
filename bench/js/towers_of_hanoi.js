// bench/towers_of_hanoi.prg in JavaScript: recursion and nested arrays, 20 disks moved between three pegs (arrays
// with a height each), every move checked to put a smaller disk on a larger one, 4 times. The arrays keep the .prg's
// 1-based indexes (element 0 unused).
const { report } = require("./bench.js");

// Moves n disks from peg from to peg to through via; returns the number of moves.
function hanoi(n, from, to, via, peg, top, bad) {
  let moves = 0;
  if (n > 0) {
    moves = hanoi(n - 1, from, via, to, peg, top, bad);
    const d = peg[from][top[from]];
    top[from]--;
    if (top[to] > 0 && peg[to][top[to]] < d) bad[1]++;
    top[to]++;
    peg[to][top[to]] = d;
    moves += 1 + hanoi(n - 1, via, to, from, peg, top, bad);
  }
  return moves;
}

const t = performance.now();
const disks = 20;
let ok = true;
for (let rep = 1; rep <= 4; rep++) {
  const peg = [null];
  for (let p = 1; p <= 3; p++) peg.push(new Array(disks + 1).fill(null));
  for (let i = 1; i <= disks; i++) peg[1][i] = disks - i + 1;
  const top = [null, disks, 0, 0];
  const bad = [null, 0];
  const moves = hanoi(disks, 1, 3, 2, peg, top, bad);
  ok = ok && moves === (1 << disks) - 1 && bad[1] === 0 && top[3] === disks;
  for (let i = 1; i <= disks; i++) ok = ok && peg[3][i] === disks - i + 1;
}
report("towers_of_hanoi", t, ok);
