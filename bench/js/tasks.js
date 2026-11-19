// bench/tasks.prg in JavaScript: 200,000 short tasks (each a function with an argument) joined by Promise.all, then a
// 50,000-link chain of then() and 20,000 tasks that each start and wait for another: what starting, scheduling,
// completing and waiting for a task costs. Node has no pool of threads that runs JavaScript: a task (TaskRun) is a
// turn of the event loop (setImmediate) that calls the function and settles a Promise; waiting for a result is await.
const { report } = require("./bench.js");

const taskRun = (f, ...args) => new Promise((resolve) => setImmediate(() => resolve(f(...args))));

(async () => {
  const t = performance.now();
  const a = new Array(200000);
  let s = 0;
  for (let k = 1; k <= a.length; k++) a[k - 1] = taskRun((i) => i % 7, k);
  const r = await Promise.all(a);
  r.forEach((x) => { s += x; });
  let c = Promise.resolve(0);
  for (let k = 1; k <= 50000; k++) c = c.then((p) => p + 1);
  s += await c;
  for (let k = 1; k <= 20000; k++) s += await taskRun(async () => await taskRun(() => 1));
  report("tasks", t, s === 599997 + 50000 + 20000);
})();
