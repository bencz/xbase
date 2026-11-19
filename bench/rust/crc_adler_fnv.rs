// bench/crc_adler_fnv.prg in Rust: checksums of bytes: twenty buffers of 1,000,000 bytes from an LCG, built a byte at
// a time (a Vec<u8>, the .prg's string of bytes), each hashed with CRC-32 (from a table), Adler-32 and FNV-1a
// (32 bits); the functions are first checked against their published values ("123456789", "Wikipedia", "a").
use std::time::Instant;

fn crc32_of(table: &[i64; 256], s: &[u8]) -> i64 {
    let mut crc: i64 = 0xFFFFFFFF;
    for &b in s {
        crc = table[((crc ^ i64::from(b)) & 0xFF) as usize] ^ (crc >> 8);
    }
    crc ^ 0xFFFFFFFF
}

fn adler32_of(s: &[u8]) -> i64 {
    let mut a: i64 = 1;
    let mut b: i64 = 0;
    for &c in s {
        a += i64::from(c);
        if a >= 65521 {
            a -= 65521;
        }
        b += a;
        if b >= 65521 {
            b -= 65521;
        }
    }
    (b << 16) | a
}

fn fnv1a_of(s: &[u8]) -> i64 {
    let mut h: i64 = 2166136261;
    for &c in s {
        h = ((h ^ i64::from(c)) * 16777619) & 0xFFFFFFFF;
    }
    h
}

fn main() {
    let t = Instant::now();
    let n = std::hint::black_box(1000000);
    let mut seed: i64 = 42;
    let mut total: i64 = 0;
    let mut table = [0i64; 256];
    for (i, e) in table.iter_mut().enumerate() {
        let mut c = i as i64;
        for _ in 0..8 {
            c = (c >> 1) ^ (0xEDB88320 & -(c & 1));
        }
        *e = c;
    }
    let ok = crc32_of(&table, b"123456789") == 0xCBF43926 && adler32_of(b"Wikipedia") == 0x11E60398
        && fnv1a_of(b"a") == 0xE40C292C;
    for _ in 0..20 {
        let mut data: Vec<u8> = Vec::new();
        for _ in 0..n {
            seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
            data.push(((seed >> 16) & 0xFF) as u8);
        }
        total += crc32_of(&table, &data) + adler32_of(&data) + fnv1a_of(&data);
    }
    let ok = ok && total == 142519858956;
    println!("crc_adler_fnv {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
