// bench/k_nucleotide.prg in JavaScript: the Homo sapiens section of fasta for n = 100,000 (500,000 bases, upper case,
// made with the benchmark's LCG before the clock starts), every k-mer of it counted in a Map for k = 1, 2, 3, 4, 6, 12
// and 18 (a substring per position as the key), the 1- and 2-mers sorted by count, and the counts of five given
// sequences.
const { report } = require("./bench.js");
const IM = 139968, IA = 3877, IC = 29573;

// Every k-mer of s with its count.
function frames(s, k) {
  const h = new Map();
  for (let i = 0; i <= s.length - k; i++) {
    const key = s.substring(i, i + k);
    const c = h.get(key);
    h.set(key, c === undefined ? 1 : c + 1);
  }
  return h;
}

function countOf(s, seq) {
  const c = frames(s, seq.length).get(seq);
  return c === undefined ? 0 : c;
}

// The k-mers by count, then by name: "A 151655 T 150452 ...".
function sorted(s, k) {
  const pairs = [...frames(s, k)];
  pairs.sort((x, y) => (x[1] !== y[1] ? y[1] - x[1] : x[0] < y[0] ? -1 : x[0] > y[0] ? 1 : 0));
  return pairs.map((p) => `${p[0]} ${p[1]}`).join(" ");
}

const n = 100000, prob = [0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008], cum = [];
let seed = 42, acc = 0, s = "";
for (const p of prob) cum.push(acc += p);
for (let i = 1; i <= 3 * n; i++) seed = (seed * IA + IC) % IM;
for (let i = 1; i <= 5 * n; i++) {
  seed = (seed * IA + IC) % IM;
  const r = seed / IM;
  let j = 0;
  while (j < 3 && r >= cum[j]) j++;
  s += "ACGT"[j];
}
const t = performance.now();
const ok = sorted(s, 1) === "A 151655 T 150452 C 99156 G 98737" &&
  sorted(s, 2) === "AA 45992 TA 45599 AT 45574 TT 45299 CA 30129 AC 30096 AG 29993 GA 29935 CT 29857 " +
    "TC 29832 GT 29722 TG 29721 GC 19617 CC 19611 CG 19559 GG 19463" &&
  countOf(s, "GGT") === 5871 && countOf(s, "GGTA") === 1769 && countOf(s, "GGTATT") === 185 &&
  countOf(s, "GGTATTTTAATT") === 4 && countOf(s, "GGTATTTTAATTTATAGT") === 4;
report("k_nucleotide", t, ok);
