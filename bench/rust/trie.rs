// bench/trie.prg in Rust: a trie of HashMaps, one per node, from a letter to the node { children, words }: 100,000
// words of 3 to 10 letters from an 8-letter alphabet inserted, then 100,000 other words of the same kind looked up,
// as words and as prefixes of the words stored.
use std::collections::HashMap;
use std::time::Instant;

struct Node {
    children: HashMap<u8, Box<Node>>,
    words: i64,
}

fn new_node() -> Box<Node> {
    Box::new(Node { children: HashMap::new(), words: 0 })
}

fn main() {
    let mut seed: i64 = 2718;
    let mut rand = |n: i64| { seed = seed * 16807 % 2147483647; seed % n };
    let mut new_word = || {
        let n = rand(8) + 3;
        let mut w = String::new();
        for _ in 0..n {
            let k = rand(8) + 1;
            w.push(char::from(b"etaoinsr"[rand(k) as usize]));
        }
        w
    };
    let mut words = Vec::with_capacity(100000);
    let mut probes = Vec::with_capacity(100000);
    for _ in 0..100000 {
        words.push(new_word());
        probes.push(new_word());
    }
    let t = Instant::now();
    let mut root = new_node();
    let (mut nodes, mut found, mut prefix) = (1i64, 0i64, 0i64);
    for w in &words {
        let mut node = &mut root;
        for c in w.bytes() {
            node = node.children.entry(c).or_insert_with(|| {
                nodes += 1;
                new_node()
            });
        }
        node.words += 1;
    }
    for w in &probes {
        let mut node = Some(&root);
        for c in w.bytes() {
            node = node.and_then(|n| n.children.get(&c));
            if node.is_none() {
                break;
            }
        }
        if let Some(n) = node {
            prefix += 1;
            if n.words > 0 {
                found += 1;
            }
        }
    }
    let ok = nodes == 159557 && found == 40767 && prefix == 50841;
    println!("trie {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
