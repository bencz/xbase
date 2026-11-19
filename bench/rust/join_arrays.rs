// bench/join_arrays.prg in Rust: joins of two Vecs of records: 20,000 customers (id, name, segment, city) and 500,000
// orders (id, customer, amount) made from a fixed seed (a third of the orders name a customer that exists). Timed: a
// hash join (the customers in a HashMap by id, each order looked up; amounts summed per segment, orders counted per
// city in a HashMap), then a sort-merge join (the orders' customer and position packed in one integer key, sorted
// with the library's sort, merged with the customers in id order; the amounts weighted by segment).
use std::collections::HashMap;
use std::time::Instant;

// id, name, segment, city
type Customer = (i64, String, i64, String);

fn main() {
    let mut seed: i64 = 2718;
    let mut rand = |n: i64| {
        seed = seed * 16807 % 2147483647;
        seed % n
    };
    let mut cust: Vec<Customer> = Vec::with_capacity(20000);
    for i in 1..=20000i64 {
        let segment = rand(5) + 1;
        let city = format!("CITY{:03}", rand(100) + 1);
        cust.push((i * 3, format!("Customer {}", i), segment, city));
    }
    let mut ord: Vec<(i64, i64, i64)> = Vec::with_capacity(500000);
    for i in 1..=500000i64 {
        let c = rand(60000) + 1;
        let amount = rand(100000) + 1;
        ord.push((i, c, amount));
    }
    let t = Instant::now();

    let mut h_cust: HashMap<i64, &Customer> = HashMap::new();
    for c in &cust {
        h_cust.insert(c.0, c);
    }
    let mut seg = [0i64; 5];
    let mut h_city: HashMap<String, i64> = HashMap::new();
    let mut matched: i64 = 0;
    for o in &ord {
        if let Some(c) = h_cust.get(&o.1) {
            seg[(c.2 - 1) as usize] += o.2;
            if let Some(v) = h_city.get_mut(&c.3) {
                *v += 1;
            } else {
                h_city.insert(c.3.clone(), 1);
            }
            matched += 1;
        }
    }
    let mut best: Option<&String> = None;
    let mut n_best: i64 = 0;
    for (k, &n) in &h_city {
        if n > n_best || (n == n_best && best.is_some_and(|b| k < b)) {
            best = Some(k);
            n_best = n;
        }
    }

    let mut keys: Vec<i64> = ord.iter().enumerate().map(|(i, o)| o.1 * 1000000 + i as i64 + 1).collect();
    keys.sort();
    let mut pos = 0usize;
    let mut w: i64 = 0;
    let mut merged: i64 = 0;
    for &k in &keys {
        let o = &ord[(k % 1000000 - 1) as usize];
        while pos < cust.len() && cust[pos].0 < o.1 {
            pos += 1;
        }
        if pos < cust.len() && cust[pos].0 == o.1 {
            w += o.2 * cust[pos].2;
            merged += 1;
        }
    }

    let ok = matched == 166641 && merged == 166641 && w == 25122891088 && seg[0] == 1626943956
        && seg[1] == 1648544230 && seg[2] == 1673211091 && seg[3] == 1694374411 && seg[4] == 1680345551
        && best.is_some_and(|b| b == "CITY080") && n_best == 1980 && h_city.len() == 100;
    println!("join_arrays {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
