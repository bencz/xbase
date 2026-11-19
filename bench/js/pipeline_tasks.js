// bench/pipeline_tasks.prg in JavaScript: a pipeline of four async stages joined by bounded channels (capacity 256): a
// source sends 200,000 records as text ("id;value", the values from a fixed seed), a parser splits them into
// [ id, value ], a stage maps each value to its square modulo 1,000,003 and keeps the even ones, and a sink sums them.
// Each stage awaits its sends and receives (for await over its input) and closes its output when its input ends. The
// stages run on the event loop, as async functions do in Node; the channel is a queue of values with queues of the
// receivers and senders waiting.
const { report } = require("./bench.js");

class Channel {
  constructor(capacity) {
    this.capacity = capacity;
    this.values = [];
    this.receivers = [];
    this.senders = [];
    this.closed = false;
  }
  // Resolves once the channel took the value: handed to a waiting receiver, kept while there is room, or else when a
  // receiver makes room.
  send(v) {
    if (this.closed) return Promise.reject(new Error("Channel closed"));
    const r = this.receivers.shift();
    if (r) {
      r({ value: v, done: false });
      return Promise.resolve(true);
    }
    if (this.values.length < this.capacity) {
      this.values.push(v);
      return Promise.resolve(true);
    }
    return new Promise((resolve, reject) => this.senders.push({ v, resolve, reject }));
  }
  // The next value as an iterator result; done once the channel is closed and empty.
  next() {
    if (this.values.length > 0) {
      const v = this.values.shift();
      const s = this.senders.shift();
      if (s) {
        this.values.push(s.v);
        s.resolve(true);
      }
      return Promise.resolve({ value: v, done: false });
    }
    if (this.closed) return Promise.resolve({ value: undefined, done: true });
    return new Promise((resolve) => this.receivers.push(resolve));
  }
  [Symbol.asyncIterator]() { return this; }
  close() {
    if (this.closed) return;
    this.closed = true;
    for (const r of this.receivers.splice(0)) r({ value: undefined, done: true });
    for (const s of this.senders.splice(0)) s.reject(new Error("Channel closed"));
  }
}

async function source(cOut, n) {
  let x = 4321;
  for (let i = 1; i <= n; i++) {
    x = (x * 16807) % 2147483647;
    await cOut.send(String(i) + ";" + String(x % 100000));
  }
  cOut.close();
}
async function parser(cIn, cOut) {
  for await (const c of cIn) {
    const n = c.indexOf(";");
    await cOut.send([Number(c.substring(0, n)), Number(c.substring(n + 1))]);
  }
  cOut.close();
}
async function square(cIn, cOut) {
  for await (const a of cIn) {
    const y = (a[1] * a[1]) % 1000003;
    if (y % 2 === 0) await cOut.send([a[0], y]);
  }
  cOut.close();
}
// [ the sum of the values, their count, the sum of their ids ]
async function sink(cIn) {
  let nSum = 0, nCount = 0, nIds = 0;
  for await (const a of cIn) {
    nSum += a[1];
    nCount++;
    nIds += a[0];
  }
  return [nSum, nCount, nIds];
}

(async () => {
  const t = performance.now();
  const c1 = new Channel(256), c2 = new Channel(256), c3 = new Channel(256);
  const aTasks = [source(c1, 200000), parser(c1, c2), square(c2, c3), sink(c3)];
  const aSink = await aTasks[3];
  await Promise.all(aTasks);
  report("pipeline_tasks", t, aSink[0] === 49893566082 && aSink[1] === 100073 && aSink[2] === 10032092994);
})();
