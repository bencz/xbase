// bench/chameneos_redux.prg in JavaScript: creatures on threads of their own (worker_threads) ask for meetings on one
// channel (their messages to the main thread); a broker (the main thread) pairs them two by two and answers each on
// its own channel (its worker's port) with the other's name and color, after which both change color; 30,000
// meetings with 3 creatures, then 30,000 with 10. Checked: the color table, and per run 2N meetings counted, none of a
// creature with itself.
const { Worker, isMainThread, parentPort, workerData } = require("node:worker_threads");
const BLUE = 0, RED = 1, YELLOW = 2;
const complement = (a, b) => (a === b ? a : 3 - a - b);

if (!isMainThread) {
  // A creature: asks for meetings until none is left; its result is { meetings, meetings with itself }.
  const id = workerData.id;
  let color = workerData.color, met = 0, same = 0;
  parentPort.on("message", (other) => {
    if (other === null) {
      parentPort.postMessage({ result: true, met, same });
      parentPort.close();
      return;
    }
    color = complement(color, other.color);
    met++;
    if (other.id === id) same++;
    parentPort.postMessage({ id, color });
  });
  parentPort.postMessage({ id, color });
} else {
  const { report } = require("./bench.js");

  // Whether n meetings of creatures of these colors count 2n meetings, none with itself; the main thread is the
  // broker, and a run ends when every creature's thread has given its result and ended (a Join).
  const run = (n, colors) => new Promise((resolve) => {
    let left = n, first = null, met = 0, same = 0, results = 0, exits = 0;
    const finish = () => {
      if (results === colors.length && exits === colors.length) resolve(met === 2 * n && same === 0);
    };
    colors.forEach((c, i) => {
      const w = new Worker(__filename, { workerData: { id: i + 1, color: c } });
      w.on("message", (m) => {
        if (m.result) {
          met += m.met;
          same += m.same;
          results++;
          finish();
        } else if (left === 0) {
          w.postMessage(null);
        } else if (first === null) {
          first = { m, w };
        } else {
          first.w.postMessage({ id: m.id, color: m.color });
          w.postMessage({ id: first.m.id, color: first.m.color });
          first = null;
          left--;
        }
      });
      w.on("exit", () => {
        exits++;
        finish();
      });
    });
  });

  (async () => {
    const t = performance.now();
    let table = "";
    for (let a = 0; a <= 2; a++) for (let b = 0; b <= 2; b++) table += complement(a, b);
    let ok = table === "021210102";
    ok = (await run(30000, [BLUE, RED, YELLOW])) && ok;
    ok = (await run(30000, [BLUE, RED, YELLOW, RED, YELLOW, BLUE, RED, YELLOW, RED, BLUE])) && ok;
    report("chameneos_redux", t, ok);
  })();
}
