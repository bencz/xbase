// bench/lru_cache.prg in Rust: a least-recently-used cache of 2,000 entries: a HashMap from key to node and a doubly
// linked list of nodes { key, value, prev, next } in order of use, the nodes in a vector linked by index (an evicted
// node's slot takes the new node); 2,000,000 reads of keys skewed toward small numbers, a miss computing the value and
// evicting the oldest entry when the cache is full.
use std::collections::HashMap;
use std::time::Instant;

struct Node {
    key: i64,
    value: i64,
    prev: usize,
    next: usize,
}

const HEAD: usize = 0;
const TAIL: usize = 1;

fn rand(seed: &mut i64, n: i64) -> i64 {
    *seed = (*seed * 16807) % 2147483647;
    *seed % n
}

fn main() {
    let t = Instant::now();
    let mut seed: i64 = 1234;
    let cap = 2000usize;
    let mut h: HashMap<i64, usize> = HashMap::new();
    let mut nodes: Vec<Node> = vec![
        Node { key: 0, value: 0, prev: HEAD, next: TAIL },
        Node { key: 0, value: 0, prev: HEAD, next: TAIL },
    ];
    let (mut hits, mut sum) = (0i64, 0i64);
    for _ in 0..2000000 {
        let k = rand(&mut seed, 20000) + 1;
        let k = rand(&mut seed, k);
        let node;
        if let Some(&n) = h.get(&k) {
            hits += 1;
            node = n;
            let (p, x) = (nodes[n].prev, nodes[n].next);
            nodes[p].next = x;
            nodes[x].prev = p;
        } else {
            let fresh = Node { key: k, value: k * 3 + 1, prev: HEAD, next: TAIL };
            if h.len() >= cap {
                let old = nodes[TAIL].prev;
                let p = nodes[old].prev;
                nodes[p].next = TAIL;
                nodes[TAIL].prev = p;
                h.remove(&nodes[old].key);
                nodes[old] = fresh;
                node = old;
            } else {
                nodes.push(fresh);
                node = nodes.len() - 1;
            }
            h.insert(k, node);
        }
        let first = nodes[HEAD].next;
        nodes[node].next = first;
        nodes[node].prev = HEAD;
        nodes[first].prev = node;
        nodes[HEAD].next = node;
        sum += nodes[node].value;
    }
    let ok = hits == 378837 && sum == 30033837356 && h.len() == cap;
    println!("lru_cache {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
