// bench/bfs_dfs.prg in Rust: BFS from 6 sources and an iterative DFS over every component of an undirected graph of
// 50,000 vertices and 150,000 random edges from an LCG (kept as vectors of adjacency, indexed from 1), 2 times.
use std::time::Instant;

fn bfs(src: usize, nv: usize, first: &[usize], adj: &[usize]) -> i64 {
    let mut level = vec![-1i64; nv + 1];
    let mut queue = vec![0usize; nv + 1];
    let (mut head, mut tail, mut sum) = (1usize, 1usize, 0i64);
    level[src] = 0;
    queue[1] = src;
    while head <= tail {
        let u = queue[head];
        head += 1;
        let d = level[u];
        sum += d + 1;
        for e in first[u]..first[u + 1] {
            let v = adj[e];
            if level[v] < 0 {
                level[v] = d + 1;
                tail += 1;
                queue[tail] = v;
            }
        }
    }
    sum
}

fn dfs(nv: usize, first: &[usize], adj: &[usize]) -> i64 {
    let mut seen = vec![false; nv + 1];
    let mut stack = vec![0usize; 2 * (adj.len() - 1) + nv + 1];
    let (mut order, mut comps, mut sum) = (0i64, 0i64, 0i64);
    for s in 1..=nv {
        if seen[s] {
            continue;
        }
        comps += 1;
        let mut top = 1usize;
        stack[1] = s;
        while top > 0 {
            let u = stack[top];
            top -= 1;
            if seen[u] {
                continue;
            }
            seen[u] = true;
            order += 1;
            sum += u as i64 * order;
            for e in (first[u]..first[u + 1]).rev() {
                if !seen[adj[e]] {
                    top += 1;
                    stack[top] = adj[e];
                }
            }
        }
    }
    comps * 1000000000 + sum
}

fn main() {
    let t = Instant::now();
    let (nv, ne) = (50000usize, 150000usize);
    let mut seed: i64 = 3;
    let mut eu = vec![0usize; ne + 1];
    let mut ev = vec![0usize; ne + 1];
    let mut first = vec![0usize; nv + 2];
    for i in 1..=ne {
        seed = seed * 16807 % 2147483647;
        eu[i] = ((seed / 16) % nv as i64) as usize + 1;
        seed = seed * 16807 % 2147483647;
        ev[i] = ((seed / 16) % nv as i64) as usize + 1;
        first[eu[i]] += 1;
        first[ev[i]] += 1;
    }
    // degrees to the first slot of each vertex; first[ nv + 1 ] ends the last one
    let mut pos = 1usize;
    for f in first.iter_mut().skip(1) {
        let d = *f;
        *f = pos;
        pos += d;
    }
    let mut fill = first.clone();
    let mut adj = vec![0usize; 2 * ne + 1];
    for i in 1..=ne {
        adj[fill[eu[i]]] = ev[i];
        fill[eu[i]] += 1;
        adj[fill[ev[i]]] = eu[i];
        fill[ev[i]] += 1;
    }
    let mut total: i64 = 0;
    for _ in 0..2 {
        for src in 1..=6usize {
            total += bfs((src - 1) * 8000 + 1, nv, &first, &adj);
        }
        total += dfs(nv, &first, &adj);
    }
    println!("bfs_dfs {} {}", t.elapsed().as_millis(), if total == 62828511998050 { "ok" } else { "WRONG" });
}
