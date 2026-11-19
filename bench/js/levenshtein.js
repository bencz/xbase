// bench/levenshtein.prg in JavaScript: the Levenshtein distance of 10,000 pairs of strings of 30 to 60 letters (the
// second one the first with random edits), by dynamic programming over two rows, the letters read one at a time.
const { report } = require("./bench.js");

let seed = 1001;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};

function distance(s, u) {
  const m = s.length, n = u.length;
  let prev = new Array(n + 1), cur = new Array(n + 1);
  for (let j = 0; j <= n; j++) prev[j] = j;
  for (let i = 1; i <= m; i++) {
    cur[0] = i;
    const c = s[i - 1];
    for (let j = 1; j <= n; j++) {
      cur[j] = Math.min(Math.min(prev[j], cur[j - 1]) + 1, prev[j - 1] + (c === u[j - 1] ? 0 : 1));
    }
    const swap = prev;
    prev = cur;
    cur = swap;
  }
  return prev[n];
}

function pair(len) {
  let s = "", u = "";
  for (let i = 0; i < len; i++) s += String.fromCharCode(97 + rand(4));
  for (let i = 0; i < len; i++) {
    const k = rand(10);
    if (k === 0) u += String.fromCharCode(97 + rand(4));
    else if (k === 1) u += s[i] + String.fromCharCode(97 + rand(4));
    else if (k !== 2) u += s[i];
  }
  return [s, u];
}

const pairs = new Array(10000);
for (let i = 0; i < pairs.length; i++) pairs[i] = pair(30 + rand(31));
const t = performance.now();
let n = 0;
for (const p of pairs) n += distance(p[0], p[1]);
report("levenshtein", t, n === 108309 && distance("kitten", "sitting") === 3);
