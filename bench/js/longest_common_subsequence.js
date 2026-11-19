// bench/longest_common_subsequence.prg in JavaScript: the length of the longest common subsequence of 600 pairs of 200
// to 300 letters from a 4-letter alphabet (the second related to the first), by dynamic programming over two rows, the
// letters split into arrays first.
const { report } = require("./bench.js");

let seed = 31;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};

function lcs(s, u) {
  const as = s.split(""), au = u.split(""), n = au.length;
  let prev = new Array(n + 1).fill(0), cur = new Array(n + 1).fill(0);
  for (let i = 0; i < as.length; i++) {
    const c = as[i];
    for (let j = 0; j < n; j++) cur[j + 1] = c === au[j] ? prev[j] + 1 : Math.max(prev[j + 1], cur[j]);
    const swap = prev;
    prev = cur;
    cur = swap;
  }
  return prev[n];
}

function pair(len) {
  let s = "", u = "";
  for (let i = 0; i < len; i++) s += String.fromCharCode(65 + rand(4));
  for (let i = 0; i < len; i++) {
    if (rand(4) !== 0) u += s[i];
    if (rand(5) === 0) u += String.fromCharCode(65 + rand(4));
  }
  return [s, u];
}

const pairs = new Array(600);
for (let i = 0; i < pairs.length; i++) pairs[i] = pair(200 + rand(101));
const t = performance.now();
let n = 0;
for (const p of pairs) n += lcs(p[0], p[1]);
report("longest_common_subsequence", t, n === 118188 && lcs("ABCBDAB", "BDCABA") === 4);
