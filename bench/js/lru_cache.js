// bench/lru_cache.prg in JavaScript: a least-recently-used cache of 2,000 entries: a Map from key to node and a doubly
// linked list of nodes [ key, value, prev, next ] in order of use; 2,000,000 reads of keys skewed toward small
// numbers, a miss computing the value and evicting the oldest entry when the cache is full.
const { report } = require("./bench.js");

const KEY = 0, VALUE = 1, PREV = 2, NEXT = 3;
let seed = 1234;
function rand(n) {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
}

const t = performance.now();
const h = new Map(), head = [null, null, null, null], tail = [null, null, null, null], cap = 2000;
let hits = 0, sum = 0;
head[NEXT] = tail;
tail[PREV] = head;
for (let i = 1; i <= 2000000; i++) {
  const k = rand(rand(20000) + 1);
  let node;
  if (h.has(k)) {
    hits++;
    node = h.get(k);
    node[PREV][NEXT] = node[NEXT];
    node[NEXT][PREV] = node[PREV];
  } else {
    if (h.size >= cap) {
      node = tail[PREV];
      node[PREV][NEXT] = tail;
      tail[PREV] = node[PREV];
      h.delete(node[KEY]);
    }
    node = [k, k * 3 + 1, null, null];
    h.set(k, node);
  }
  node[NEXT] = head[NEXT];
  node[PREV] = head;
  head[NEXT][PREV] = node;
  head[NEXT] = node;
  sum += node[VALUE];
}
report("lru_cache", t, hits === 378837 && sum === 30033837356 && h.size === cap);
