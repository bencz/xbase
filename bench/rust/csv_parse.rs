// bench/csv_parse.prg in Rust: CSV parsed byte by byte: a 40,000-line text (about 2 MB) of 5 fields, some quoted with
// commas, doubled quotes and line breaks inside, read by a state machine into a vector of records (vectors of
// Strings); numbers read as Val() reads them; 6 rounds.
use std::time::Instant;

// The number at the start of s, as Val() reads it (its leading digits).
fn val(s: &str) -> i64 {
    s.bytes().take_while(|c| c.is_ascii_digit()).fold(0, |n, c| n * 10 + i64::from(c - b'0'))
}

fn parse_csv(c: &str) -> Vec<Vec<String>> {
    let b = c.as_bytes();
    let mut recs: Vec<Vec<String>> = Vec::new();
    let mut rec: Vec<String> = Vec::new();
    let mut field = String::new();
    let mut quoted = false;
    let mut i = 0;
    while i < b.len() {
        let x = b[i];
        if quoted {
            if x == b'"' {
                if i + 1 < b.len() && b[i + 1] == b'"' {
                    field.push('"');
                    i += 1;
                } else {
                    quoted = false;
                }
            } else {
                field.push(x as char);
            }
        } else if x == b'"' {
            quoted = true;
        } else if x == b',' {
            rec.push(std::mem::take(&mut field));
        } else if x == b'\n' {
            rec.push(std::mem::take(&mut field));
            recs.push(std::mem::take(&mut rec));
        } else {
            field.push(x as char);
        }
        i += 1;
    }
    recs
}

fn main() {
    let mut seed: i64 = 1999;
    let mut rand = |n: i64| {
        seed = (seed * 16807) % 2147483647;
        seed % n
    };
    // A field: plain letters, or quoted with a comma, a doubled quote or a line break inside.
    let make_field = |rand: &mut dyn FnMut(i64) -> i64| {
        let k = rand(6);
        let ch = ((97 + rand(26)) as u8 as char).to_string();
        let w = ch.repeat((3 + rand(12)) as usize);
        match k {
            0 => format!("\"{}, {}\"", w, w),
            1 => format!("\"say \"\"{}\"\"\"", w),
            2 => format!("\"{}\n{}\"", w, w),
            _ => w,
        }
    };
    let mut c = String::new();
    for i in 1..=40000 {
        let f2 = make_field(&mut rand);
        let n3 = rand(100000);
        let f4 = make_field(&mut rand);
        let f5 = "w".repeat(rand(10) as usize);
        c += &format!("{},{},{},{},{}\n", i, f2, n3, f4, f5);
    }
    let t = Instant::now();
    let (mut n, mut quoted) = (0i64, 0i64);
    let mut recs = Vec::new();
    for _ in 0..6 {
        recs = parse_csv(&c);
        for a in &recs {
            n += val(&a[0]) + val(&a[2]) + a[1].len() as i64 + a[3].len() as i64 + a.len() as i64;
            if a[3].contains('\n') || a[1].contains('"') {
                quoted += 1;
            }
        }
    }
    let ok = recs.len() == 40000 && n == 16750579326 && quoted == 73038;
    println!("csv_parse {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
