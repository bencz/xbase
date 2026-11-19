// bench/ray_tracer.prg in Rust: a 320 x 240 image of five spheres ray traced (diffuse light, shadows, reflections
// three deep), vectors as values of three doubles (a Copy struct, as the C port: no allocation where the .prg makes
// arrays), checked by the sum of the pixels' gray levels, 2 times.
use std::time::Instant;

#[derive(Clone, Copy)]
struct V {
    x: f64,
    y: f64,
    z: f64,
}

fn add(a: V, b: V) -> V { V { x: a.x + b.x, y: a.y + b.y, z: a.z + b.z } }
fn sub(a: V, b: V) -> V { V { x: a.x - b.x, y: a.y - b.y, z: a.z - b.z } }
fn scale(a: V, s: f64) -> V { V { x: a.x * s, y: a.y * s, z: a.z * s } }
fn dot(a: V, b: V) -> f64 { a.x * b.x + a.y * b.y + a.z * b.z }
fn norm(a: V) -> V { scale(a, 1.0 / dot(a, a).sqrt()) }

// Its center, its radius, how much it reflects and its albedo.
struct Sphere {
    c: V,
    r: f64,
    refl: f64,
    albedo: f64,
}

const EPS: f64 = 0.0001;

// The distance along d from o to sphere s, or 0 when the ray misses it.
fn intersect(s: &Sphere, o: V, d: V) -> f64 {
    let oc = sub(o, s.c);
    let b = dot(oc, d);
    let disc = b * b - (dot(oc, oc) - s.r * s.r);
    if disc > 0.0 {
        let q = disc.sqrt();
        let mut t = -b - q;
        if t < EPS {
            t = -b + q;
        }
        if t > EPS {
            return t;
        }
    }
    0.0
}

// The gray level seen along d from o.
fn trace(spheres: &[Sphere], light: V, o: V, d: V, depth: i32) -> f64 {
    let mut best = 0.0;
    let mut hit: Option<&Sphere> = None;
    for s in spheres {
        let t = intersect(s, o, d);
        if t > 0.0 && (hit.is_none() || t < best) {
            best = t;
            hit = Some(s);
        }
    }
    let Some(hit) = hit else {
        return 0.2 + 0.5 * f64::max(0.0, d.y);
    };
    let p = add(o, scale(d, best));
    let n = scale(sub(p, hit.c), 1.0 / hit.r);
    let to_light = sub(light, p);
    let dist = dot(to_light, to_light).sqrt();
    let l = scale(to_light, 1.0 / dist);
    let mut diff = dot(n, l);
    for s in spheres {
        if diff <= 0.0 {
            break;
        }
        let t = intersect(s, p, l);
        if t > 0.0 && t < dist {
            diff = 0.0;
        }
    }
    let mut col = hit.albedo * (0.1 + 0.9 * f64::max(diff, 0.0));
    if depth < 3 && hit.refl > 0.0 {
        let r = sub(d, scale(n, 2.0 * dot(d, n)));
        col = col * (1.0 - hit.refl) + hit.refl * trace(spheres, light, p, r, depth + 1);
    }
    col
}

fn main() {
    let t = Instant::now();
    let v = |x, y, z| V { x, y, z };
    let spheres = std::hint::black_box(vec![
        Sphere { c: v(0.0, -1000.0, 0.0), r: 999.0, refl: 0.2, albedo: 0.6 },
        Sphere { c: v(0.0, 0.0, 5.0), r: 1.0, refl: 0.5, albedo: 0.9 },
        Sphere { c: v(-2.0, 0.0, 6.0), r: 1.0, refl: 0.3, albedo: 0.7 },
        Sphere { c: v(2.0, 0.0, 6.0), r: 1.0, refl: 0.3, albedo: 0.8 },
        Sphere { c: v(0.0, 2.2, 7.0), r: 1.0, refl: 0.6, albedo: 0.5 },
    ]);
    let light = v(5.0, 10.0, -5.0);
    let eye = v(0.0, 0.5, -1.0);
    let (width, height) = (320, 240);
    let mut ok = true;
    for _ in 0..2 {
        let mut total: i64 = 0;
        for y in 0..height {
            for x in 0..width {
                let d = norm(v(
                    (f64::from(x) - f64::from(width) / 2.0 + 0.5) / f64::from(height),
                    (f64::from(height) / 2.0 - f64::from(y) - 0.5) / f64::from(height),
                    1.0,
                ));
                total += (f64::min(trace(&spheres, light, eye, d, 0), 1.0) * 255.0) as i64;
            }
        }
        ok = ok && total == 7100994;
    }
    println!("ray_tracer {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
