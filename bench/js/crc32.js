// bench/crc32.prg in JavaScript: CRC-32 of 20,000 bytes (the table made each time, the data built in chunks), 100
// times.
const { report } = require("./bench.js");

function crc32Of20K() {
  const table = new Uint32Array(256);
  for (let i = 0; i < 256; i++) {
    let c = i;
    for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xedb88320 & -(c & 1));
    table[i] = c >>> 0;
  }
  let data = "";
  for (let i = 0; i < 20000; i += 100) {
    let chunk = "";
    for (let k = i; k < i + 100; k++) chunk += String.fromCharCode((k * 31 + 7) % 251);
    data += chunk;
  }
  let crc = 0xffffffff;
  for (let i = 0; i < data.length; i++) crc = (table[(crc ^ data.charCodeAt(i)) & 0xff] ^ (crc >>> 8)) >>> 0;
  return (crc ^ 0xffffffff) >>> 0;
}

const t = performance.now();
let ok = true;
for (let rep = 1; rep <= 100; rep++) ok = ok && crc32Of20K() === 2954121016;
report("crc32", t, ok);
