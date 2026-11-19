// bench/shared_counter.prg in JavaScript: a counter shared by 8 threads (worker threads), contended: each adds 1 to an
// atomic integer 250,000 times with Atomics.add, then 50,000 times with a compare-exchange loop, then 50,000 times to
// a shared element under a mutex (a lock word taken with compare-exchange, waited for with Atomics.wait). No increment
// may be lost, whatever the scheduling. The shared values are 32-bit integers in a SharedArrayBuffer (the .prg's
// AtomicInt holds 64 bits; the counts here stay far below 2^31).
const { Worker, isMainThread, workerData } = require("node:worker_threads");

const ATOMIC = 0, CAS = 1, MUTEX = 2, BOX = 3;

// The lock word: 0 free, 1 held, 2 held with waiters.
function lock(m) {
  let c = Atomics.compareExchange(m, MUTEX, 0, 1);
  if (c === 0) return;
  if (c !== 2) c = Atomics.exchange(m, MUTEX, 2);
  while (c !== 0) {
    Atomics.wait(m, MUTEX, 2);
    c = Atomics.exchange(m, MUTEX, 2);
  }
}
function unlock(m) {
  if (Atomics.sub(m, MUTEX, 1) !== 1) {
    Atomics.store(m, MUTEX, 0);
    Atomics.notify(m, MUTEX, 1);
  }
}

function work(m) {
  for (let i = 1; i <= 250000; i++) Atomics.add(m, ATOMIC, 1);
  for (let i = 1; i <= 50000; i++) {
    for (;;) {
      const n = Atomics.load(m, CAS);
      if (Atomics.compareExchange(m, CAS, n, n + 1) === n) break;
    }
  }
  for (let i = 1; i <= 50000; i++) {
    lock(m);
    m[BOX]++;
    unlock(m);
  }
}

if (!isMainThread) {
  work(new Int32Array(workerData));
} else {
  const { report } = require("./bench.js");
  const t = performance.now();
  const sab = new SharedArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT), m = new Int32Array(sab), aT = [];
  for (let k = 1; k <= 8; k++) {
    const w = new Worker(__filename, { workerData: sab });
    aT.push(new Promise((resolve) => w.on("exit", resolve)));
  }
  Promise.all(aT).then(() => {
    report("shared_counter", t, m[ATOMIC] === 8 * 250000 && m[CAS] === 8 * 50000 && m[BOX] === 8 * 50000);
  });
}
