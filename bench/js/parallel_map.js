// bench/parallel_map.prg in JavaScript: a parallel map: 1,000,000 numbers (1 to 1,000,000, from a fixed seed) mapped to
// their Collatz step counts by 64 tasks, each mapping its slice into an array of its own, on a pool of worker threads
// (one per processor, as the runtime's task workers); the slices are joined in order (Promise.all, then set() into the
// result), and the result checked: its sum, its maximum and where it is first, a running hash of the whole. Workers
// share no JavaScript arrays: the input is an Int32Array over a SharedArrayBuffer, each slice comes back as an
// Int32Array whose buffer is transferred, and the result is an Int32Array.
const { Worker, isMainThread, parentPort, workerData } = require("node:worker_threads");
const os = require("node:os");

function steps(x) {
  let s = 0;
  while (x !== 1) {
    x = x % 2 === 0 ? x / 2 : 3 * x + 1;
    s++;
  }
  return s;
}

if (!isMainThread) {
  const aIn = new Int32Array(workerData);
  parentPort.on("message", ({ id, from, count }) => {
    const a = new Int32Array(count);
    for (let i = 0; i < count; i++) a[i] = steps(aIn[from + i]);
    parentPort.postMessage({ id, a }, [a.buffer]);
  });
} else {
  const { report } = require("./bench.js");
  const n = 1000000;
  const sab = new SharedArrayBuffer(n * Int32Array.BYTES_PER_ELEMENT), aIn = new Int32Array(sab);
  let x = 777;
  for (let i = 0; i < n; i++) {
    x = (x * 16807) % 2147483647;
    aIn[i] = (x % 1000000) + 1;
  }
  const t = performance.now(); // the threads start inside the timing, as the runtime's start when tasks need them
  // A pool of workers, each given the shared input: a part goes to a free worker, or waits for one.
  const waiting = [], pending = new Map(), free = [];
  const workers = [];
  for (let k = 0; k < os.availableParallelism(); k++) {
    const w = new Worker(__filename, { workerData: sab });
    w.on("message", ({ id, a }) => {
      pending.get(id)(a);
      pending.delete(id);
      const next = waiting.shift();
      if (next) w.postMessage(next);
      else free.push(w);
    });
    workers.push(w);
    free.push(w);
  }
  let ids = 0;
  const run = (from, count) => new Promise((resolve) => {
    const job = { id: ids++, from, count };
    pending.set(job.id, resolve);
    const w = free.pop();
    if (w) w.postMessage(job);
    else waiting.push(job);
  });

  (async () => {
    const nChunk = n / 64, aTasks = [], aOut = new Int32Array(n);
    let nPos = 0, nSum = 0, nMax = 0, nAt = 0, nChk = 0;
    for (let i = 0; i < n; i += nChunk) aTasks.push(run(i, nChunk));
    for (const a of await Promise.all(aTasks)) {
      aOut.set(a, nPos);
      nPos += a.length;
    }
    for (let i = 0; i < n; i++) {
      const v = aOut[i];
      nSum += v;
      if (v > nMax) {
        nMax = v;
        nAt = i + 1;
      }
      nChk = (nChk * 31 + v) % 1000000007;
    }
    report("parallel_map", t, nPos === n && nSum === 131359344 && nMax === 524 && nAt === 237731 && nChk === 68222219);
    for (const w of workers) w.terminate();
  })();
}
