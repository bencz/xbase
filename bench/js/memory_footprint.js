// bench/memory_footprint.prg in JavaScript: memory footprint, read from V8 after full collections (the bytes of live
// objects, used_heap_size): an array of 1,000,000 integers, 200,000 objects of a class of three fields, a Map of
// 300,000 string keys, 600,000 records [ number, 11-character string ], each made by a function and measured before it
// returns. Each must stay within the .prg's budget of heap bytes per element, and once they are all dropped the heap
// must come back near where it started. V8 counts no live objects, so the .prg's limits on their count are not
// checked. The time is that of making them and of the collections, which plain `node` runs once V8's gc() is exposed.
const v8 = require("node:v8");
const vm = require("node:vm");
const { report } = require("./bench.js");

v8.setFlagsFromString("--expose-gc");
const gc = vm.runInNewContext("gc");
const heap = () => {
  gc();
  return v8.getHeapStatistics().used_heap_size;
};
// Str( i, 7 ): right-aligned in 7 characters.
const str7 = (i) => String(i).padStart(7, " ");

class TPoint {
  constructor(x, y) {
    this.x = x;
    this.y = y;
    this.cTag = "point";
  }
}

// Whether the structure x, of n elements, is within nBytes heap bytes per element.
function within(x, size, n, nBytes, nHeap) {
  const nUsed = heap() - nHeap;
  return size(x) === n && nUsed <= n * nBytes;
}
const length = (x) => x.length;

function integers(nHeap) {
  const a = new Array(1000000);
  for (let i = 0; i < a.length; i++) a[i] = (i + 1) * 3;
  return within(a, length, 1000000, 24, nHeap);
}
function objects(nHeap) {
  const a = new Array(200000);
  for (let i = 0; i < a.length; i++) a[i] = new TPoint(i + 1, -(i + 1));
  return within(a, length, 200000, 128, nHeap);
}
function keys(nHeap) {
  const h = new Map();
  for (let i = 1; i <= 300000; i++) h.set("k" + str7(i), i);
  return within(h, (x) => x.size, 300000, 224, nHeap);
}
function records(nHeap) {
  const a = new Array(600000);
  for (let i = 0; i < a.length; i++) a[i] = [i + 1, "item" + str7(i + 1)];
  return within(a, length, 600000, 224, nHeap);
}

const t = performance.now();
const nHeap = heap();
let ok = integers(nHeap) && objects(nHeap) && keys(nHeap) && records(nHeap);
ok = ok && heap() <= nHeap + 16 * 1024 * 1024;
report("memory_footprint", t, ok);
