// bench/pidigits.prg in Rust: arbitrary precision in Vecs: the first 1,500 digits of pi from Gibbons' unbounded spigot,
// its numbers kept as Vecs of 30-bit limbs (the least significant first; the accumulator's sign apart), with
// multiplication by a small number, addition and subtraction of a multiple, comparison, and a quotient below 2^30
// estimated from the leading limbs with doubles, then corrected (a hand-written bignum: the standard library has
// none). Checked: the first and the last 50 digits and their sum.
use std::cmp::Ordering;
use std::time::Instant;

const BITS: u32 = 30;
const LIMB_MASK: i64 = 0x3FFFFFFF;
const RADIX: f64 = 1073741824.0;

// Drops the leading zero limbs (keeps one).
fn normalize(a: &mut Vec<i64>) {
    let mut n = a.len();
    while n > 1 && a[n - 1] == 0 {
        n -= 1;
    }
    a.truncate(n);
}

// a *= m
fn mul_small(a: &mut Vec<i64>, m: i64) {
    let mut carry: i64 = 0;
    for v in a.iter_mut() {
        let x = *v * m + carry;
        *v = x & LIMB_MASK;
        carry = x >> BITS;
    }
    while carry > 0 {
        a.push(carry & LIMB_MASK);
        carry >>= BITS;
    }
    normalize(a);
}

// a += b * m
fn add_mul(a: &mut Vec<i64>, b: &[i64], m: i64) {
    if a.len() < b.len() {
        a.resize(b.len(), 0);
    }
    let mut carry: i64 = 0;
    let mut i = 0;
    while i < b.len() {
        let x = a[i] + b[i] * m + carry;
        a[i] = x & LIMB_MASK;
        carry = x >> BITS;
        i += 1;
    }
    while carry > 0 && i < a.len() {
        let x = a[i] + carry;
        a[i] = x & LIMB_MASK;
        carry = x >> BITS;
        i += 1;
    }
    while carry > 0 {
        a.push(carry & LIMB_MASK);
        carry >>= BITS;
    }
}

// a -= b * m, when b * m <= a
fn sub_mul(a: &mut Vec<i64>, b: &[i64], m: i64) {
    let mut borrow: i64 = 0;
    let mut i = 0;
    while i < b.len() {
        let x = a[i] - b[i] * m - borrow;
        a[i] = x & LIMB_MASK;
        borrow = -(x >> BITS);
        i += 1;
    }
    while borrow > 0 {
        let x = a[i] - borrow;
        a[i] = x & LIMB_MASK;
        borrow = -(x >> BITS);
        i += 1;
    }
    normalize(a);
}

fn cmp(a: &[i64], b: &[i64]) -> Ordering {
    if a.len() != b.len() {
        return a.len().cmp(&b.len());
    }
    for i in (0..a.len()).rev() {
        if a[i] != b[i] {
            return a[i].cmp(&b[i]);
        }
    }
    Ordering::Equal
}

// a += b * m (m >= 0), neg the sign of a, term_neg that of the term.
fn add_signed(a: &mut Vec<i64>, neg: &mut bool, b: &[i64], m: i64, term_neg: bool) {
    if *neg == term_neg {
        add_mul(a, b, m);
    } else {
        let mut tmp = b.to_vec();
        mul_small(&mut tmp, m);
        if cmp(a, &tmp) != Ordering::Less {
            sub_mul(a, &tmp, 1);
        } else {
            sub_mul(&mut tmp, a, 1);
            *a = tmp;
            *neg = term_neg;
        }
    }
    if a.len() == 1 && a[0] == 0 {
        *neg = false;
    }
}

// The limb at i (from 0) as a double, 0 past either end.
fn top(a: &[i64], i: i64) -> f64 {
    if i >= 0 && (i as usize) < a.len() { a[i as usize] as f64 } else { 0.0 }
}

// Int( x / d ) for a quotient below 2^30: estimated from the three leading limbs, then corrected.
fn quotient(x: &[i64], d: &[i64]) -> i64 {
    let h = x.len().max(d.len()) as i64 - 1;
    let fx = (top(x, h) * RADIX + top(x, h - 1)) * RADIX + top(x, h - 2);
    let fd = (top(d, h) * RADIX + top(d, h - 1)) * RADIX + top(d, h - 2);
    let mut q = (fx / fd) as i64;
    let mut tmp = d.to_vec();
    mul_small(&mut tmp, q);
    while cmp(&tmp, x) == Ordering::Greater {
        sub_mul(&mut tmp, d, 1);
        q -= 1;
    }
    add_mul(&mut tmp, d, 1);
    while cmp(&tmp, x) != Ordering::Greater {
        add_mul(&mut tmp, d, 1);
        q += 1;
    }
    q
}

fn main() {
    let t = Instant::now();
    let n = std::hint::black_box(1500usize);
    let mut digits = String::new();
    let mut neg = false;
    let mut k: i64 = 0;
    let mut numer: Vec<i64> = vec![1];
    let mut accum: Vec<i64> = vec![0];
    let mut denom: Vec<i64> = vec![1];
    while digits.len() < n {
        k += 1;
        add_signed(&mut accum, &mut neg, &numer, 2, false);
        mul_small(&mut accum, 2 * k + 1);
        mul_small(&mut denom, 2 * k + 1);
        mul_small(&mut numer, k);
        if neg || cmp(&numer, &accum) == Ordering::Greater {
            continue;
        }
        let mut x = accum.clone();
        let mut x_neg = neg;
        add_signed(&mut x, &mut x_neg, &numer, 3, false);
        let d = quotient(&x, &denom);
        add_signed(&mut x, &mut x_neg, &numer, 1, false);
        if d != quotient(&x, &denom) {
            continue;
        }
        digits.push((b'0' + d as u8) as char);
        add_signed(&mut accum, &mut neg, &denom, d, true);
        mul_small(&mut accum, 10);
        mul_small(&mut numer, 10);
    }
    let sum: i64 = digits.bytes().map(|c| i64::from(c - b'0')).sum();
    let ok = digits.starts_with("31415926535897932384626433832795028841971693993751")
        && digits.ends_with("55570674983850549458858692699569092721079750930295")
        && sum == 6775;
    println!("pidigits {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
