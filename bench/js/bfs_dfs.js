// bench/bfs_dfs.prg in JavaScript: breadth-first and depth-first search on an undirected graph of 50,000 vertices and
// 150,000 random edges (from a linear congruential generator) kept as arrays of adjacency: BFS from 6 sources (the sum
// of the levels and of the vertices reached), then an iterative DFS with an explicit stack over every component (the
// number of components and a checksum of the visiting order); 2 times. Vertices and slots are numbered from 1, as in
// the .prg (element 0 unused).
const { report } = require("./bench.js");

// The sum of the levels of the vertices reached from src, plus their number.
function bfs(src, nV, first, adj) {
  const level = new Array(nV + 1).fill(-1), queue = new Array(nV + 1);
  let head = 1, tail = 1, sum = 0;
  level[src] = 0;
  queue[1] = src;
  while (head <= tail) {
    const u = queue[head++], d = level[u];
    sum += d + 1;
    for (let e = first[u]; e <= first[u + 1] - 1; e++) {
      const v = adj[e];
      if (level[v] < 0) {
        level[v] = d + 1;
        queue[++tail] = v;
      }
    }
  }
  return sum;
}

// The number of components times 1,000,000,000 plus the sum of each vertex times its place in the visiting order.
function dfs(nV, first, adj) {
  const seen = new Array(nV + 1).fill(false), stack = new Array(2 * (adj.length - 1) + nV + 1);
  let order = 0, comps = 0, sum = 0;
  for (let s = 1; s <= nV; s++) {
    if (seen[s]) continue;
    comps++;
    let top = 1;
    stack[1] = s;
    while (top > 0) {
      const u = stack[top--];
      if (seen[u]) continue;
      seen[u] = true;
      order++;
      sum += u * order;
      for (let e = first[u + 1] - 1; e >= first[u]; e--) {
        if (!seen[adj[e]]) stack[++top] = adj[e];
      }
    }
  }
  return comps * 1000000000 + sum;
}

const t = performance.now();
const nV = 50000, nE = 150000, eu = new Array(nE + 1), ev = new Array(nE + 1), first = new Array(nV + 2).fill(0);
let seed = 3, total = 0;
for (let i = 1; i <= nE; i++) {
  seed = (seed * 16807) % 2147483647;
  eu[i] = (Math.floor(seed / 16) % nV) + 1;
  seed = (seed * 16807) % 2147483647;
  ev[i] = (Math.floor(seed / 16) % nV) + 1;
  first[eu[i]]++;
  first[ev[i]]++;
}
// degrees to the first slot of each vertex; first[ nV + 1 ] ends the last one
let fill = 1;
for (let i = 1; i <= nV + 1; i++) {
  const d = first[i];
  first[i] = fill;
  fill += d;
}
const next = first.slice(), adj = new Array(2 * nE + 1);
for (let i = 1; i <= nE; i++) {
  adj[next[eu[i]]++] = ev[i];
  adj[next[ev[i]]++] = eu[i];
}
for (let rep = 1; rep <= 2; rep++) {
  for (let src = 1; src <= 6; src++) total += bfs((src - 1) * 8000 + 1, nV, first, adj);
  total += dfs(nV, first, adj);
}
report("bfs_dfs", t, total === 62828511998050);
