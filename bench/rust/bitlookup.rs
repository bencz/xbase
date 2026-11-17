// bench/bitlookup.prg in Rust: the bitwise AND from a table of the ANDs of two nibbles (made as the C program's macros
// make it), IOR and XOR written out as the .prg's macros expand them, for every pair a, b in 0 .. 999: 7 calls of and()
// a pair, which the compiler may neither inline nor merge (inline(never), arguments through black_box), as xbase calls
// its function.
use std::hint::black_box;
use std::time::Instant;

fn table() -> [u16; 256] {
    let mut t = [0u16; 256];
    for a in 0..16u16 { for b in 0..16u16 { t[(a * 16 + b) as usize] = a & b; } }
    t
}

#[inline(never)]
fn and(t: &[u16; 256], mut a: i32, mut b: i32) -> i32 {
    let mut r: i32 = 0;
    for _ in 0..4 {
        r = r / 16 + i32::from(t[((a % 16) * 16 + (b % 16)) as usize]) * 4096;
        a /= 16;
        b /= 16;
    }
    r
}
// AND() through black_box: each call is made.
fn and_(t: &[u16; 256], a: i32, b: i32) -> i32 { and(black_box(t), black_box(a), black_box(b)) }

fn main() {
    let t0 = Instant::now();
    let t = black_box(table());
    let mut ok = true;
    'outer: for a in 0..1000i32 {
        for b in 0..1000i32 {
            let ior = (a + b - 2 * and_(&t, a, b)) + and_(&t, a, b)
                - 2 * and_(&t, a + b - 2 * and_(&t, a, b), and_(&t, a, b));
            if and_(&t, a, b) != a & b || ior != a | b || a + b - 2 * and_(&t, a, b) != a ^ b {
                ok = false;
                break 'outer;
            }
        }
    }
    println!("bitlookup {} {}", t0.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
