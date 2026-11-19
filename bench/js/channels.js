// bench/channels.prg in JavaScript: 4 producers send 250,000 numbers each through a bounded channel (capacity 64) to 2
// consumers (waiting when it is full or empty), then 1,000,000 trySend/tryReceive pairs on an unbounded one: what
// handing a value from task to task costs. Node has no pool of threads that runs JavaScript: the producers and
// consumers are async functions on the event loop (in xbase, tasks on the pool's threads), the channel a queue of
// values with queues of the receivers and senders waiting.
const { report } = require("./bench.js");

class Channel {
  constructor(capacity = Infinity) {
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
  trySend(v) {
    if (this.closed || this.values.length >= this.capacity) return false;
    const r = this.receivers.shift();
    if (r) r({ value: v, done: false });
    else this.values.push(v);
    return true;
  }
  tryReceive(slot) {
    if (this.values.length === 0) return false;
    slot[0] = this.values.shift();
    const s = this.senders.shift();
    if (s) {
      this.values.push(s.v);
      s.resolve(true);
    }
    return true;
  }
  close() {
    if (this.closed) return;
    this.closed = true;
    for (const r of this.receivers.splice(0)) r({ value: undefined, done: true });
    for (const s of this.senders.splice(0)) s.reject(new Error("Channel closed"));
  }
}

async function produce(ch, n) {
  for (let i = 1; i <= n; i++) await ch.send(i);
}
async function consume(ch) {
  let s = 0;
  for await (const x of ch) s += x;
  return s;
}

(async () => {
  const t = performance.now();
  const ch = new Channel(64), u = new Channel(), aSlot = [undefined], aP = [], aC = [];
  let s = 0;
  for (let k = 1; k <= 2; k++) aC.push(consume(ch));
  for (let k = 1; k <= 4; k++) aP.push(produce(ch, 250000));
  await Promise.all(aP);
  ch.close();
  for (const x of await Promise.all(aC)) s += x;
  for (let k = 1; k <= 1000000; k++) {
    u.trySend(k);
    u.tryReceive(aSlot);
    s += aSlot[0] % 2;
  }
  report("channels", t, s === 4 * 31250125000 + 500000);
})();
