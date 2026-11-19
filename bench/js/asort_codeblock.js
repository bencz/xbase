// bench/asort_codeblock.prg in JavaScript: 150,000 employees [ name, department, salary, id ] made from a fixed seed,
// sorted with the library's (stable) sort by department, then salary descending, then name, through a comparison of
// three keys. The order is checked with a running hash of the ids.
const { report } = require("./bench.js");

let seed = 4242;
function rand(n) {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
}

const a = new Array(150000);
for (let i = 0; i < a.length; i++) {
  let c = "";
  for (let k = 1; k <= 8; k++) c += String.fromCharCode(65 + rand(26));
  a[i] = [c, rand(40) + 1, rand(90000) + 1000, i + 1];
}
const t = performance.now();
a.sort((p, q) => {
  if (p[1] !== q[1]) return p[1] - q[1];
  if (p[2] !== q[2]) return q[2] - p[2];
  return p[0] < q[0] ? -1 : p[0] > q[0] ? 1 : 0;
});
let chk = 0;
for (let i = 0; i < a.length; i++) chk = (chk * 31 + a[i][3]) % 1000000007;
report("asort_codeblock", t, chk === 773935306 && a[0][3] === 84060 && a[0][0] === "SOFHDANH" &&
  a[75000][3] === 10853 && a[149999][3] === 11664);
