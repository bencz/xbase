// bench/linked_list.prg in JavaScript: a singly linked list of two-element arrays [ value, next ]: 1,000,000 nodes
// pushed at the head, walked, reversed in place, walked again, and every third node unlinked; 3 rounds.
const { report } = require("./bench.js");

const t = performance.now();
let sum = 0, count = 0, first = 0;
for (let r = 1; r <= 3; r++) {
  let head = null;
  for (let i = 1; i <= 1000000; i++) head = [i, head];
  let node = head;
  while (node !== null) {
    sum += node[0];
    node = node[1];
  }
  let prev = null;
  node = head;
  while (node !== null) {
    const next = node[1];
    node[1] = prev;
    prev = node;
    node = next;
  }
  head = prev;
  first += head[0];
  node = head;
  while (node !== null && node[1] !== null) {
    node[1] = node[1][1];
    node = node[1];
  }
  node = head;
  while (node !== null) {
    count++;
    node = node[1];
  }
}
report("linked_list", t, sum === 1500001500000 && first === 3 && count === 1500000);
