// bench/index.prg in JavaScript: a table of 200,000 records with random 10-digit keys, an order built on them, 100,000
// random seeks, a pass through the order, 20,000 records appended with the order kept up to date. Node has no DBF or
// NTX: the records are fixed-width (a deletion flag, K of 10 characters, N of 10) in "bench_index.dat", a file the
// program creates in its current directory (written through a buffer, read back a record at a time by position) and
// removes at the end; the order is an array of [ key, record number ] kept sorted in memory (a seek is a binary
// search, an append is put at its place with splice()).
const fs = require("node:fs");
const { report } = require("./bench.js");

const REC_LEN = 21;
const FILE_NAME = "bench_index.dat";

let seed = 7;
// ( seed * 1103515245 + 12345 ) % 2^31: the product passes 2^53, but the remainder by 2^31 needs only its low 32 bits,
// which Math.imul gives exactly.
function rand(n) {
  seed = (Math.imul(seed, 1103515245) + 12345) & 0x7fffffff;
  return Math.floor(seed / 65536) % n;
}
const zero10 = (n) => String(n).padStart(10, "0");
const record = (k, n) => " " + k.padEnd(10, " ") + String(n).padStart(10, " ");

// The key K of record recno (1-based), read from the file.
function readKey(fd, recno, buf) {
  fs.readSync(fd, buf, 0, REC_LEN, (recno - 1) * REC_LEN);
  return buf.toString("latin1", 1, 11);
}

// The first position of the order whose entry is not below [ key, rec ].
function lowerBound(order, key, rec) {
  let lo = 0, hi = order.length;
  while (lo < hi) {
    const mid = (lo + hi) >>> 1;
    const e = order[mid];
    if (e[0] < key || (e[0] === key && e[1] < rec)) lo = mid + 1;
    else hi = mid;
  }
  return lo;
}

const t = performance.now();
const recs = 200000;
let hits = 0, count = 0, ok = true;
{
  const out = Buffer.alloc(recs * REC_LEN);
  for (let i = 1; i <= recs; i++) out.write(record(zero10(rand(100000000)), i), (i - 1) * REC_LEN, "latin1");
  fs.writeFileSync(FILE_NAME, out);
}
const order = [];
{
  const data = fs.readFileSync(FILE_NAME);
  for (let recno = 1; recno <= recs; recno++) {
    const at = (recno - 1) * REC_LEN;
    order.push([data.toString("latin1", at + 1, at + 11), recno]);
  }
}
order.sort((p, q) => (p[0] < q[0] ? -1 : p[0] > q[0] ? 1 : p[1] - q[1]));
for (let i = 1; i <= 100000; i++) {
  const k = zero10(rand(100000000));
  // a soft seek: the first entry not below the key; a hit when its key is the one sought
  const at = lowerBound(order, k, 0);
  if (at < order.length && order[at][0] === k) hits++;
}
const fd = fs.openSync(FILE_NAME, "r+");
const buf = Buffer.alloc(REC_LEN);
let prev = "";
for (const e of order) {
  const k = readKey(fd, e[1], buf);
  ok = ok && k >= prev;
  prev = k;
  count++;
}
ok = ok && count === recs;
for (let i = 1; i <= 20000; i++) {
  const k = zero10(rand(100000000));
  fs.writeSync(fd, record(k, recs + i), (recs + i - 1) * REC_LEN, "latin1");
  order.splice(lowerBound(order, k, recs + i), 0, [k, recs + i]);
}
count = 0;
for (const e of order) {
  readKey(fd, e[1], buf);
  count++;
}
ok = ok && count === recs + 20000 && hits >= 0;
fs.closeSync(fd);
fs.unlinkSync(FILE_NAME);
report("index", t, ok);
