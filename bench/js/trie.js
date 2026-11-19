// bench/trie.prg in JavaScript: a trie of Maps, one per node, from a letter to the node [children, words]: 100,000
// words of 3 to 10 letters from an 8-letter alphabet inserted, then 100,000 other words of the same kind looked up, as
// words and as prefixes of the words stored.
const { report } = require("./bench.js");

let seed = 2718;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};
function newWord() {
  const n = rand(8) + 3;
  let w = "";
  for (let i = 0; i < n; i++) w += "etaoinsr"[rand(rand(8) + 1)];
  return w;
}

const words = new Array(100000), probes = new Array(100000);
for (let i = 0; i < words.length; i++) {
  words[i] = newWord();
  probes[i] = newWord();
}
const t = performance.now();
const root = [new Map(), 0];
let nodes = 1, found = 0, prefix = 0;
for (const w of words) {
  let node = root;
  for (let j = 0; j < w.length; j++) {
    const c = w[j];
    let next = node[0].get(c);
    if (next === undefined) {
      next = [new Map(), 0];
      node[0].set(c, next);
      nodes++;
    }
    node = next;
  }
  node[1]++;
}
for (const w of probes) {
  let node = root;
  for (let j = 0; j < w.length; j++) {
    node = node[0].get(w[j]);
    if (node === undefined) break;
  }
  if (node !== undefined) {
    prefix++;
    if (node[1] > 0) found++;
  }
}
report("trie", t, nodes === 159557 && found === 40767 && prefix === 50841);
