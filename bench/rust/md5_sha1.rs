// bench/md5_sha1.prg in Rust: MD5 and SHA-1 written by hand (32-bit words in 64-bit integers, masked after each sum)
// of a 200,000-byte buffer from an LCG, 40 times each, padded as the standards say; first checked against the
// published digests of "" and "abc".
use std::time::Instant;

const MASK: i64 = 0xFFFFFFFF;

const MD5_K: [i64; 64] = [
    0xD76AA478, 0xE8C7B756, 0x242070DB, 0xC1BDCEEE, 0xF57C0FAF, 0x4787C62A, 0xA8304613, 0xFD469501,
    0x698098D8, 0x8B44F7AF, 0xFFFF5BB1, 0x895CD7BE, 0x6B901122, 0xFD987193, 0xA679438E, 0x49B40821,
    0xF61E2562, 0xC040B340, 0x265E5A51, 0xE9B6C7AA, 0xD62F105D, 0x02441453, 0xD8A1E681, 0xE7D3FBC8,
    0x21E1CDE6, 0xC33707D6, 0xF4D50D87, 0x455A14ED, 0xA9E3E905, 0xFCEFA3F8, 0x676F02D9, 0x8D2A4C8A,
    0xFFFA3942, 0x8771F681, 0x6D9D6122, 0xFDE5380C, 0xA4BEEA44, 0x4BDECFA9, 0xF6BB4B60, 0xBEBFBC70,
    0x289B7EC6, 0xEAA127FA, 0xD4EF3085, 0x04881D05, 0xD9D4D039, 0xE6DB99E5, 0x1FA27CF8, 0xC4AC5665,
    0xF4292244, 0x432AFF97, 0xAB9423A7, 0xFC93A039, 0x655B59C3, 0x8F0CCC92, 0xFFEFF47D, 0x85845DD1,
    0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1, 0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391,
];
const MD5_R: [i64; 16] = [7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21];

fn rotl(x: i64, s: i64) -> i64 { ((x << s) | (x >> (32 - s))) & MASK }

// The message, a 0x80 byte, zeros up to 56 bytes modulo 64, then its length in bits in 8 bytes, the least significant
// first when little.
fn pad(s: &[u8], little: bool) -> Vec<u8> {
    let n = s.len();
    let len = (n + 8) / 64 * 64 + 64;
    let mut p = vec![0u8; len];
    p[..n].copy_from_slice(s);
    p[n] = 0x80;
    for i in 0..8 {
        p[len - 8 + if little { i } else { 7 - i }] = (((n as i64 * 8) >> (8 * i)) & 0xFF) as u8;
    }
    p
}

// The words as lowercase hexadecimal, each word's bytes the least significant first when little.
fn hex(w: &[i64], little: bool) -> String {
    let digits = b"0123456789abcdef";
    let mut out = String::new();
    for &x in w {
        for b in 0..4 {
            let byte = (x >> if little { 8 * b } else { 24 - 8 * b }) & 0xFF;
            out.push(digits[(byte >> 4) as usize] as char);
            out.push(digits[(byte & 15) as usize] as char);
        }
    }
    out
}

fn md5_of(s: &[u8]) -> String {
    let p = pad(s, true);
    let mut h: [i64; 4] = [0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476];
    let mut m = [0i64; 16];
    for blk in p.chunks(64) {
        for j in 0..16 {
            let q = 4 * j;
            m[j] = blk[q] as i64 | (blk[q + 1] as i64) << 8 | (blk[q + 2] as i64) << 16 | (blk[q + 3] as i64) << 24;
        }
        let (mut a, mut b, mut c, mut d) = (h[0], h[1], h[2], h[3]);
        for i in 0..64usize {
            let (f, g) = if i < 16 {
                ((b & c) | ((b ^ MASK) & d), i)
            } else if i < 32 {
                ((d & b) | ((d ^ MASK) & c), (5 * i + 1) & 15)
            } else if i < 48 {
                (b ^ c ^ d, (3 * i + 5) & 15)
            } else {
                (c ^ (b | (d ^ MASK)), (7 * i) & 15)
            };
            let tmp = d;
            d = c;
            c = b;
            b = (b + rotl((a + f + MD5_K[i] + m[g]) & MASK, MD5_R[(i >> 4) * 4 + (i & 3)])) & MASK;
            a = tmp;
        }
        h[0] = (h[0] + a) & MASK;
        h[1] = (h[1] + b) & MASK;
        h[2] = (h[2] + c) & MASK;
        h[3] = (h[3] + d) & MASK;
    }
    hex(&h, true)
}

fn sha1_of(s: &[u8]) -> String {
    let p = pad(s, false);
    let mut h: [i64; 5] = [0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0];
    let mut w = [0i64; 80];
    for blk in p.chunks(64) {
        for j in 0..16 {
            let q = 4 * j;
            w[j] = (blk[q] as i64) << 24 | (blk[q + 1] as i64) << 16 | (blk[q + 2] as i64) << 8 | blk[q + 3] as i64;
        }
        for j in 16..80 {
            w[j] = rotl(w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16], 1);
        }
        let (mut a, mut b, mut c, mut d, mut e) = (h[0], h[1], h[2], h[3], h[4]);
        for i in 0..80 {
            let (f, k) = if i < 20 {
                ((b & c) | ((b ^ MASK) & d), 0x5A827999)
            } else if i < 40 {
                (b ^ c ^ d, 0x6ED9EBA1)
            } else if i < 60 {
                ((b & c) | (b & d) | (c & d), 0x8F1BBCDC)
            } else {
                (b ^ c ^ d, 0xCA62C1D6)
            };
            let tmp = (rotl(a, 5) + f + e + k + w[i]) & MASK;
            e = d;
            d = c;
            c = rotl(b, 30);
            b = a;
            a = tmp;
        }
        h[0] = (h[0] + a) & MASK;
        h[1] = (h[1] + b) & MASK;
        h[2] = (h[2] + c) & MASK;
        h[3] = (h[3] + d) & MASK;
        h[4] = (h[4] + e) & MASK;
    }
    hex(&h, false)
}

fn main() {
    let t = Instant::now();
    let n = std::hint::black_box(200000);
    let mut ok = md5_of(b"") == "d41d8cd98f00b204e9800998ecf8427e"
        && md5_of(b"abc") == "900150983cd24fb0d6963f7d28e17f72"
        && sha1_of(b"") == "da39a3ee5e6b4b0d3255bfef95601890afd80709"
        && sha1_of(b"abc") == "a9993e364706816aba3e25717850c26c9cd0d89d";
    let mut seed: i64 = 7;
    let mut data: Vec<u8> = Vec::new();
    for _ in 0..n {
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        data.push(((seed >> 16) & 0xFF) as u8);
    }
    for _ in 0..40 {
        ok = ok && md5_of(&data) == "9caadd7dbbe1174da261d335b92f85fb"
            && sha1_of(&data) == "cec3d79455d923d234a1a40741be135a4ba980e2";
    }
    println!("md5_sha1 {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
