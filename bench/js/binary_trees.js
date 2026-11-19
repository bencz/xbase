// bench/binary_trees.prg in JavaScript: the allocator and the collector, binary trees of two-element arrays made and
// walked, depths 4 to 14 (a stretch tree of depth 15, one of depth 14 that lives throughout, 2^(14 - d + 4) trees of
// each even depth d).
const { report } = require("./bench.js");

const make = (d) => (d === 0 ? [null, null] : [make(d - 1), make(d - 1)]);

// The nodes of the tree.
const check = (node) => (node[0] === null ? 1 : 1 + check(node[0]) + check(node[1]));

const t = performance.now();
const maxDepth = 14;
let total = 0;
total += check(make(maxDepth + 1));
const longLived = make(maxDepth);
for (let d = 4; d <= maxDepth; d += 2) {
  const iters = 1 << (maxDepth - d + 4);
  for (let i = 1; i <= iters; i++) total += check(make(d));
}
total += check(longLived);
report("binary_trees", t, total === 3222190);
