// bench/fib.prg in JavaScript: recursive calls, Fib( 37 ).
const { report } = require("./bench.js");
const fib = (n) => (n < 2 ? n : fib(n - 1) + fib(n - 2));
const t = performance.now();
report("fib", t, fib(37) === 24157817);
