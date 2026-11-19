// bench/sequential_scan.prg in JavaScript: sequential scans of a table. Node has no DBF: the 200,000 sales records
// (region, amount, quantity, customer, date) made from a fixed seed go into "bench_seqscan.dat" in the current
// directory, a file of fixed-width ASCII records as a DBF's (a deletion flag, then the fields; numbers right-justified,
// the date as YYYYMMDD) without the DBF header. Then timed, 4 rounds of six passes over every record, each read back
// from the file through a buffer of records: a loop to the end with skip, a sum, a count with a condition, an
// evaluation of a function with a condition, a locate with a condition and its continue, and a sum over a date range.
// The file is deleted at the end.
const fs = require("node:fs");
const { report } = require("./bench.js");

const FILE = "bench_seqscan.dat";
// The record: deletion flag, REGION N 1, AMOUNT N 10, QTY N 3, CUST C 10, SALEDATE D 8.
const REGION = 1, AMOUNT = 2, QTY = 12, CUST = 15, SALEDATE = 25, RECLEN = 33;
const BLOCK = 65536 - (65536 % RECLEN);

// SToD(): the date of a YYYYMMDD text, as Date.UTC gives it.
const stod = (s) => Date.UTC(Number(s.substring(0, 4)), Number(s.substring(4, 6)) - 1, Number(s.substring(6, 8)));

let seed = 31337;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};

// A table opened on the file, one record current; its fields decoded from the buffered block that holds it.
class Table {
  constructor(path) {
    this.fd = fs.openSync(path, "r");
    this.recCount = fs.fstatSync(this.fd).size / RECLEN;
    this.buf = Buffer.alloc(BLOCK);
    this.first = 0; // the first record in the buffer (1-based; 0 when none)
    this.last = -1;
    this.recno = 0;
    this.off = 0;
    this.eof = true;
    this.found = false;
    this.locateFor = null;
  }
  close() { fs.closeSync(this.fd); }
  goTo(n) {
    this.recno = n;
    this.eof = n > this.recCount;
    if (this.eof) return;
    if (n < this.first || n > this.last) {
      const bytes = fs.readSync(this.fd, this.buf, 0, BLOCK, (n - 1) * RECLEN);
      this.first = n;
      this.last = n + bytes / RECLEN - 1;
    }
    this.off = (n - this.first) * RECLEN;
  }
  goTop() { this.goTo(1); }
  skip() { this.goTo(this.recno + 1); }
  num(at, len) {
    let n = 0;
    for (let i = this.off + at, end = i + len; i < end; i++) {
      const b = this.buf[i];
      if (b !== 32) n = n * 10 + b - 48;
    }
    return n;
  }
  region() { return this.num(REGION, 1); }
  amount() { return this.num(AMOUNT, 10); }
  qty() { return this.num(QTY, 3); }
  cust() { return this.buf.toString("latin1", this.off + CUST, this.off + CUST + 10); }
  // The date as Date.UTC gives it.
  saleDate() {
    const at = this.off + SALEDATE;
    return Date.UTC(this.num(SALEDATE, 4), (this.buf[at + 4] - 48) * 10 + this.buf[at + 5] - 49,
      (this.buf[at + 6] - 48) * 10 + this.buf[at + 7] - 48);
  }
  // SUM, COUNT and dbEval over every record, with an optional condition.
  eval(block, cond) {
    for (this.goTop(); !this.eof; this.skip()) if (cond === undefined || cond()) block();
  }
  sum(field, cond) {
    let s = 0;
    this.eval(() => (s += field()), cond);
    return s;
  }
  count(cond) {
    let n = 0;
    this.eval(() => n++, cond);
    return n;
  }
  // LOCATE FOR from the top, and CONTINUE from the next record.
  locate(cond) {
    this.locateFor = cond;
    this.goTop();
    this.seekFor();
  }
  continue() {
    this.skip();
    this.seekFor();
  }
  seekFor() {
    while (!this.eof && !this.locateFor()) this.skip();
    this.found = !this.eof;
  }
}

function scans(db) {
  let sumR3 = 0, cntR3 = 0, qty = 0, loc = 0, locIds = 0;
  db.goTop();
  while (!db.eof) {
    if (db.region() === 3) {
      sumR3 += db.amount();
      cntR3++;
    }
    db.skip();
  }
  const all = db.sum(() => db.amount());
  const big = db.count(() => db.amount() > 900000);
  db.eval(() => (qty += db.qty()), () => db.region() <= 2);
  db.locate(() => db.cust().substring(0, 8) === "CUST0001");
  while (db.found) {
    loc++;
    locIds += db.recno;
    db.continue();
  }
  const jan = db.sum(() => db.amount(), () => db.saleDate() < stod("20260201"));
  return sumR3 === 11099768125 && cntR3 === 22094 && all === 99898922935 && big === 19973 && qty === 1135702 &&
    loc === 396 && locIds === 40072280 && jan === 8435099403;
}

const records = 200000, data = Buffer.alloc(records * RECLEN, 32);
const base = Date.UTC(2026, 0, 1), pad = (n, w, c) => String(n).padStart(w, c);
for (let i = 0; i < records; i++) {
  const at = i * RECLEN;
  data.write(pad(rand(9) + 1, 1, " "), at + REGION, "latin1");
  data.write(pad(rand(1000000), 10, " "), at + AMOUNT, "latin1");
  data.write(pad(rand(50) + 1, 3, " "), at + QTY, "latin1");
  const d = new Date(base + rand(365) * 86400000);
  data.write(pad(d.getUTCFullYear(), 4, "0") + pad(d.getUTCMonth() + 1, 2, "0") + pad(d.getUTCDate(), 2, "0"),
    at + SALEDATE, "latin1");
  data.write("CUST" + pad(rand(50000), 6, "0"), at + CUST, "latin1");
}
fs.writeFileSync(FILE, data);
try {
  const db = new Table(FILE);
  const t = performance.now();
  let ok = true;
  for (let i = 1; i <= 4; i++) ok = ok && scans(db);
  report("sequential_scan", t, ok);
  db.close();
} finally {
  fs.unlinkSync(FILE);
}
