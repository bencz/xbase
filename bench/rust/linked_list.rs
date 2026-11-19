// bench/linked_list.prg in Rust: a singly linked list of boxed nodes { value, next }: 1,000,000 nodes pushed at the
// head, walked, reversed in place, walked again, and every second node after the head unlinked; 3 rounds.
use std::time::Instant;

struct Node {
    value: i64,
    next: Option<Box<Node>>,
}

// Drops the list a node at a time (the default drop of a long Box chain recurses once per node).
fn free(mut head: Option<Box<Node>>) {
    while let Some(mut n) = head {
        head = n.next.take();
    }
}

fn main() {
    let t = Instant::now();
    let (mut sum, mut count, mut first) = (0i64, 0i64, 0i64);
    for _ in 0..3 {
        let mut head: Option<Box<Node>> = None;
        for i in 1..=std::hint::black_box(1000000i64) {
            head = Some(Box::new(Node { value: i, next: head.take() }));
        }
        let mut p = head.as_deref();
        while let Some(n) = p {
            sum += n.value;
            p = n.next.as_deref();
        }
        let mut prev: Option<Box<Node>> = None;
        let mut cur = head.take();
        while let Some(mut n) = cur {
            cur = n.next.take();
            n.next = prev;
            prev = Some(n);
        }
        head = prev;
        first += head.as_ref().map_or(0, |n| n.value);
        let mut p = head.as_deref_mut();
        while let Some(n) = p {
            if let Some(mut gone) = n.next.take() {
                n.next = gone.next.take();
            }
            p = n.next.as_deref_mut();
        }
        let mut p = head.as_deref();
        while let Some(n) = p {
            count += 1;
            p = n.next.as_deref();
        }
        free(head);
    }
    let ok = sum == 1500001500000 && first == 3 && count == 1500000;
    println!("linked_list {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
