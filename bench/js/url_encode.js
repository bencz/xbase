// bench/url_encode.prg in JavaScript: percent-encoding written out (as the .prg writes it): 20,000 strings of 20 to 80
// bytes (letters, digits, blanks, punctuation and bytes above 127, one character each) encoded as in a URL query
// (unreserved bytes kept, a blank as +, any other byte as %XX) and decoded back; 8 rounds. The string's length is drawn
// again at every test of its loop, as the .prg's FOR re-evaluates its limit.
const { report } = require("./bench.js");

let seed = 80;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};
const HEX = "0123456789ABCDEF";
const UNRESERVED = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~";

function urlEncode(c) {
  let out = "";
  for (let i = 0; i < c.length; i++) {
    const x = c[i];
    if (UNRESERVED.includes(x)) {
      out += x;
    } else if (x === " ") {
      out += "+";
    } else {
      const n = x.charCodeAt(0);
      out += "%" + HEX[Math.trunc(n / 16)] + HEX[n % 16];
    }
  }
  return out;
}

function urlDecode(c) {
  let out = "", i = 0;
  while (i < c.length) {
    const x = c[i];
    if (x === "%") {
      out += String.fromCharCode(HEX.indexOf(c[i + 1]) * 16 + HEX.indexOf(c[i + 2]));
      i += 3;
    } else {
      out += x === "+" ? " " : x;
      i++;
    }
  }
  return out;
}

const chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJ0123456789     -_.~&=/?%:#+" + String.fromCharCode(233, 252);
const a = new Array(20000);
for (let i = 0; i < a.length; i++) {
  let c = "";
  for (let j = 1; j <= 20 + rand(61); j++) c += chars[rand(chars.length)];
  a[i] = c;
}
const t = performance.now();
let n = 0, ok = true;
for (let r = 1; r <= 8; r++) {
  for (const c of a) {
    const enc = urlEncode(c);
    n += enc.length + enc.charCodeAt(enc.length - 1);
    ok = ok && urlDecode(enc) === c;
  }
}
ok = ok && n === 18744664 && urlEncode("a b&c=" + String.fromCharCode(233)) === "a+b%26c%3D%E9";
report("url_encode", t, ok);
