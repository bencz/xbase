// bench/dijkstra.prg in Rust: Dijkstra's shortest paths on a directed graph of 20,000 vertices and 120,000 edges from
// an LCG (vectors of adjacency, indexed from 1), with a binary heap of (distance, vertex) pairs written by hand and
// stale entries skipped; from 12 sources.
use std::time::Instant;

// The sum of the shortest distances from vertex src to every vertex.
fn dijkstra(src: usize, nv: usize, first: &[usize], to: &[usize], w: &[i64]) -> i64 {
    let ne = to.len() - 1;
    let mut dist = vec![-1i64; nv + 1];
    let mut done = vec![false; nv + 1];
    let mut hd = vec![0i64; ne + 2];
    let mut hv = vec![0usize; ne + 2];
    let (mut nh, mut sum) = (1usize, 0i64);
    dist[src] = 0;
    hd[1] = 0;
    hv[1] = src;
    while nh > 0 {
        let (d, u) = (hd[1], hv[1]);
        // pop: the last entry sifted down from the root
        let (x, y) = (hd[nh], hv[nh]);
        nh -= 1;
        let mut i = 1usize;
        loop {
            let mut c = i * 2;
            if c > nh {
                break;
            }
            if c < nh && hd[c + 1] < hd[c] {
                c += 1;
            }
            if hd[c] >= x {
                break;
            }
            hd[i] = hd[c];
            hv[i] = hv[c];
            i = c;
        }
        hd[i] = x;
        hv[i] = y;
        if done[u] {
            continue;
        }
        done[u] = true;
        sum += d;
        for e in first[u]..first[u + 1] {
            let v = to[e];
            let nd = d + w[e];
            if !done[v] && (dist[v] < 0 || nd < dist[v]) {
                dist[v] = nd;
                // push: sifted up from a new leaf
                nh += 1;
                let mut i = nh;
                while i > 1 && hd[i / 2] > nd {
                    hd[i] = hd[i / 2];
                    hv[i] = hv[i / 2];
                    i /= 2;
                }
                hd[i] = nd;
                hv[i] = v;
            }
        }
    }
    sum
}

fn main() {
    let t = Instant::now();
    let (nv, nout) = (20000usize, 6usize);
    let ne = nv * nout;
    let mut first = vec![0usize; nv + 2];
    let mut to = vec![0usize; ne + 1];
    let mut w = vec![0i64; ne + 1];
    let mut seed: i64 = 7;
    let mut e = 0usize;
    for i in 1..=nv {
        first[i] = e + 1;
        e += 1;
        to[e] = if i == nv { 1 } else { i + 1 };
        seed = seed * 16807 % 2147483647;
        w[e] = (seed / 256) % 100 + 1;
        for _ in 2..=nout {
            e += 1;
            seed = seed * 16807 % 2147483647;
            to[e] = ((seed / 16) % nv as i64) as usize + 1;
            seed = seed * 16807 % 2147483647;
            w[e] = (seed / 256) % 100 + 1;
        }
    }
    first[nv + 1] = e + 1;
    let mut total: i64 = 0;
    for src in 1..=12usize {
        total += dijkstra((src - 1) * 1600 + 1, nv, &first, &to, &w);
    }
    println!("dijkstra {} {}", t.elapsed().as_millis(), if total == 41762116 { "ok" } else { "WRONG" });
}
