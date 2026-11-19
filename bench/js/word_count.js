// bench/word_count.prg in JavaScript: 200,000 lines of 1 to 30 words (about 1,300,000 words, of a 5,000-word
// vocabulary in mixed case and a skewed distribution) cut at blanks, lower-cased and counted in a Map; the most
// frequent word found. The limits of the loops that make the words and the lines are drawn again at every test, as
// the .prg's FOR re-evaluates its limit.
const { report } = require("./bench.js");

let seed = 4242;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};

const vocab = new Array(5000), lines = new Array(200000);
for (let i = 0; i < vocab.length; i++) {
  let w = "";
  for (let j = 1; j <= 2 + rand(9); j++) w += "eEtaAoinNsrhldcumfpgwybvkxjqz"[rand(rand(29) + 1)];
  vocab[i] = w;
}
for (let i = 0; i < lines.length; i++) {
  let c = "";
  for (let j = 1; j <= 1 + rand(30); j++) c += vocab[rand(rand(5000) + 1)] + " ";
  lines[i] = c;
}
const t = performance.now();
const h = new Map();
let words = 0, top = 0, topWord = "", check = 0;
// FOR EACH c IN aLines: c is the element itself, so each shortened text is stored back into the array.
for (let i = 0; i < lines.length; i++) {
  let c = lines[i], n;
  while ((n = c.indexOf(" ")) >= 0) {
    const w = c.substring(0, n).toLowerCase();
    c = lines[i] = c.substring(n + 1);
    words++;
    const x = h.get(w);
    h.set(w, x === undefined ? 1 : x + 1);
  }
}
for (const n of h.values()) {
  check += n * n;
  if (n > top) top = n;
}
for (const w of vocab) {
  if (h.get(w.toLowerCase()) === top) {
    topWord = w.toLowerCase();
    break;
  }
}
report("word_count", t, words === 1312183 && h.size === 4328 && top === 7336 && topWord === "ee" &&
  check === 971695061);
