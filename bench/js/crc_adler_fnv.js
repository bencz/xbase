// bench/crc_adler_fnv.prg in JavaScript: checksums of bytes, twenty strings of 1,000,000 characters (codes 0 .. 255)
// from an LCG, built a character at a time, each hashed with CRC-32 (from a table), Adler-32 and FNV-1a (32 bits);
// the functions are first checked against their published values ("123456789", "Wikipedia", "a"). The products
// seed * 1103515245 and h * 16777619 pass 2^53; Math.imul keeps their low 32 bits exact, and only those are kept.
// The 32-bit values are kept unsigned (>>> 0), as the .prg's 64-bit integers hold them.
const { report } = require("./bench.js");

function crc32Of(table, s) {
  let crc = 0xffffffff;
  for (let i = 0; i < s.length; i++) crc = (table[(crc ^ s.charCodeAt(i)) & 0xff] ^ (crc >>> 8)) >>> 0;
  return (crc ^ 0xffffffff) >>> 0;
}

function adler32Of(s) {
  let a = 1, b = 0;
  for (let i = 0; i < s.length; i++) {
    a += s.charCodeAt(i);
    if (a >= 65521) a -= 65521;
    b += a;
    if (b >= 65521) b -= 65521;
  }
  return ((b << 16) | a) >>> 0;
}

function fnv1aOf(s) {
  let h = 2166136261;
  for (let i = 0; i < s.length; i++) h = Math.imul(h ^ s.charCodeAt(i), 16777619) >>> 0;
  return h;
}

const t = performance.now();
const n = 1000000, table = new Array(256);
let seed = 42, total = 0;
for (let i = 0; i <= 255; i++) {
  let c = i;
  for (let k = 1; k <= 8; k++) c = ((c >>> 1) ^ (0xedb88320 & -(c & 1))) >>> 0;
  table[i] = c;
}
const ok = crc32Of(table, "123456789") === 0xcbf43926 && adler32Of("Wikipedia") === 0x11e60398 &&
  fnv1aOf("a") === 0xe40c292c;
for (let rep = 1; rep <= 20; rep++) {
  let data = "";
  for (let i = 1; i <= n; i++) {
    seed = (Math.imul(seed, 1103515245) + 12345) & 0x7fffffff;
    data += String.fromCharCode((seed >> 16) & 0xff);
  }
  total += crc32Of(table, data) + adler32Of(data) + fnv1aOf(data);
}
report("crc_adler_fnv", t, ok && total === 142519858956);
