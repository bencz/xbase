// bench/base64.prg in JavaScript: Base64 written out (as the .prg writes it, not Buffer's): 1,200,000 bytes from a
// fixed-seed generator, one character each, encoded (three bytes to four characters through a table) and decoded back;
// 3 rounds.
const { report } = require("./bench.js");

const TABLE = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
let seed = 64;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};

function encode(c) {
  let out = "";
  const len = c.length, rest = len % 3;
  for (let i = 0; i < len - rest; i += 3) {
    const n = c.charCodeAt(i) * 65536 + c.charCodeAt(i + 1) * 256 + c.charCodeAt(i + 2);
    out += TABLE[Math.trunc(n / 262144)] + TABLE[Math.trunc(n / 4096) % 64] + TABLE[Math.trunc(n / 64) % 64] +
      TABLE[n % 64];
  }
  if (rest === 1) {
    const n = c.charCodeAt(len - 1);
    out += TABLE[Math.trunc(n / 4)] + TABLE[(n % 4) * 16] + "==";
  } else if (rest === 2) {
    const n = c.charCodeAt(len - 2) * 256 + c.charCodeAt(len - 1);
    out += TABLE[Math.trunc(n / 1024)] + TABLE[Math.trunc(n / 16) % 64] + TABLE[(n % 16) * 4] + "=";
  }
  return out;
}

let VALUE = null; // built on the first decode, as the .prg's STATIC table
function decode(c) {
  let out = "", pad = 0;
  if (VALUE === null) {
    VALUE = new Array(256).fill(0);
    for (let i = 0; i < 64; i++) VALUE[TABLE.charCodeAt(i)] = i;
  }
  if (c.endsWith("==")) pad = 2;
  else if (c.endsWith("=")) pad = 1;
  for (let i = 0; i < c.length; i += 4) {
    const n = VALUE[c.charCodeAt(i)] * 262144 + VALUE[c.charCodeAt(i + 1)] * 4096 + VALUE[c.charCodeAt(i + 2)] * 64 +
      VALUE[c.charCodeAt(i + 3)];
    out += String.fromCharCode(Math.trunc(n / 65536)) + String.fromCharCode(Math.trunc(n / 256) % 256) +
      String.fromCharCode(n % 256);
  }
  return out.substring(0, out.length - pad);
}

let c = "";
for (let i = 0; i < 1200000; i++) c += String.fromCharCode(rand(256));
const t = performance.now();
let n = 0, ok = true;
for (let r = 1; r <= 3; r++) {
  const enc = encode(c.substring(0, c.length - r + 1));
  ok = ok && decode(enc) === c.substring(0, c.length - r + 1);
  n += enc.length;
  for (let i = 0; i < enc.length; i += 97) n += enc.charCodeAt(i);
}
ok = ok && n === 9038319 && encode("Ma") === "TWE=" && decode("TQ==") === "M";
report("base64", t, ok);
