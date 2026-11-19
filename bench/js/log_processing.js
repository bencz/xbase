// bench/log_processing.prg in JavaScript: 300,000 lines of an access log (time, level, [module], user, duration,
// status) made from a fixed seed as one text, then timed: split into lines, each line parsed (substrings at fixed
// columns, indexOf for the module and the key=value fields, parseInt), lines counted per level in a Map, durations
// summed per module, the slowest request found, server errors counted per hour, distinct users counted in a Map.
const { report } = require("./bench.js");

let seed = 9001;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};
const pad = (n, w) => String(n).padStart(w, "0");

const LEVELS = ["INFO ", "WARN ", "ERROR", "DEBUG"], MODULES = ["auth", "db", "http", "cache", "queue", "mail"];
const STATUS = [200, 200, 200, 201, 204, 301, 404, 500, 503, 200];
const n = 300000;
let lines = new Array(n);
for (let i = 1; i <= n; i++) {
  const sec = Math.trunc((i * 86400) / n);
  const x = rand(1000);
  let l = rand(10);
  l = l < 6 ? 1 : l < 8 ? 4 : l < 9 ? 2 : 3;
  lines[i - 1] = "2026-11-19 " + pad(Math.trunc(sec / 3600), 2) + ":" + pad(Math.trunc(sec / 60) % 60, 2) + ":" +
    pad(sec % 60, 2) + "." + pad(x, 3) + " " + LEVELS[l - 1] + " [" + MODULES[rand(6)] + "] request done user=u" +
    rand(5000) + " ms=" + rand(2000) + " status=" + String(STATUS[rand(10)]).padStart(3, " ");
}
let c = "";
for (const x of lines) c += x + "\n";
lines = null;
const t = performance.now();
const lv = new Map(), mod = new Map(), users = new Map(), hourErr = new Array(24).fill(0);
let i = 0, max = -1, maxLine = 0, err5 = 0;
for (const x of c.split("\n")) {
  if (x.trim() === "") continue;
  i++;
  const l = x.substring(24, 29).trimEnd();
  lv.set(l, (lv.get(l) ?? 0) + 1);
  const b = x.indexOf("[");
  const m = x.substring(b + 1, x.indexOf("]"));
  const ms = parseInt(x.substring(x.indexOf(" ms=") + 4), 10);
  mod.set(m, (mod.get(m) ?? 0) + ms);
  if (ms > max) {
    max = ms;
    maxLine = i;
  }
  const st = parseInt(x.substring(x.indexOf(" status=") + 8), 10);
  if (st >= 500) {
    err5++;
    hourErr[parseInt(x.substring(11, 13), 10)]++;
  }
  users.set(x.substring(x.indexOf(" user=") + 6, x.indexOf(" ms=")), true);
}
const ok = c.length === 23765927 && i === n && lv.get("INFO") === 179990 && lv.get("WARN") === 29916 &&
  lv.get("ERROR") === 30173 && lv.get("DEBUG") === 59921 && mod.get("auth") === 50005329 &&
  mod.get("db") === 50599071 && mod.get("http") === 50094170 && mod.get("cache") === 49851623 &&
  mod.get("queue") === 49898190 && mod.get("mail") === 49855051 && max === 1999 && maxLine === 1062 &&
  err5 === 59808 && hourErr[13] === 2503 && users.size === 5000;
report("log_processing", t, ok);
