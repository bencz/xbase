// bench/sequential_scan.prg in Rust: sequential scans of a table: 200,000 sales records (region, amount, quantity,
// customer, date) made from a fixed seed, then timed, 4 rounds of six passes over every record: a loop over all of
// them, a sum, a count with a condition, a sum with a condition, a search with its continuations, and a sum over a
// date range. No DBF here: the records are written to "bench_seqscan.dat" in the current directory, as fixed-width
// text records with the DBF's field widths (a deletion flag, region 1, amount 10, quantity 3, customer 10, date 8 as
// YYYYMMDD, numbers right-aligned), and each pass reads them back from the file (buffered) and decodes the fields it
// needs. The file is deleted at the end.
use std::fs::File;
use std::io::{BufReader, BufWriter, Read, Seek, SeekFrom, Write};
use std::time::Instant;

const PATH: &str = "bench_seqscan.dat";
const REC_LEN: usize = 33;
const REGION: (usize, usize) = (1, 1);
const AMOUNT: (usize, usize) = (2, 10);
const QTY: (usize, usize) = (12, 3);
const CUST: (usize, usize) = (15, 10);
const SALEDATE: (usize, usize) = (25, 8);

// A numeric field's value: its digits after the leading blanks.
fn num(rec: &[u8], f: (usize, usize)) -> i64 {
    rec[f.0..f.0 + f.1].iter().filter(|c| c.is_ascii_digit()).fold(0, |n, c| n * 10 + i64::from(c - b'0'))
}

fn field(rec: &[u8], f: (usize, usize)) -> &[u8] {
    &rec[f.0..f.0 + f.1]
}

// The civil date (year, month, day) of a day count from 1970-01-01.
fn civil(days: i64) -> (i64, i64, i64) {
    let z = days + 719468;
    let era = z.div_euclid(146097);
    let doe = z - era * 146097;
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = doy - (153 * mp + 2) / 5 + 1;
    let m = if mp < 10 { mp + 3 } else { mp - 9 };
    (yoe + era * 400 + if m <= 2 { 1 } else { 0 }, m, d)
}

// Every record from the first, with its 1-based record number, as a pass of the RDD reads them.
fn pass(file: &mut File, f: &mut dyn FnMut(i64, &[u8])) {
    file.seek(SeekFrom::Start(0)).unwrap();
    let mut r = BufReader::new(&*file);
    let mut rec = [0u8; REC_LEN];
    let mut recno = 0;
    while r.read_exact(&mut rec).is_ok() {
        recno += 1;
        f(recno, &rec);
    }
}

fn scans(file: &mut File) -> bool {
    let (mut sum_r3, mut cnt_r3, mut all, mut big, mut qty) = (0i64, 0i64, 0i64, 0i64, 0i64);
    let (mut loc, mut loc_ids, mut jan) = (0i64, 0i64, 0i64);
    pass(file, &mut |_, r| {
        if num(r, REGION) == 3 {
            sum_r3 += num(r, AMOUNT);
            cnt_r3 += 1;
        }
    });
    pass(file, &mut |_, r| all += num(r, AMOUNT));
    pass(file, &mut |_, r| {
        if num(r, AMOUNT) > 900000 {
            big += 1;
        }
    });
    pass(file, &mut |_, r| {
        if num(r, REGION) <= 2 {
            qty += num(r, QTY);
        }
    });
    pass(file, &mut |recno, r| {
        if field(r, CUST).starts_with(b"CUST0001") {
            loc += 1;
            loc_ids += recno;
        }
    });
    pass(file, &mut |_, r| {
        if num(r, SALEDATE) < 20260201 {
            jan += num(r, AMOUNT);
        }
    });
    sum_r3 == 11099768125 && cnt_r3 == 22094 && all == 99898922935 && big == 19973 && qty == 1135702 && loc == 396
        && loc_ids == 40072280 && jan == 8435099403
}

fn main() {
    let mut seed: i64 = 31337;
    let mut rand = |n: i64| {
        seed = seed * 16807 % 2147483647;
        seed % n
    };
    let base = 20454; // 2026-01-01 as days from 1970-01-01
    {
        let mut w = BufWriter::new(File::create(PATH).unwrap());
        for _ in 0..200000 {
            let region = rand(9) + 1;
            let amount = rand(1000000);
            let qty = rand(50) + 1;
            let (y, m, d) = civil(base + rand(365));
            let cust = format!("CUST{:06}", rand(50000));
            write!(w, " {:1}{:10}{:3}{:<10}{:04}{:02}{:02}", region, amount, qty, cust, y, m, d).unwrap();
        }
        w.flush().unwrap();
    }
    let mut file = File::open(PATH).unwrap();
    let t = Instant::now();
    let mut ok = true;
    for _ in 0..4 {
        ok = ok && scans(&mut file);
    }
    println!("sequential_scan {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
    drop(file);
    std::fs::remove_file(PATH).unwrap();
}
