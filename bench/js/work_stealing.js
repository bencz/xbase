// bench/work_stealing.prg in JavaScript: fork-join over an unbalanced tree (as the unbalanced tree search benchmark):
// the root and the next four levels have 8 children each, every node below has 5 children or none as its hash says
// (subtrees of very different sizes, 2,859,034 nodes in all), each node hashed 40 times. The four levels below the
// root are tasks, 4,680 of them; a task awaits its children's results in turn. Node's threads share no functions or
// objects, so the tasks are async functions on the main thread and the work of the 4,096 tasks of the fourth level
// (the subtree walked in place) goes to a pool of worker threads (one per processor, as the runtime's task workers),
// each taking the next waiting part when it is free: a shared queue, not stealing from one another's queues. The node
// count and the sum of the hashes must be the tree's, whatever the scheduling.
const { Worker, isMainThread, parentPort } = require("node:worker_threads");
const os = require("node:os");

function hash(s) {
  for (let i = 1; i <= 40; i++) s = (s * 16807) % 2147483647;
  return s;
}

// Adds the nodes and the hashes of the subtree of seed s at depth d to acc.
function visit(s, d, acc) {
  const h = hash(s);
  acc.count++;
  acc.sum += h % 1000;
  if (d < 5 || h % 1000 < 198) {
    const n = d < 5 ? 8 : 5;
    for (let k = 1; k <= n; k++) visit((s * 16807 + k * 48271) % 2147483647, d + 1, acc);
  }
}

if (!isMainThread) {
  parentPort.on("message", ({ id, s, d }) => {
    const acc = { count: 0, sum: 0 };
    visit(s, d, acc);
    parentPort.postMessage({ id, count: acc.count, sum: acc.sum });
  });
} else {
  const { report } = require("./bench.js");
  const t = performance.now(); // the threads start inside the timing, as the runtime's start when tasks need them
  // A pool of workers: a part goes to a free worker, or waits for one.
  const waiting = [], pending = new Map(), free = [];
  const workers = [];
  for (let k = 0; k < os.availableParallelism(); k++) {
    const w = new Worker(__filename);
    w.on("message", ({ id, count, sum }) => {
      pending.get(id)([count, sum]);
      pending.delete(id);
      const next = waiting.shift();
      if (next) w.postMessage(next);
      else free.push(w);
    });
    workers.push(w);
    free.push(w);
  }
  let ids = 0;
  const run = (s, d) => new Promise((resolve) => {
    const job = { id: ids++, s, d };
    pending.set(job.id, resolve);
    const w = free.pop();
    if (w) w.postMessage(job);
    else waiting.push(job);
  });

  // [ nodes, sum ] of the subtree of seed s at depth d: its children as tasks down to depth 4, then walked in place.
  async function branch(s, d) {
    if (d >= 4) return run(s, d);
    const h = hash(s), aTasks = [];
    let nCount = 0, nSum = 0;
    for (let k = 1; k <= 8; k++) aTasks.push(branch((s * 16807 + k * 48271) % 2147483647, d + 1));
    for (let k = 7; k >= 0; k--) {
      const a = await aTasks[k];
      nCount += a[0];
      nSum += a[1];
    }
    return [nCount + 1, nSum + (h % 1000)];
  }

  (async () => {
    const a = await branch(19937, 0);
    report("work_stealing", t, a[0] === 2859034 && a[1] === 1428166988);
    for (const w of workers) w.terminate();
  })();
}
