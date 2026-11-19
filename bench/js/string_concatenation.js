// bench/string_concatenation.prg in JavaScript: 3,000,000 strings of seven parts joined with +, picked by index from
// small tables, each one's length and last character read.
const { report } = require("./bench.js");

const t = performance.now();
const a = ["alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta"];
const b = ["red", "green", "blue", "cyan", "magenta"];
let n = 0;
for (let i = 1; i <= 3000000; i++) {
  const s = a[i % 7] + "-" + b[i % 5] + "/" + a[i % 3] + ":" + b[i % 4];
  n += s.length + s.charCodeAt(s.length - 1);
}
report("string_concatenation", t, n === 378692857);
