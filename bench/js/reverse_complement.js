// bench/reverse_complement.prg in JavaScript: strings read and built a character at a time: the output of fasta for
// n = 2,500,000 (25 MB, made with the benchmark's LCG before the clock starts) read section by section, each sequence
// joined, reverse-complemented through the IUB table (an array indexed by the character code) and written again in
// lines of 60 after its header.
const { report } = require("./bench.js");
const IM = 139968, IA = 3877, IC = 29573, LINE = 60;

// The sequence reverse-complemented, in lines of 60.
function revComp(seq, comp) {
  let out = "", col = 0;
  for (let i = seq.length - 1; i >= 0; i--) {
    out += comp[seq.charCodeAt(i)];
    if (++col === LINE) {
      out += "\n";
      col = 0;
    }
  }
  if (col > 0) out += "\n";
  return out;
}

function repeatSeq(n) {
  const alu = "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTC" +
    "AGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCG" +
    "TGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGG" +
    "AGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA";
  let pos = 0, out = "";
  for (let done = 0; done < n; done += LINE) {
    const m = Math.min(n - done, LINE);
    for (let i = 0; i < m; i++) {
      out += alu[pos];
      if (++pos >= alu.length) pos = 0;
    }
    out += "\n";
  }
  return out;
}

function randomSeq(seedBox, sym, prob, n) {
  const cum = [];
  let acc = 0, seed = seedBox[0], out = "";
  for (const p of prob) cum.push(acc += p);
  for (let done = 0; done < n; done += LINE) {
    const m = Math.min(n - done, LINE);
    for (let i = 0; i < m; i++) {
      seed = (seed * IA + IC) % IM;
      const r = seed / IM;
      let j = 0;
      while (j < prob.length - 1 && r >= cum[j]) j++;
      out += sym[j];
    }
    out += "\n";
  }
  seedBox[0] = seed;
  return out;
}

// The three sections of fasta for n: the ALU sequence over 2n characters, 3n IUB codes, 5n Homo sapiens bases.
function fasta(n) {
  const seed = [42];
  return ">ONE Homo sapiens alu\n" + repeatSeq(2 * n) + ">TWO IUB ambiguity codes\n" +
    randomSeq(seed, "acgtBDHKMNRSVWY",
      [0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02], 3 * n) +
    ">THREE Homo sapiens frequency\n" +
    randomSeq(seed, "acgt", [0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008], 5 * n);
}

const input = fasta(2500000);
input.charCodeAt(0); // one flat string before the clock starts, as the .prg's is
const t = performance.now();
const from = "ACBDGHKMNSRUTWVYacbdghkmnsrutwvy", to = "TGVHCDMKNSYAABWRTGVHCDMKNSYAABWR", comp = new Array(256);
for (let i = 0; i < 256; i++) comp[i] = String.fromCharCode(i);
for (let i = 0; i < from.length; i++) comp[from.charCodeAt(i)] = to[i];
let out = "", seq = "", i = 0;
while (i < input.length) {
  let c = input[i];
  if (c === ">") {
    if (seq !== "") {
      out += revComp(seq, comp);
      seq = "";
    }
    while ((c = input[i++]) !== "\n") out += c;
    out += c;
  } else {
    if (c !== "\n") seq += c;
    i++;
  }
}
out += revComp(seq, comp);
const two = out.indexOf(">TWO") + 1, three = out.indexOf(">THREE") + 1;
const ok = out.length === 25416745 &&
  out.substr(22, 60) === "GTAGCTGGGATTACAGGCGCGCGCCACCACGCCCGGCTAATTTTTGTATTTTTAGTAGAG" &&
  out.substr(two + 24, 60) === "TTTATTYSTTGTATCCCTMWCGAACTAVGYHTATANTAWCARAWTTTVCTSTKDNTATHA" &&
  out.substr(three + 29, 60) === "GTAATTTCTCTCGGCTATTAGATTTGAACATATACACAGGCCAGATGATCTGGAAAAATC" &&
  out.slice(-21) === "CTATAACTAATATATTCGTC\n";
report("reverse_complement", t, ok);
