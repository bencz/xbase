// bench/async_pi.prg in JavaScript: the Basel sum by 32 async functions, each awaiting its 50,000,000 terms summed on
// a pool of worker threads (one per processor, as the runtime's task workers), then Promise.all; pi is
// Math.sqrt( 6 * sum ).
const { Worker, isMainThread, parentPort } = require("node:worker_threads");
const os = require("node:os");
const TASKS = 32;
const TERMS = 50000000;

function basel(from, to) {
  let s = 0;
  for (let n = from; n <= to; n++) {
    const inv = 1 / n;
    s += inv * inv;
  }
  return s;
}

if (!isMainThread) {
  parentPort.on("message", ({ id, from, to }) => parentPort.postMessage({ id, sum: basel(from, to) }));
} else {
  const { report } = require("./bench.js");
  // A pool of workers: a part goes to a free worker, or waits for one.
  const waiting = [], pending = new Map(), free = [];
  const workers = [];
  for (let k = 0; k < os.availableParallelism(); k++) {
    const w = new Worker(__filename);
    w.on("message", ({ id, sum }) => {
      pending.get(id)(sum);
      pending.delete(id);
      const next = waiting.shift();
      if (next) w.postMessage(next);
      else free.push(w);
    });
    workers.push(w);
    free.push(w);
  }
  let ids = 0;
  const run = (from, to) => new Promise((resolve) => {
    const job = { id: ids++, from, to };
    pending.set(job.id, resolve);
    const w = free.pop();
    if (w) w.postMessage(job);
    else waiting.push(job);
  });
  const partial = async (i) => await run(i * TERMS + 1, (i + 1) * TERMS);

  (async () => {
    const t = performance.now();
    const parts = [];
    for (let k = 0; k < TASKS; k++) parts.push(partial(k));
    let total = 0;
    for (const x of await Promise.all(parts)) total += x;
    const pi = Math.sqrt(total * 6);
    report("async_pi", t, Math.abs(Math.PI - pi) < 1e-9);
    for (const w of workers) w.terminate();
  })();
}
