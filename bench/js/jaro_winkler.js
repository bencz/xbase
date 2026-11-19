// bench/jaro_winkler.prg in JavaScript: the Jaro-Winkler similarity of 300,000 pairs of 6 to 14 letters (the second
// one the first with random changes), the sum of the doubles checked to 6 decimals.
const { report } = require("./bench.js");

let seed = 271;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};

function jaroWinkler(s1, s2) {
  const l1 = s1.length, l2 = s2.length;
  if (l1 === 0 || l2 === 0) return l1 === l2 ? 1 : 0;
  const range = Math.max(Math.trunc(Math.max(l1, l2) / 2) - 1, 0);
  const a1 = new Array(l1).fill(false), a2 = new Array(l2).fill(false);
  let m = 0, trans = 0, prefix = 0;
  for (let i = 0; i < l1; i++) {
    for (let j = Math.max(0, i - range); j <= Math.min(l2 - 1, i + range); j++) {
      if (!a2[j] && s1[i] === s2[j]) {
        a1[i] = true;
        a2[j] = true;
        m++;
        break;
      }
    }
  }
  if (m === 0) return 0;
  let k = 0;
  for (let i = 0; i < l1; i++) {
    if (a1[i]) {
      while (!a2[k]) k++;
      if (s1[i] !== s2[k]) trans++;
      k++;
    }
  }
  const jaro = (m / l1 + m / l2 + (m - trans / 2) / m) / 3;
  while (prefix < 4 && prefix < Math.min(l1, l2) && s1[prefix] === s2[prefix]) prefix++;
  return jaro + prefix * 0.1 * (1 - jaro);
}

function pair(len) {
  let s = "";
  for (let i = 0; i < len; i++) s += String.fromCharCode(65 + rand(6));
  let u = s;
  for (let i = 1; i <= 3; i++) {
    const k = rand(u.length) + 1, del = rand(2), ins = String.fromCharCode(65 + rand(6));
    u = u.substring(0, k - 1) + ins + u.substring(k - 1 + del);
  }
  return [s, u];
}

const pairs = new Array(300000);
for (let i = 0; i < pairs.length; i++) pairs[i] = pair(6 + rand(9));
const t = performance.now();
let sum = 0;
for (const p of pairs) sum += jaroWinkler(p[0], p[1]);
report("jaro_winkler", t, Math.abs(sum - 260943.0356007) < 0.000001 &&
  Math.abs(jaroWinkler("MARTHA", "MARHTA") - 0.961111111) < 0.000000001);
