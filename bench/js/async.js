// bench/async.prg in JavaScript: a million async calls that complete at once, awaited; a hundred thousand round trips
// between two async functions over channels of one; ten thousand handlers suspended at once on timers (one thread:
// the event loop).
const { report } = require("./bench.js");

class Channel { // a bounded channel of one value
  constructor() {
    this.value = undefined;
    this.full = false;
    this.readers = [];
    this.writers = [];
  }
  async write(v) {
    while (this.full) await new Promise((r) => this.writers.push(r));
    this.value = v;
    this.full = true;
    const r = this.readers.shift();
    if (r) r();
  }
  async read() {
    while (!this.full) await new Promise((r) => this.readers.push(r));
    const v = this.value;
    this.full = false;
    const w = this.writers.shift();
    if (w) w();
    return v;
  }
}

async function sync(k) { return k; }
async function echo(a, b, n) {
  for (let k = 1; k <= n; k++) await b.write(await a.read());
}
async function pingPong(n) {
  const a = new Channel(), b = new Channel(), t = echo(a, b, n);
  let s = 0;
  for (let k = 1; k <= n; k++) {
    await a.write(k);
    s += await b.read();
  }
  await t;
  return s;
}
const delay = (ms) => new Promise((r) => setTimeout(r, ms));
async function handler(k) {
  await delay(k % 10);
  // TaskRun( {|| NIL } ): Node has no pool of threads that runs JavaScript; a turn of the event loop stands for it.
  await new Promise((r) => setImmediate(r));
  return k;
}

(async () => {
  const t = performance.now();
  let s = 0;
  for (let k = 1; k <= 1000000; k++) s += await sync(k);
  s += await pingPong(100000);
  const h = [];
  for (let k = 1; k <= 10000; k++) h.push(handler(k));
  for (const x of await Promise.all(h)) s += x;
  report("async", t, s === 500000500000 + 5000050000 + 50005000);
})();
