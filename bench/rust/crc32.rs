// bench/crc32.prg in Rust: CRC-32 of 20,000 bytes (the table made each time, the data built in chunks), 100 times.
use std::time::Instant;

fn crc32_of_20k() -> u32 {
    let mut table = [0u32; 256];
    for i in 0..256u32 {
        let mut c = i;
        for _ in 0..8 { c = (c >> 1) ^ (0xEDB88320u32 & (0u32.wrapping_sub(c & 1))); }
        table[i as usize] = c;
    }
    let mut data: Vec<u8> = Vec::new();
    for i in (0..20000).step_by(100) {
        let chunk: Vec<u8> = (i..i + 100).map(|k| ((k * 31 + 7) % 251) as u8).collect();
        data.extend_from_slice(&chunk);
    }
    let mut crc = 0xFFFFFFFFu32;
    for &d in &data { crc = table[((crc ^ d as u32) & 0xFF) as usize] ^ (crc >> 8); }
    crc ^ 0xFFFFFFFF
}

fn main() {
    let t = Instant::now();
    let mut ok = true;
    for _ in 0..100 { ok = ok && std::hint::black_box(crc32_of_20k()) == 2954121016; }
    println!("crc32 {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
