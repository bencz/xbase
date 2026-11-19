// bench/json_roundtrip.prg in JavaScript: JSON written and read back by the program's own code, as the .prg does (not
// JSON.stringify and JSON.parse): 20,000 records as Maps (an id, a name with quotes, backslashes, tabs and line feeds,
// an array of tags, a boolean, a number, a null, a nested Map) made from a fixed seed, then timed: serialized to text,
// parsed back into Maps and arrays by a recursive descent parser, and serialized again. The two texts must be equal,
// and the values read back add up as made.
const { report } = require("./bench.js");

let seed = 1618;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};

const quote = (c) =>
  '"' + c.replaceAll("\\", "\\\\").replaceAll('"', '\\"').replaceAll("\n", "\\n").replaceAll("\t", "\\t") + '"';

function toJson(x) {
  if (x instanceof Map) {
    let c = "{", sep = "";
    for (const [k, v] of x) {
      const item = quote(k) + ":" + toJson(v);
      c += sep + item;
      sep = ",";
    }
    return c + "}";
  }
  if (Array.isArray(x)) {
    let c = "[", sep = "";
    for (const v of x) {
      const item = toJson(v);
      c += sep + item;
      sep = ",";
    }
    return c + "]";
  }
  switch (typeof x) {
    case "string":
      return quote(x);
    case "number":
      return String(x);
    case "boolean":
      return x ? "true" : "false";
  }
  return "null";
}

// A parser over c; pos is the position of the next value, which parse() leaves after it.
class Parser {
  constructor(c) {
    this.c = c;
    this.pos = 0;
  }
  parse() {
    const c = this.c;
    let ch = c[this.pos], x = null;
    if (ch === "{") {
      x = new Map();
      this.pos++;
      if (c[this.pos] === "}") {
        this.pos++;
        return x;
      }
      for (;;) {
        const k = this.parseString();
        this.pos++;
        x.set(k, this.parse());
        ch = c[this.pos++];
        if (ch === "}") break;
      }
    } else if (ch === "[") {
      x = [];
      this.pos++;
      if (c[this.pos] === "]") {
        this.pos++;
        return x;
      }
      for (;;) {
        x.push(this.parse());
        ch = c[this.pos++];
        if (ch === "]") break;
      }
    } else if (ch === '"') {
      x = this.parseString();
    } else if (ch === "t") {
      x = true;
      this.pos += 4;
    } else if (ch === "f") {
      x = false;
      this.pos += 5;
    } else if (ch === "n") {
      this.pos += 4;
    } else {
      const k = this.pos;
      while (this.pos < c.length && "-0123456789".includes(c[this.pos])) this.pos++;
      x = Number(c.substring(k, this.pos));
    }
    return x;
  }
  parseString() {
    const c = this.c;
    let out = "", start = ++this.pos, ch;
    while ((ch = c[this.pos]) !== '"') {
      if (ch === "\\") {
        out += c.substring(start, this.pos);
        ch = c[this.pos + 1];
        out += ch === "n" ? "\n" : ch === "t" ? "\t" : ch;
        this.pos += 2;
        start = this.pos;
      } else {
        this.pos++;
      }
    }
    out += c.substring(start, this.pos);
    this.pos++;
    return out;
  }
}

// A record, drawn from the seed in the order the checks' constants were computed.
function makeRecord(i) {
  let name = "", parent = null;
  let n = rand(11) + 5;
  for (let k = 0; k < n; k++) name += 'abcdefghijk ABC"\\\n\txyz'[rand(22)];
  n = rand(4);
  const active = rand(2) === 1;
  const qty = rand(2001) - 1000;
  if (rand(3) !== 0) parent = rand(i) + 1;
  const city = "City " + rand(50);
  const zip = rand(90000) + 10000;
  const tags = [];
  for (let k = 0; k < n; k++) tags.push("tag" + rand(100));
  return new Map([["id", i], ["name", name], ["tags", tags], ["active", active], ["qty", qty], ["parent", parent],
    ["address", new Map([["city", city], ["zip", zip]])]]);
}

const data = new Array(20000);
for (let i = 1; i <= data.length; i++) data[i - 1] = makeRecord(i);
const t = performance.now();
const c1 = toJson(data);
const parser = new Parser(c1);
const back = parser.parse();
const c2 = toJson(back);
let qty = 0, active = 0, tags = 0, nulls = 0, zip = 0;
for (const h of back) {
  qty += h.get("qty");
  active += h.get("active") ? 1 : 0;
  tags += h.get("tags").length;
  nulls += h.get("parent") === null ? 1 : 0;
  zip += h.get("address").get("zip");
}
const ok = c1 === c2 && c1.length === 2697804 && parser.pos === c1.length && back.length === 20000 &&
  qty === -88759 && active === 9960 && tags === 29729 && nulls === 6525 && zip === 1098378340 &&
  back[0].get("name") === "ykAeACczkccifkx" && back[1].get("name") === 'Cdkk"\\Ae\nx\nz' &&
  c1.substring(0, 33) === '[{"id":1,"name":"ykAeACczkccifkx"';
report("json_roundtrip", t, ok);
