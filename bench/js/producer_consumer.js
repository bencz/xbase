// bench/producer_consumer.prg in JavaScript: producers and consumers on worker threads over a bounded queue of
// 64 guarded by a mutex and two condition variables (not full, not empty): 4 producers put 50,000 numbers each, 4
// consumers take them until each finds the end mark the program puts once the producers are joined. The sum and count
// of what was taken must be what was put, whatever the scheduling. Workers share no JavaScript arrays: the queue is a
// ring of 64 32-bit integers in a SharedArrayBuffer, the mutex a lock word taken with compare-exchange, a condition a
// sequence number waited on with Atomics.wait; the end mark is 0 (no producer puts 0).
const { Worker, isMainThread, workerData, parentPort } = require("node:worker_threads");

const MUTEX = 0, NOT_FULL = 1, NOT_EMPTY = 2, HEAD = 3, COUNT = 4, RING = 5, SIZE = 64, END = 0;

// The lock word: 0 free, 1 held, 2 held with waiters.
function lock(s) {
  let c = Atomics.compareExchange(s, MUTEX, 0, 1);
  if (c === 0) return;
  if (c !== 2) c = Atomics.exchange(s, MUTEX, 2);
  while (c !== 0) {
    Atomics.wait(s, MUTEX, 2);
    c = Atomics.exchange(s, MUTEX, 2);
  }
}
function unlock(s) {
  if (Atomics.sub(s, MUTEX, 1) !== 1) {
    Atomics.store(s, MUTEX, 0);
    Atomics.notify(s, MUTEX, 1);
  }
}
// Releases the mutex, waits for a notification of the condition cv, takes the mutex back.
function wait(s, cv) {
  const seq = Atomics.load(s, cv);
  unlock(s);
  Atomics.wait(s, cv, seq);
  lock(s);
}
function notify(s, cv) {
  Atomics.add(s, cv, 1);
  Atomics.notify(s, cv, 1);
}

function put(s, x) {
  lock(s);
  while (s[COUNT] >= SIZE) wait(s, NOT_FULL);
  s[RING + ((s[HEAD] + s[COUNT]) % SIZE)] = x;
  s[COUNT]++;
  notify(s, NOT_EMPTY);
  unlock(s);
}

// Producer p puts p * 1000000 + i for i from 1 to n.
function produce(s, p, n) {
  for (let i = 1; i <= n; i++) put(s, p * 1000000 + i);
}

// Takes numbers until the end mark; returns [ their sum, their count ].
function consume(s) {
  let nSum = 0, nCount = 0;
  for (;;) {
    lock(s);
    while (s[COUNT] === 0) wait(s, NOT_EMPTY);
    const x = s[RING + s[HEAD]];
    s[HEAD] = (s[HEAD] + 1) % SIZE;
    s[COUNT]--;
    notify(s, NOT_FULL);
    unlock(s);
    if (x === END) break;
    nSum += x;
    nCount++;
  }
  return [nSum, nCount];
}

if (!isMainThread) {
  const s = new Int32Array(workerData.sab);
  if (workerData.p === 0) parentPort.postMessage(consume(s));
  else produce(s, workerData.p, workerData.n);
} else {
  const { report } = require("./bench.js");
  const nItems = 50000;
  const t = performance.now();
  const sab = new SharedArrayBuffer((RING + SIZE) * Int32Array.BYTES_PER_ELEMENT), s = new Int32Array(sab);
  const aP = [], aC = [];
  for (let k = 1; k <= 4; k++) {
    const w = new Worker(__filename, { workerData: { sab, p: 0 } });
    aC.push(new Promise((resolve) => w.once("message", resolve)));
  }
  for (let k = 1; k <= 4; k++) {
    const w = new Worker(__filename, { workerData: { sab, p: k, n: nItems } });
    aP.push(new Promise((resolve) => w.on("exit", resolve)));
  }
  (async () => {
    await Promise.all(aP);
    for (let k = 1; k <= 4; k++) put(s, END);
    let nSum = 0, nCount = 0;
    for (const a of await Promise.all(aC)) {
      nSum += a[0];
      nCount += a[1];
    }
    report("producer_consumer", t, nCount === 4 * nItems && nSum === 10000000 * nItems + 2 * nItems * (nItems + 1));
  })();
}
