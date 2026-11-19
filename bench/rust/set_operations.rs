// bench/set_operations.prg in Rust: sets as HashMaps (each key its own value): two sets of 200,000 draws each from
// 0 .. 399,999, their union, intersection, difference and symmetric difference built as new maps; 2 rounds with new
// members.
use std::collections::HashMap;
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let mut seed: i64 = 777;
    let mut rand = |n: i64| {
        seed = (seed * 16807) % 2147483647;
        seed % n
    };
    let mut check: usize = 0;
    let mut ok = true;
    for _ in 0..2 {
        let mut ha: HashMap<i64, i64> = HashMap::new();
        let mut hb: HashMap<i64, i64> = HashMap::new();
        for _ in 0..200000 {
            let k = rand(400000);
            ha.insert(k, k);
            let k = rand(400000);
            hb.insert(k, k);
        }
        let mut union: HashMap<i64, i64> = HashMap::new();
        let mut inter: HashMap<i64, i64> = HashMap::new();
        let mut diff: HashMap<i64, i64> = HashMap::new();
        let mut sym: HashMap<i64, i64> = HashMap::new();
        for &k in ha.values() {
            union.insert(k, k);
            if hb.contains_key(&k) {
                inter.insert(k, k);
            } else {
                diff.insert(k, k);
                sym.insert(k, k);
            }
        }
        for &k in hb.values() {
            union.insert(k, k);
            if !ha.contains_key(&k) {
                sym.insert(k, k);
            }
        }
        check += ha.len() + hb.len() * 2 + union.len() * 3 + inter.len() * 5 + diff.len() * 7 + sym.len() * 11;
        ok = union.len() == 252664 && union.len() == inter.len() + sym.len() && ha.len() == inter.len() + diff.len();
    }
    ok = ok && check == 8609885;
    println!("set_operations {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
