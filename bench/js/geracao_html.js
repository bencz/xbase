// bench/geracao_html.prg in JavaScript: a table of 100,000 items [ name of two words, quantity, price in cents ] made
// from a fixed seed, then timed: one page built by appending to a string, each row with its id, a zebra class, the
// name escaped (replaceAll of & < > "), and the price and total formatted with toFixed( 2 ).
const { report } = require("./bench.js");

let seed = 4711;
function rand(n) {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
}
const escape = (c) => c.replaceAll("&", "&amp;").replaceAll("<", "&lt;").replaceAll(">", "&gt;")
  .replaceAll('"', "&quot;");

const words = ["Alpha", "Beta & Sons", "<Gamma>", "Delta", 'Echo "Q"', "Foxtrot", "Golf", "H&M"];
const items = new Array(100000);
for (let i = 0; i < items.length; i++) {
  const name = words[rand(8)] + " " + words[rand(8)];
  const qty = rand(100) + 1;
  items[i] = [name, qty, rand(1000000) + 1];
}
const t = performance.now();
let c = "<html><head><title>Items</title></head><body>\n<table>\n" +
  "<tr><th>Id</th><th>Name</th><th>Qty</th><th>Price</th><th>Total</th></tr>\n";
let pos77 = 0;
for (let i = 1; i <= items.length; i++) {
  const r = items[i - 1];
  const id = String(i);
  if (i === 77777) pos77 = c.length + 1;
  c += '<tr id="r' + id + '" class="' + (i % 2 === 1 ? "odd" : "even") + '"><td>' + id + "</td><td>" +
    escape(r[0]) + '</td><td class="n">' + String(r[1]) + '</td><td class="n">' + (r[2] / 100).toFixed(2) +
    '</td><td class="n">' + ((r[1] * r[2]) / 100).toFixed(2) + "</td></tr>\n";
}
c += "</table>\n</body></html>\n";
const row77 = '<tr id="r77777" class="odd"><td>77777</td><td>&lt;Gamma&gt; Delta</td><td class="n">56</td>' +
  '<td class="n">3992.87</td><td class="n">223600.72</td></tr>\n';
const tail = '<tr id="r100000" class="even"><td>100000</td><td>Foxtrot Alpha</td><td class="n">57</td>' +
  '<td class="n">4506.67</td><td class="n">256880.19</td></tr>\n</table>\n</body></html>\n';
report("geracao_html", t, c.length === 15121698 && pos77 === 11756338 &&
  c.substr(pos77 - 1, row77.length) === row77 && c.endsWith(tail));
