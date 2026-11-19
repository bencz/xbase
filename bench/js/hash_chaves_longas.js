// bench/hash_chaves_longas.prg in JavaScript: a Map of 300,000 keys of 82 to 131 bytes that share a 66-byte prefix,
// inserted, then looked up through other strings of the same text, and 300,000 missing keys of the same shape looked
// up.
const { report } = require("./bench.js");

let seed = 99;
function rand(n) {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
}
const zero = (n, w) => String(n).padStart(w, "0");

const prefix = "https://example.com/catalog/products/category/electronics/item?id=";
const keys = new Array(300000), same = new Array(300000), miss = new Array(300000);
for (let i = 1; i <= keys.length; i++) {
  const n = rand(1000000);
  keys[i - 1] = prefix + zero(i, 7) + "&v=" + zero(n, 6) + "x".repeat(i % 50);
  same[i - 1] = prefix + zero(i, 7) + "&v=" + zero(n, 6) + "x".repeat(i % 50);
  miss[i - 1] = prefix + zero(i, 7) + "&w=" + zero(n, 6) + "x".repeat(i % 50);
}
const t = performance.now();
const h = new Map();
let sum = 0, missing = 0;
for (let i = 0; i < keys.length; i++) h.set(keys[i], i + 1);
for (let i = same.length - 1; i >= 0; i--) sum += h.get(same[i]);
for (const k of miss) if (!h.has(k)) missing++;
report("hash_chaves_longas", t, h.size === 300000 && sum === 45000150000 && missing === 300000);
