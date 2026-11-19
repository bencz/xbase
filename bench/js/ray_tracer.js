// bench/ray_tracer.prg in JavaScript: small arrays of doubles made and dropped, function calls: a 320 x 240 image of
// five spheres ray traced (diffuse light, shadows, reflections three deep), vectors as arrays of three doubles,
// checked by the sum of the pixels' gray levels, 2 times.
const { report } = require("./bench.js");

const vAdd = (a, b) => [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
const vSub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const vScale = (a, s) => [a[0] * s, a[1] * s, a[2] * s];
const vDot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const vNorm = (a) => vScale(a, 1.0 / Math.sqrt(vDot(a, a)));

// The distance along dir from origin to sphere s, or 0 when the ray misses it.
function intersect(s, origin, dir) {
  const oc = vSub(origin, s[0]), b = vDot(oc, dir), disc = b * b - (vDot(oc, oc) - s[1] * s[1]);
  if (disc > 0.0) {
    const q = Math.sqrt(disc);
    let t = -b - q;
    if (t < 0.0001) t = -b + q;
    if (t > 0.0001) return t;
  }
  return 0.0;
}

// The gray level seen along dir from origin.
function trace(spheres, light, origin, dir, depth) {
  let best = 0.0, hit = null;
  for (const s of spheres) {
    const t = intersect(s, origin, dir);
    if (t > 0.0 && (hit === null || t < best)) {
      best = t;
      hit = s;
    }
  }
  if (hit === null) return 0.2 + 0.5 * Math.max(0.0, dir[1]);
  const p = vAdd(origin, vScale(dir, best)), n = vScale(vSub(p, hit[0]), 1.0 / hit[1]);
  const toLight = vSub(light, p), dist = Math.sqrt(vDot(toLight, toLight)), l = vScale(toLight, 1.0 / dist);
  let diff = vDot(n, l);
  for (const s of spheres) {
    if (diff <= 0.0) break;
    const t = intersect(s, p, l);
    if (t > 0.0 && t < dist) diff = 0.0;
  }
  let col = hit[3] * (0.1 + 0.9 * Math.max(diff, 0.0));
  if (depth < 3 && hit[2] > 0.0) {
    col = col * (1.0 - hit[2]) + hit[2] * trace(spheres, light, p, vSub(dir, vScale(n, 2.0 * vDot(dir, n))), depth + 1);
  }
  return col;
}

const t = performance.now();
const width = 320, height = 240;
// Each sphere: its center, its radius, how much it reflects and its albedo.
const spheres = [[[0.0, -1000.0, 0.0], 999.0, 0.2, 0.6], [[0.0, 0.0, 5.0], 1.0, 0.5, 0.9],
  [[-2.0, 0.0, 6.0], 1.0, 0.3, 0.7], [[2.0, 0.0, 6.0], 1.0, 0.3, 0.8], [[0.0, 2.2, 7.0], 1.0, 0.6, 0.5]];
const light = [5.0, 10.0, -5.0], eye = [0.0, 0.5, -1.0];
let ok = true;
for (let rep = 1; rep <= 2; rep++) {
  let total = 0;
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const dir = vNorm([(x - width / 2.0 + 0.5) / height, (height / 2.0 - y - 0.5) / height, 1.0]);
      total += Math.trunc(Math.min(trace(spheres, light, eye, dir, 0), 1.0) * 255.0);
    }
  }
  ok = ok && total === 7100994;
}
report("ray_tracer", t, ok);
