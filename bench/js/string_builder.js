// bench/string_builder.prg in JavaScript: 3 rounds of 400,000 table rows of about 37 bytes appended with += to one
// string (about 15 MB each), its length and every 1,000th byte checked.
const { report } = require("./bench.js");

const t = performance.now();
const names = ["ant", "bee", "cat", "dog", "eel", "fox", "gnu", "hen", "ibis", "jay"];
let n = 0;
for (let r = 1; r <= 3; r++) {
  let c = "";
  for (let i = 1; i <= 400000; i++) c += "<tr><td>" + names[i % 10] + "</td><td>" + String(i + r) + "</td></tr>\n";
  n += c.length;
  for (let i = 0; i < c.length; i += 1000) n += c.charCodeAt(i);
}
report("string_builder", t, n === 47588053);
