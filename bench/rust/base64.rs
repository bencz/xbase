// bench/base64.prg in Rust: Base64 written out: 1,200,000 bytes from a fixed-seed generator encoded (three bytes to
// four characters through a table) and decoded back; 3 rounds (the .prg's strings of bytes as Vec<u8>, the encoded
// text a String).
use std::time::Instant;

const B64_TABLE: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

fn encode(c: &[u8]) -> String {
    let mut out = String::new();
    let len = c.len();
    let rest = len % 3;
    let ch = |k: usize| B64_TABLE[k] as char;
    for i in (0..len - rest).step_by(3) {
        let n = usize::from(c[i]) * 65536 + usize::from(c[i + 1]) * 256 + usize::from(c[i + 2]);
        out.push(ch(n / 262144));
        out.push(ch(n / 4096 % 64));
        out.push(ch(n / 64 % 64));
        out.push(ch(n % 64));
    }
    if rest == 1 {
        let n = usize::from(c[len - 1]);
        out.push(ch(n / 4));
        out.push(ch(n % 4 * 16));
        out.push_str("==");
    } else if rest == 2 {
        let n = usize::from(c[len - 2]) * 256 + usize::from(c[len - 1]);
        out.push(ch(n / 1024));
        out.push(ch(n / 16 % 64));
        out.push(ch(n % 16 * 4));
        out.push('=');
    }
    out
}

fn decode(value: &[usize; 256], c: &str) -> Vec<u8> {
    let c = c.as_bytes();
    let mut out: Vec<u8> = Vec::new();
    let pad = if c.ends_with(b"==") { 2 } else if c.ends_with(b"=") { 1 } else { 0 };
    for q in c.chunks(4) {
        let n = value[usize::from(q[0])] * 262144 + value[usize::from(q[1])] * 4096 + value[usize::from(q[2])] * 64
            + value[usize::from(q[3])];
        out.push((n / 65536) as u8);
        out.push((n / 256 % 256) as u8);
        out.push((n % 256) as u8);
    }
    out.truncate(out.len() - pad);
    out
}

fn main() {
    let mut seed: i64 = 64;
    let mut c: Vec<u8> = Vec::new();
    for _ in 0..1200000 {
        seed = (seed * 16807) % 2147483647;
        c.push((seed % 256) as u8);
    }
    let c = std::hint::black_box(c);
    let t = Instant::now();
    let mut value = [0usize; 256];
    for (i, &b) in B64_TABLE.iter().enumerate() {
        value[usize::from(b)] = i;
    }
    let mut ok = true;
    let mut n: i64 = 0;
    for r in 1..=3usize {
        let part = &c[..c.len() - r + 1];
        let enc = encode(part);
        let dec = decode(&value, &enc);
        ok = ok && dec == part;
        n += enc.len() as i64;
        for &b in enc.as_bytes().iter().step_by(97) {
            n += i64::from(b);
        }
    }
    ok = ok && n == 9038319 && encode(b"Ma") == "TWE=" && decode(&value, "TQ==") == b"M";
    println!("base64 {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
