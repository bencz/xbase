// bench/index.prg in Rust: a table of 200,000 records with random 10-digit keys, an order built on them, 100,000
// random seeks, a pass through the order, 20,000 records appended with the order kept up to date. Std has no DBF or
// NTX: the records are fixed-width (a deletion flag, K of 10 characters, N of 10) in "bench_index.dat", a file the
// program creates in its current directory (written through a buffer, read back a record at a time by position) and
// removes at the end; the order is a BTreeSet of (key, record number) kept in memory.
use std::collections::BTreeSet;
use std::fs::{self, File, OpenOptions};
use std::io::{BufReader, BufWriter, Read, Seek, SeekFrom, Write};
use std::time::Instant;

const REC_LEN: u64 = 21;
const FILE_NAME: &str = "bench_index.dat";

fn record(k: &str, n: i64) -> String {
    format!(" {:<10}{:>10}", k, n)
}

// The key K of record recno (1-based), read from the file.
fn read_key(f: &mut File, recno: u64, buf: &mut [u8; REC_LEN as usize]) -> String {
    f.seek(SeekFrom::Start((recno - 1) * REC_LEN)).unwrap();
    f.read_exact(buf).unwrap();
    String::from_utf8_lossy(&buf[1..11]).into_owned()
}

fn main() {
    let t = Instant::now();
    let mut seed: i64 = 7;
    let mut rand = |n: i64| { seed = (seed * 1103515245 + 12345) % 2147483648; seed / 65536 % n };
    let recs: u64 = 200000;
    let mut ok = true;
    {
        let mut w = BufWriter::new(File::create(FILE_NAME).unwrap());
        for i in 1..=recs {
            w.write_all(record(&format!("{:010}", rand(100000000)), i as i64).as_bytes()).unwrap();
        }
        w.flush().unwrap();
    }
    let mut keys: Vec<(String, u64)> = Vec::with_capacity(recs as usize);
    {
        let mut r = BufReader::new(File::open(FILE_NAME).unwrap());
        let mut buf = [0u8; REC_LEN as usize];
        for recno in 1..=recs {
            r.read_exact(&mut buf).unwrap();
            keys.push((String::from_utf8_lossy(&buf[1..11]).into_owned(), recno));
        }
    }
    keys.sort();
    let mut order: BTreeSet<(String, u64)> = keys.into_iter().collect();
    let mut hits = 0i64;
    for _ in 0..100000 {
        let k = format!("{:010}", rand(100000000));
        // a soft seek: the first entry not below the key; a hit when its key is the one sought
        if order.range((k.clone(), 0)..).next().is_some_and(|e| e.0 == k) {
            hits += 1;
        }
    }
    let mut f = OpenOptions::new().read(true).write(true).open(FILE_NAME).unwrap();
    let mut buf = [0u8; REC_LEN as usize];
    let mut prev = String::new();
    let mut count: u64 = 0;
    for e in &order {
        let k = read_key(&mut f, e.1, &mut buf);
        ok = ok && k >= prev;
        prev = k;
        count += 1;
    }
    ok = ok && count == recs;
    for i in 1..=20000u64 {
        let k = format!("{:010}", rand(100000000));
        f.seek(SeekFrom::Start((recs + i - 1) * REC_LEN)).unwrap();
        f.write_all(record(&k, (recs + i) as i64).as_bytes()).unwrap();
        order.insert((k, recs + i));
    }
    count = 0;
    for e in &order {
        read_key(&mut f, e.1, &mut buf);
        count += 1;
    }
    ok = ok && count == recs + 20000 && hits >= 0;
    drop(f);
    fs::remove_file(FILE_NAME).unwrap();
    println!("index {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
