// bench/xml_simples.prg in JavaScript: a catalog of 50,000 books [title, author, price, year] (titles and authors with
// & < > and ' in them) made from a fixed seed, then timed: written as one document (the text escaped), split into
// elements, and read back (each child's text found with indexOf between its tags, the id attribute, the entities
// decoded with replaceAll, the price with parseFloat). The values read back add up as made.
const { report } = require("./bench.js");

let seed = 5150;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};

// The text between <tag> and </tag> in c.
function inner(c, tag) {
  const start = c.indexOf("<" + tag + ">") + tag.length + 2;
  return c.substring(start, c.indexOf("</" + tag + ">"));
}
const escape = (c) =>
  c.replaceAll("&", "&amp;").replaceAll("<", "&lt;").replaceAll(">", "&gt;").replaceAll("'", "&apos;");
const unescape = (c) =>
  c.replaceAll("&apos;", "'").replaceAll("&gt;", ">").replaceAll("&lt;", "<").replaceAll("&amp;", "&");

const WORDS = ["Rivers", "Salt & Pepper", "<Night>", "Stone", "Tales", "Tom & Jerry", "Glass", "Winter", "Red", "Old"];
const AUTHORS = ["Ana Lima", "Bruno Costa", "Carla Dias", "Diego Reis", "Eva O'Neil"];
const books = new Array(50000);
for (let i = 0; i < books.length; i++) {
  const n = rand(2) + 2;
  let title = "";
  for (let k = 1; k <= n; k++) title += (k > 1 ? " " : "") + WORDS[rand(10)];
  books[i] = [title, AUTHORS[rand(5)], rand(10000) + 100, 1950 + rand(75)];
}
const t = performance.now();
let c = '<?xml version="1.0"?>\n<catalog>\n';
for (let i = 1; i <= books.length; i++) {
  const b = books[i - 1];
  c += '<book id="' + i + '"><title>' + escape(b[0]) + "</title><author>" + escape(b[1]) + "</author><price>" +
    (b[2] / 100).toFixed(2) + "</price><year>" + b[3] + "</year></book>\n";
}
c += "</catalog>\n";
let ok = c.length === 6276273;
const parts = c.split("<book ");
let ids = 0, amp = 0, c12345, eva = 0, cents = 0, years = 0;
for (let i = 1; i < parts.length; i++) {
  const book = parts[i];
  ids += parseInt(book.substring(4), 10);
  const title = unescape(inner(book, "title"));
  if (title.includes("&")) amp++;
  if (i === 12345) c12345 = title;
  if (unescape(inner(book, "author")) === "Eva O'Neil") eva++;
  cents += Math.round(parseFloat(inner(book, "price")) * 100);
  years += parseInt(inner(book, "year"), 10);
}
ok = ok && parts.length === 50001 && ids === (50000 * 50001) / 2 && cents === 255248286 && amp === 21225 &&
  years === 99351507 && eva === 10152 && c12345 === "Winter Glass Tom & Jerry";
report("xml_simples", t, ok);
