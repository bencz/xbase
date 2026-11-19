// bench/validacao_dados.prg in JavaScript: 250,000 records [CPF, e-mail, date text, amount text] made from a fixed
// seed, with errors mixed in (a wrong check digit, an e-mail with no "@", no dot or a bad character, impossible dates,
// a letter among the digits). Timed: each field validated by its rule (the CPF's two mod-11 check digits, the e-mail's
// parts and characters, the DD/MM/YYYY date through a Date in UTC, the amount's digits and range) and the valid ones
// counted.
const { report } = require("./bench.js");

let seed = 8086;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};
const isDigit = (x) => x >= "0" && x <= "9";

function cpfOk(c) {
  const d = c.replaceAll(".", "").replaceAll("-", "");
  if (d.length !== 11) return false;
  for (let i = 0; i < 11; i++) if (!isDigit(d[i])) return false;
  let s = 0;
  for (let i = 1; i <= 9; i++) s += (d.charCodeAt(i - 1) - 48) * (11 - i);
  if (((s * 10) % 11) % 10 !== d.charCodeAt(9) - 48) return false;
  s = 0;
  for (let i = 1; i <= 10; i++) s += (d.charCodeAt(i - 1) - 48) * (12 - i);
  return ((s * 10) % 11) % 10 === d.charCodeAt(10) - 48;
}

function emailOk(e) {
  const at = e.indexOf("@");
  if (at < 1 || e.substring(at + 1).includes("@")) return false;
  for (let i = 0; i < at; i++) if (!"abcdefghijklmnopqrstuvwxyz0123456789._".includes(e[i])) return false;
  const dom = e.substring(at + 1);
  if (dom.length < 3 || !dom.includes(".") || dom[0] === "." || dom[dom.length - 1] === ".") return false;
  for (let i = 0; i < dom.length; i++) if (!"abcdefghijklmnopqrstuvwxyz0123456789.-".includes(dom[i])) return false;
  return true;
}

// Whether the DD/MM/YYYY text names a day of the calendar (CToD gives a date, not the empty one).
function dateOk(s) {
  const day = +s.slice(0, 2), month = +s.slice(3, 5), year = +s.slice(6, 10);
  const d = new Date(Date.UTC(year, month - 1, day));
  return d.getUTCFullYear() === year && d.getUTCMonth() === month - 1 && d.getUTCDate() === day;
}

function amountOk(c) {
  for (let i = 0; i < c.length; i++) if (!isDigit(c[i])) return false;
  const n = parseInt(c, 10);
  return n >= 1 && n <= 999999;
}

// A record; its errors come from the seed as the checks' constants were computed.
function makeRecord() {
  const d = new Array(11);
  let s = 0, cpf = "", mail = "", amt = "";
  for (let k = 0; k < 9; k++) d[k] = rand(10);
  for (let k = 1; k <= 9; k++) s += d[k - 1] * (11 - k);
  d[9] = ((s * 10) % 11) % 10;
  s = 0;
  for (let k = 1; k <= 10; k++) s += d[k - 1] * (12 - k);
  d[10] = ((s * 10) % 11) % 10;
  if (rand(4) === 0) d[10] = (d[10] + 1) % 10;
  for (let k = 1; k <= 11; k++) cpf += d[k - 1] + (k === 3 || k === 6 ? "." : k === 9 ? "-" : "");
  let len = rand(8) + 3;
  for (let k = 0; k < len; k++) mail += "abcdefghijklmnopqrstuvwxyz0123456789"[rand(36)];
  const kind = rand(20);
  const dom = ["mail.com", "example.com.br", "x.org"][rand(3)];
  if (kind === 0) mail += dom;
  else if (kind === 1) mail += "@examplecom";
  else if (kind === 2) mail += "!@" + dom;
  else mail += "@" + dom;
  const date = String(rand(31) + 1).padStart(2, "0") + "/" + String(rand(12) + 1).padStart(2, "0") + "/" +
    (1990 + rand(41));
  len = rand(7) + 1;
  for (let k = 0; k < len; k++) amt += rand(10);
  if (rand(10) === 0) {
    const p = rand(len);
    amt = amt.substring(0, p) + "x" + amt.substring(p + 1);
  }
  return [cpf, mail, date, amt];
}

const recs = new Array(250000);
for (let i = 0; i < recs.length; i++) recs[i] = makeRecord();
const t = performance.now();
let nC = 0, nE = 0, nD = 0, nA = 0, all = 0, sum = 0;
for (const r of recs) {
  const lC = cpfOk(r[0]), lE = emailOk(r[1]), lD = dateOk(r[2]), lA = amountOk(r[3]);
  if (lA) sum += parseInt(r[3], 10);
  nC += lC ? 1 : 0;
  nE += lE ? 1 : 0;
  nD += lD ? 1 : 0;
  nA += lA ? 1 : 0;
  all += lC && lE && lD && lA ? 1 : 0;
}
report("validacao_dados", t, nC === 187295 && nE === 212602 && nD === 245457 && nA === 192603 && all === 120445 &&
  sum === 19490512431);
