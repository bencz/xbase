// bench/binary_trees.prg in Rust: binary trees made and walked, depths 4 to 14 (a stretch tree of depth 15, one of
// depth 14 that lives throughout, 2^(14 - d + 4) trees of each even depth d), each node a Box freed when its tree is
// dropped.
use std::hint::black_box;
use std::time::Instant;

struct Node {
    left: Option<Box<Node>>,
    right: Option<Box<Node>>,
}

fn make(d: i64) -> Box<Node> {
    if d == 0 {
        Box::new(Node { left: None, right: None })
    } else {
        Box::new(Node { left: Some(make(d - 1)), right: Some(make(d - 1)) })
    }
}

// The nodes of the tree.
fn check(n: &Node) -> i64 {
    match (&n.left, &n.right) {
        (Some(l), Some(r)) => 1 + check(l) + check(r),
        _ => 1,
    }
}

fn main() {
    let t = Instant::now();
    let max_depth: i64 = black_box(14);
    let mut total = check(&make(max_depth + 1));
    let long_lived = make(max_depth);
    for d in (4..=max_depth).step_by(2) {
        let iters = 1i64 << (max_depth - d + 4);
        for _ in 0..iters {
            total += check(&make(d));
        }
    }
    total += check(&long_lived);
    println!("binary_trees {} {}", t.elapsed().as_millis(), if total == 3222190 { "ok" } else { "WRONG" });
}
