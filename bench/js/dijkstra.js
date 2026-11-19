// bench/dijkstra.prg in JavaScript: Dijkstra's shortest paths on a directed graph of 20,000 vertices and 120,000 edges
// (each vertex linked to the next and to 5 others, weights 1 .. 100, from a linear congruential generator) kept as
// arrays of adjacency, with a binary heap of (distance, vertex) pairs written out and stale entries skipped; from 12
// sources, the sum of the distances checked. Vertices, edges and heap slots are numbered from 1, as in the .prg.
const { report } = require("./bench.js");

// The sum of the shortest distances from vertex src to every vertex.
function dijkstra(src, nV, first, to, w) {
  const dist = new Array(nV + 1).fill(-1), done = new Array(nV + 1).fill(false);
  const hD = new Array(to.length + 1), hV = new Array(to.length + 1);
  let nH = 1, sum = 0;
  dist[src] = 0;
  hD[1] = 0;
  hV[1] = src;
  while (nH > 0) {
    const d = hD[1], u = hV[1];
    // pop: the last entry sifted down from the root
    const x = hD[nH], y = hV[nH];
    nH--;
    let i = 1, c;
    while ((c = i * 2) <= nH) {
      if (c < nH && hD[c + 1] < hD[c]) c++;
      if (hD[c] >= x) break;
      hD[i] = hD[c];
      hV[i] = hV[c];
      i = c;
    }
    hD[i] = x;
    hV[i] = y;
    if (done[u]) continue;
    done[u] = true;
    sum += d;
    for (let e = first[u]; e <= first[u + 1] - 1; e++) {
      const v = to[e], nd = d + w[e];
      if (!done[v] && (dist[v] < 0 || nd < dist[v])) {
        dist[v] = nd;
        // push: sifted up from a new leaf
        i = ++nH;
        while (i > 1 && hD[(c = Math.floor(i / 2))] > nd) {
          hD[i] = hD[c];
          hV[i] = hV[c];
          i = c;
        }
        hD[i] = nd;
        hV[i] = v;
      }
    }
  }
  return sum;
}

const t = performance.now();
const nV = 20000, nOut = 6, nE = nV * nOut, first = new Array(nV + 2), to = new Array(nE + 1), w = new Array(nE + 1);
let seed = 7, e = 0, total = 0;
for (let i = 1; i <= nV; i++) {
  first[i] = e + 1;
  e++;
  to[e] = i === nV ? 1 : i + 1;
  seed = (seed * 16807) % 2147483647;
  w[e] = (Math.floor(seed / 256) % 100) + 1;
  for (let k = 2; k <= nOut; k++) {
    e++;
    seed = (seed * 16807) % 2147483647;
    to[e] = (Math.floor(seed / 16) % nV) + 1;
    seed = (seed * 16807) % 2147483647;
    w[e] = (Math.floor(seed / 256) % 100) + 1;
  }
}
first[nV + 1] = e + 1;
for (let src = 1; src <= 12; src++) total += dijkstra((src - 1) * 1600 + 1, nV, first, to, w);
report("dijkstra", t, total === 41762116);
