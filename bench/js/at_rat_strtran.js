// bench/at_rat_strtran.prg in JavaScript: indexOf() and lastIndexOf() of 200 words in a 20,000-byte text of those
// words, and replaceAll() of each word by a longer one; 80 rounds.
const { report } = require("./bench.js");

let seed = 33;
function rand(n) {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
}

const words = new Array(200);
for (let i = 0; i < words.length; i++) {
  let w = "";
  // FOR ... TO evaluates its limit at every test, as here: Rand() is called on each.
  for (let j = 1; j <= 3 + rand(6); j++) w += String.fromCharCode(97 + rand(26));
  words[i] = w;
}
let c = "";
while (c.length < 20000) c += words[rand(rand(200) + 1)] + " ";
const t = performance.now();
let n = 0;
for (let r = 1; r <= 80; r++) {
  for (const w of words) n += c.indexOf(w) + 1 + c.lastIndexOf(w) + 1 + c.replaceAll(w, "<" + w + ">").length;
}
report("at_rat_strtran", t, n === 620527280);
