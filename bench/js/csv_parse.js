// bench/csv_parse.prg in JavaScript: CSV parsed character by character: a 40,000-line text (about 2 MB) of 5 fields,
// some quoted with commas, doubled quotes and line breaks inside, read by a state machine into an array of records;
// numbers read with parseInt; 6 rounds.
const { report } = require("./bench.js");

let seed = 1999;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};

function parseCsv(c) {
  const recs = [];
  let rec = [], field = "", quoted = false, i = 0;
  const len = c.length;
  while (i < len) {
    const x = c[i];
    if (quoted) {
      if (x === '"') {
        if (c[i + 1] === '"') {
          field += x;
          i++;
        } else {
          quoted = false;
        }
      } else {
        field += x;
      }
    } else if (x === '"') {
      quoted = true;
    } else if (x === ",") {
      rec.push(field);
      field = "";
    } else if (x === "\n") {
      rec.push(field);
      recs.push(rec);
      rec = [];
      field = "";
    } else {
      field += x;
    }
    i++;
  }
  return recs;
}

// A field: plain letters, or quoted with a comma, a doubled quote or a line break inside.
function makeField() {
  const k = rand(6);
  const w = String.fromCharCode(97 + rand(26)).repeat(3 + rand(12));
  if (k === 0) return '"' + w + ", " + w + '"';
  if (k === 1) return '"say ""' + w + '"""';
  if (k === 2) return '"' + w + "\n" + w + '"';
  return w;
}

let c = "";
for (let i = 1; i <= 40000; i++) {
  c += i + "," + makeField() + "," + rand(100000) + "," + makeField() + "," + "w".repeat(rand(10)) + "\n";
}
const t = performance.now();
let recs, n = 0, quoted = 0;
for (let r = 1; r <= 6; r++) {
  recs = parseCsv(c);
  for (const a of recs) {
    n += parseInt(a[0], 10) + parseInt(a[2], 10) + a[1].length + a[3].length + a.length;
    if (a[3].includes("\n") || a[1].includes('"')) quoted++;
  }
}
report("csv_parse", t, recs.length === 40000 && n === 16750579326 && quoted === 73038);
