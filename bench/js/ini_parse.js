// bench/ini_parse.prg in JavaScript: a text of 400 sections of 25 keys (comments, blank lines, spaces around "=",
// quoted values, section names in either case) made from a fixed seed, then timed: parsed 80 times into a Map of
// sections, each a Map of keys (lines split, trimmed, names lowered, quotes taken off); the numbers summed and values
// looked up.
const { report } = require("./bench.js");

let seed = 7777;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};
const strZero = (n, w) => String(n).padStart(w, "0");

// The sections of the text, each a Map of its keys, and the count of quoted values.
function parseIni(c) {
  const ini = new Map();
  let section, quoted = 0;
  // FOR EACH cLine IN c:Split( ... ): cLine is the element itself, so the trimmed line is stored back into the array.
  const parts = c.split("\n");
  for (let i = 0; i < parts.length; i++) {
    const line = parts[i] = parts[i].trim();
    if (line === "" || ";#".includes(line[0])) continue;
    if (line[0] === "[") {
      section = new Map();
      ini.set(line.substring(1, line.length - 1).toLowerCase(), section);
    } else {
      const eq = line.indexOf("=");
      if (eq >= 0) {
        const key = line.substring(0, eq).trimEnd().toLowerCase();
        let val = line.substring(eq + 1).trimStart();
        if (val.length >= 2 && val[0] === '"' && val[val.length - 1] === '"') {
          val = val.substring(1, val.length - 1);
          quoted++;
        }
        section.set(key, val);
      }
    }
  }
  return [ini, quoted];
}

const lines = [];
for (let s = 1; s <= 400; s++) {
  lines.push("; section " + s);
  lines.push("[" + (rand(2) === 1 ? "Section" : "SECTION") + s + "]");
  for (let k = 1; k <= 25; k++) {
    const sp = rand(3);
    const n = String(rand(100000));
    const val = k % 5 === 0 ? n : k % 5 === 1 ? '"text ' + n + '"' : "w" + n;
    lines.push((sp === 2 ? "  " : "") + "Key" + strZero(k, 2) + (sp > 0 ? " = " : "=") + val);
    if (k % 7 === 0) lines.push("# comment");
  }
  lines.push("");
}
let c = "";
for (const x of lines) c += x + "\n";
const t = performance.now();
let ok = true;
for (let i = 1; i <= 80; i++) {
  const [h, quoted] = parseIni(c);
  let sum = 0;
  for (const x of h.values()) {
    for (let k = 5; k <= 25; k += 5) sum += parseInt(x.get("key" + strZero(k, 2)), 10);
  }
  ok = ok && h.size === 400 && sum === 102092980 && quoted === 2000 &&
    h.get("section77").get("key11") === "text 72557" && h.get("section400").size === 25;
}
ok = ok && c.length === 181694;
report("ini_parse", t, ok);
