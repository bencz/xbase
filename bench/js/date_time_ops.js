// bench/date_time_ops.prg in JavaScript: 200,000 dates between 1900 and 2099 and times of day from a fixed seed, as
// Date values (UTC). For each: year, month and day against the YYYYMMDD text; the weekday and its name (Sundays,
// Fridays the 13th); the British DD/MM/YYYY text and back; 13 months added (the day kept within the month, through a
// date built from its text and date arithmetic); the days to the year's end; a time of day through HH:MM:SS and back.
const { report } = require("./bench.js");

const DAY = 86400000;
const DAYS = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
let seed = 1999;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};
const pad = (n, w) => String(n).padStart(w, "0");
const addDays = (d, n) => new Date(d.getTime() + n * DAY);
const diffDays = (a, b) => (a.getTime() - b.getTime()) / DAY;
const dtos = (d) => pad(d.getUTCFullYear(), 4) + pad(d.getUTCMonth() + 1, 2) + pad(d.getUTCDate(), 2);
const stod = (s) => new Date(Date.UTC(+s.slice(0, 4), +s.slice(4, 6) - 1, +s.slice(6, 8)));
const dtoc = (d) => pad(d.getUTCDate(), 2) + "/" + pad(d.getUTCMonth() + 1, 2) + "/" + pad(d.getUTCFullYear(), 4);
// DD/MM/YYYY back to a date; null (the empty date) when the text names no day of the calendar.
function ctod(s) {
  const day = +s.slice(0, 2), month = +s.slice(3, 5), year = +s.slice(6, 10);
  const d = new Date(Date.UTC(year, month - 1, day));
  return d.getUTCFullYear() === year && d.getUTCMonth() === month - 1 && d.getUTCDate() === day ? d : null;
}
const tstring = (s) => pad(Math.trunc(s / 3600), 2) + ":" + pad(Math.trunc(s / 60) % 60, 2) + ":" + pad(s % 60, 2);
const secs = (c) => +c.slice(0, 2) * 3600 + +c.slice(3, 5) * 60 + +c.slice(6, 8);

// The date n months after y-m-d, its day made the month's last when the month is shorter.
function addMonths(y, m, day, n) {
  let nm = m + n;
  const ny = y + Math.trunc((nm - 1) / 12);
  nm = ((nm - 1) % 12) + 1;
  const first = stod(pad(ny, 4) + pad(nm, 2) + "01");
  const next = addDays(first, 31);
  return addDays(first, Math.min(day, addDays(next, -next.getUTCDate()).getUTCDate()) - 1);
}

const t = performance.now();
const base = stod("19000101");
let ymd = 0, sun = 0, fri13 = 0, add = 0, toEnd = 0, sec = 0, bad = 0;
for (let i = 1; i <= 200000; i++) {
  const d = addDays(base, rand(73000));
  const y = d.getUTCFullYear(), m = d.getUTCMonth() + 1, day = d.getUTCDate();
  const n = y * 10000 + m * 100 + day;
  ymd += n;
  const back = ctod(dtoc(d));
  if (Number(dtos(d)) !== n || back === null || back.getTime() !== d.getTime()) bad++;
  if (d.getUTCDay() + 1 === 1) sun++;
  if (day === 13 && DAYS[d.getUTCDay()] === "Friday") fri13++;
  add += Number(dtos(addMonths(y, m, day, 13)));
  toEnd += diffDays(stod(String(y).padStart(4, " ") + "1231"), d);
  const s = rand(86400);
  if (secs(tstring(s)) !== s) bad++;
  sec += s;
}
report("date_time_ops", t, bad === 0 && ymd === 3998853334023 && sun === 28567 && fri13 === 951 &&
  add === 4001021793716 && toEnd === 36531947 && sec === 8632784536);
