// bench/md5_sha1.prg in JavaScript: MD5 and SHA-1 written out: 32-bit words (unsigned, wrapped with >>> 0 after each
// sum), rotations from the shift operators, bytes read with charCodeAt from a string. A 200,000-byte buffer from an
// LCG is hashed 40 times with each, padded as the standards say; both are first checked against the published digests
// of "" and "abc", and the buffer's against md5sum/sha1sum. The LCG's product passes 2^53; only its low 31 bits are
// kept, so Math.imul (the low 32 bits of the product, exact) gives the same values as the .prg's 64-bit integers.
const { report } = require("./bench.js");

const rotl = (x, s) => ((x << s) | (x >>> (32 - s))) >>> 0;

// The message, a 0x80 byte, zeros up to 56 bytes modulo 64, then its length in bits in 8 bytes, the least significant
// first when little.
function pad(s, little) {
  const n = s.length, len = Math.trunc((n + 8) / 64) * 64 + 64;
  let bits = "";
  for (let i = 0; i <= 7; i++) {
    const b = String.fromCharCode(Math.floor((n * 8) / 2 ** (8 * i)) % 256);
    bits = little ? bits + b : b + bits;
  }
  return s + "\x80" + "\0".repeat(len - n - 9) + bits;
}

// The words as lowercase hexadecimal, each word's bytes the least significant first when little.
function hex(words, little) {
  const digits = "0123456789abcdef";
  let out = "";
  for (const w of words) {
    for (let b = 0; b <= 3; b++) {
      const byte = (w >>> (little ? 8 * b : 24 - 8 * b)) & 0xFF;
      out += digits[byte >> 4];
      out += digits[byte & 15];
    }
  }
  return out;
}

const K = [
  0xD76AA478, 0xE8C7B756, 0x242070DB, 0xC1BDCEEE, 0xF57C0FAF, 0x4787C62A, 0xA8304613, 0xFD469501,
  0x698098D8, 0x8B44F7AF, 0xFFFF5BB1, 0x895CD7BE, 0x6B901122, 0xFD987193, 0xA679438E, 0x49B40821,
  0xF61E2562, 0xC040B340, 0x265E5A51, 0xE9B6C7AA, 0xD62F105D, 0x02441453, 0xD8A1E681, 0xE7D3FBC8,
  0x21E1CDE6, 0xC33707D6, 0xF4D50D87, 0x455A14ED, 0xA9E3E905, 0xFCEFA3F8, 0x676F02D9, 0x8D2A4C8A,
  0xFFFA3942, 0x8771F681, 0x6D9D6122, 0xFDE5380C, 0xA4BEEA44, 0x4BDECFA9, 0xF6BB4B60, 0xBEBFBC70,
  0x289B7EC6, 0xEAA127FA, 0xD4EF3085, 0x04881D05, 0xD9D4D039, 0xE6DB99E5, 0x1FA27CF8, 0xC4AC5665,
  0xF4292244, 0x432AFF97, 0xAB9423A7, 0xFC93A039, 0x655B59C3, 0x8F0CCC92, 0xFFEFF47D, 0x85845DD1,
  0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1, 0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391];
const R = [7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21];

function md5Of(s) {
  const p = pad(s, true), h = [0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476], m = new Array(16);
  for (let blk = 0; blk < p.length; blk += 64) {
    for (let j = 0; j <= 15; j++) {
      const q = blk + 4 * j;
      m[j] = (p.charCodeAt(q) | (p.charCodeAt(q + 1) << 8) | (p.charCodeAt(q + 2) << 16) |
        (p.charCodeAt(q + 3) << 24)) >>> 0;
    }
    let a = h[0], b = h[1], c = h[2], d = h[3];
    for (let i = 0; i <= 63; i++) {
      let f, g;
      if (i < 16) {
        f = (b & c) | (~b & d);
        g = i;
      } else if (i < 32) {
        f = (d & b) | (~d & c);
        g = (5 * i + 1) & 15;
      } else if (i < 48) {
        f = b ^ c ^ d;
        g = (3 * i + 5) & 15;
      } else {
        f = c ^ (b | ~d);
        g = (7 * i) & 15;
      }
      f >>>= 0;
      const tmp = d;
      d = c;
      c = b;
      b = (b + rotl((a + f + K[i] + m[g]) >>> 0, R[(i >> 4) * 4 + (i & 3)])) >>> 0;
      a = tmp;
    }
    h[0] = (h[0] + a) >>> 0;
    h[1] = (h[1] + b) >>> 0;
    h[2] = (h[2] + c) >>> 0;
    h[3] = (h[3] + d) >>> 0;
  }
  return hex(h, true);
}

function sha1Of(s) {
  const p = pad(s, false), h = [0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0], w = new Array(80);
  for (let blk = 0; blk < p.length; blk += 64) {
    for (let j = 0; j <= 15; j++) {
      const q = blk + 4 * j;
      w[j] = ((p.charCodeAt(q) << 24) | (p.charCodeAt(q + 1) << 16) | (p.charCodeAt(q + 2) << 8) |
        p.charCodeAt(q + 3)) >>> 0;
    }
    for (let j = 16; j < 80; j++) w[j] = rotl((w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16]) >>> 0, 1);
    let a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (let i = 0; i <= 79; i++) {
      let f, k;
      if (i < 20) {
        f = (b & c) | (~b & d);
        k = 0x5A827999;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }
      const tmp = (rotl(a, 5) + (f >>> 0) + e + k + w[i]) >>> 0;
      e = d;
      d = c;
      c = rotl(b, 30);
      b = a;
      a = tmp;
    }
    h[0] = (h[0] + a) >>> 0;
    h[1] = (h[1] + b) >>> 0;
    h[2] = (h[2] + c) >>> 0;
    h[3] = (h[3] + d) >>> 0;
    h[4] = (h[4] + e) >>> 0;
  }
  return hex(h, false);
}

const t = performance.now();
const n = 200000;
let seed = 7, data = "";
let ok = md5Of("") === "d41d8cd98f00b204e9800998ecf8427e" && md5Of("abc") === "900150983cd24fb0d6963f7d28e17f72" &&
  sha1Of("") === "da39a3ee5e6b4b0d3255bfef95601890afd80709" &&
  sha1Of("abc") === "a9993e364706816aba3e25717850c26c9cd0d89d";
for (let i = 1; i <= n; i++) {
  seed = (Math.imul(seed, 1103515245) + 12345) & 0x7FFFFFFF;
  data += String.fromCharCode((seed >> 16) & 0xFF);
}
for (let rep = 1; rep <= 40; rep++) {
  ok = ok && md5Of(data) === "9caadd7dbbe1174da261d335b92f85fb" &&
    sha1Of(data) === "cec3d79455d923d234a1a40741be135a4ba980e2";
}
report("md5_sha1", t, ok);
