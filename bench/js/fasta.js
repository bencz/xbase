// bench/fasta.prg in JavaScript: the fasta benchmark for n = 1,500,000: the ALU sequence repeated over 2n characters,
// then 3n IUB codes and 5n Homo sapiens bases picked from cumulative probabilities with the benchmark's LCG (seed 42),
// in lines of 60, appended to one string of 15 MB. Checked: its length, the generator's last state, the sum of the
// indexes picked, the end of the first and of the last section.
const { report } = require("./bench.js");
const IM = 139968, IA = 3877, IC = 29573, LINE = 60;

// n characters of the ALU sequence, cyclically, in lines of 60.
function repeatSeq(n) {
  const alu = "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTC" +
    "AGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCG" +
    "TGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGG" +
    "AGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA";
  const k = alu.length;
  let pos = 0, out = "";
  for (let done = 0; done < n; done += LINE) {
    const m = Math.min(n - done, LINE);
    for (let i = 0; i < m; i++) {
      out += alu[pos];
      if (++pos >= k) pos = 0;
    }
    out += "\n";
  }
  return out;
}

// n symbols drawn with the LCG in state[0] (the sum of their indexes, from 0, added to state[1]), in lines of 60.
function randomSeq(state, sym, prob, n) {
  const cum = [];
  let acc = 0, seed = state[0], picks = 0, out = "";
  for (const p of prob) cum.push(acc += p);
  for (let done = 0; done < n; done += LINE) {
    const m = Math.min(n - done, LINE);
    for (let i = 0; i < m; i++) {
      seed = (seed * IA + IC) % IM;
      const r = seed / IM;
      let j = 0;
      while (j < prob.length - 1 && r >= cum[j]) j++;
      picks += j;
      out += sym[j];
    }
    out += "\n";
  }
  state[0] = seed;
  state[1] += picks;
  return out;
}

const t = performance.now();
const n = 1500000, state = [42, 0];
let out = "";
out += ">ONE Homo sapiens alu\n";
out += repeatSeq(2 * n);
out += ">TWO IUB ambiguity codes\n";
out += randomSeq(state, "acgtBDHKMNRSVWY",
  [0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02], 3 * n);
out += ">THREE Homo sapiens frequency\n";
out += randomSeq(state, "acgt", [0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008], 5 * n);
const two = out.indexOf(">TWO") + 1;
const ok = out.length === 15250077 && state[0] === 28650 && state[1] === 25406718 && two === 3050023 &&
  out.substring(two - 22, two - 1) === "TGGGCGACAGAGCGAGACTC\n" && out.slice(-21) === "acacatatagttttatacaa\n";
report("fasta", t, ok);
