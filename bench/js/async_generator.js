// bench/async_generator.prg in JavaScript: a pipeline of three async generators (a source of 1,000,000 numbers that
// yields to the event loop every 1000, a filter of the even ones, a map that doubles them), consumed by for await.
const { report } = require("./bench.js");

async function* source(n) {
  for (let k = 1; k <= n; k++) {
    if (k % 1000 === 0) await new Promise((r) => setImmediate(r));
    yield k;
  }
}

async function* evens(g) {
  for await (const x of g) if (x % 2 === 0) yield x;
}

async function* doubled(g) {
  for await (const x of g) yield x * 2;
}

(async () => {
  const start = performance.now();
  let s = 0;
  for await (const x of doubled(evens(source(1000000)))) s += x;
  report("async_generator", start, s === 500001000000);
})();
