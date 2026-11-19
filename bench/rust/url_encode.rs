// bench/url_encode.prg in Rust: percent-encoding written by hand over byte strings (Vec<u8>: the text holds bytes
// above 127 that are not UTF-8): 20,000 strings of 20 to 80 bytes encoded as in a URL query (unreserved bytes kept, a
// blank as +, any other byte as %XX) and decoded back; 8 rounds.
use std::time::Instant;

const HEX_DIGITS: &[u8] = b"0123456789ABCDEF";
const UNRESERVED: &[u8] = b"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~";

fn rand(seed: &mut i64, n: i64) -> i64 {
    *seed = (*seed * 16807) % 2147483647;
    *seed % n
}

#[inline(never)]
fn url_encode(c: &[u8]) -> Vec<u8> {
    let mut out = Vec::new();
    for &x in c {
        if UNRESERVED.contains(&x) {
            out.push(x);
        } else if x == b' ' {
            out.push(b'+');
        } else {
            out.push(b'%');
            out.push(HEX_DIGITS[(x / 16) as usize]);
            out.push(HEX_DIGITS[(x % 16) as usize]);
        }
    }
    out
}

fn hex(x: u8) -> u8 { HEX_DIGITS.iter().position(|&h| h == x).unwrap_or(0) as u8 }

#[inline(never)]
fn url_decode(c: &[u8]) -> Vec<u8> {
    let mut out = Vec::new();
    let mut i = 0;
    while i < c.len() {
        let x = c[i];
        if x == b'%' {
            out.push(hex(c[i + 1]) * 16 + hex(c[i + 2]));
            i += 3;
        } else {
            out.push(if x == b'+' { b' ' } else { x });
            i += 1;
        }
    }
    out
}

fn main() {
    let mut seed: i64 = 80;
    let mut chars: Vec<u8> = b"abcdefghijklmnopqrstuvwxyzABCDEFGHIJ0123456789     -_.~&=/?%:#+".to_vec();
    chars.extend_from_slice(&[233, 252]);
    let mut a: Vec<Vec<u8>> = Vec::with_capacity(20000);
    for _ in 0..20000 {
        let mut c = Vec::new();
        // FOR j := 1 TO 20 + Rand( 61 ): the limit is evaluated again before every step, as xBase does.
        let mut j = 1;
        while j <= 20 + rand(&mut seed, 61) {
            c.push(chars[rand(&mut seed, chars.len() as i64) as usize]);
            j += 1;
        }
        a.push(c);
    }
    let a = std::hint::black_box(a);
    let t = Instant::now();
    let mut n: i64 = 0;
    let mut ok = true;
    for _ in 0..8 {
        for c in &a {
            let enc = url_encode(c);
            n += enc.len() as i64 + enc.last().map_or(0, |&b| b as i64);
            ok = ok && url_decode(&enc) == *c;
        }
    }
    ok = ok && n == 18744664 && url_encode(&[b'a', b' ', b'b', 38, b'c', b'=', 233]) == b"a+b%26c%3D%E9";
    println!("url_encode {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
