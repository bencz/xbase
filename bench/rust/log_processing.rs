// bench/log_processing.prg in Rust: log processing: 300,000 lines of an access log (time, level, [module], user,
// duration, status) made from a fixed seed as one text, then timed: split into lines (split collected into a vector
// of slices), each line parsed (slices at fixed columns, find for the module and the key=value fields, the leading
// number read as Val() reads it), lines counted per level in a HashMap, durations summed per module, the slowest
// request found, server errors counted per hour, distinct users counted in a HashMap. The keys are slices of the text.
use std::collections::HashMap;
use std::time::Instant;

// The number at the start of s, as Val() reads it (its leading digits).
fn val(s: &str) -> i64 {
    s.bytes().take_while(|c| c.is_ascii_digit()).fold(0, |n, c| n * 10 + i64::from(c - b'0'))
}

fn main() {
    let lv = ["INFO ", "WARN ", "ERROR", "DEBUG"];
    let md = ["auth", "db", "http", "cache", "queue", "mail"];
    let st = [200, 200, 200, 201, 204, 301, 404, 500, 503, 200];
    let n: i64 = 300000;
    let mut seed: i64 = 9001;
    let mut rand = |n: i64| {
        seed = seed * 16807 % 2147483647;
        seed % n
    };
    let mut lines: Vec<String> = Vec::with_capacity(n as usize);
    for i in 1..=n {
        let sec = i * 86400 / n;
        let x = rand(1000);
        let l = rand(10);
        let l = if l < 6 { 1 } else if l < 8 { 4 } else if l < 9 { 2 } else { 3 };
        let m = md[rand(6) as usize];
        let user = rand(5000);
        let ms = rand(2000);
        let status = st[rand(10) as usize];
        lines.push(format!(
            "2026-11-19 {:02}:{:02}:{:02}.{:03} {} [{}] request done user=u{} ms={} status={:3}",
            sec / 3600, sec / 60 % 60, sec % 60, x, lv[l - 1], m, user, ms, status
        ));
    }
    let mut c = String::new();
    for x in &lines {
        c += x;
        c.push('\n');
    }
    drop(lines);
    let t = Instant::now();
    let mut h_lv: HashMap<&str, i64> = HashMap::new();
    let mut h_mod: HashMap<&str, i64> = HashMap::new();
    let mut h_users: HashMap<&str, bool> = HashMap::new();
    let mut hour_err = [0i64; 24];
    let (mut max, mut max_line, mut err5, mut i) = (-1i64, 0i64, 0i64, 0i64);
    let parts: Vec<&str> = c.split('\n').collect();
    for x in parts {
        if x.trim().is_empty() {
            continue;
        }
        i += 1;
        let l = x[24..29].trim_end();
        *h_lv.entry(l).or_insert(0) += 1;
        let b = x.find('[').unwrap();
        let module = &x[b + 1..x.find(']').unwrap()];
        let p_ms = x.find(" ms=").unwrap();
        let ms = val(&x[p_ms + 4..]);
        *h_mod.entry(module).or_insert(0) += ms;
        if ms > max {
            max = ms;
            max_line = i;
        }
        let status = val(&x[x.find(" status=").unwrap() + 8..]);
        if status >= 500 {
            err5 += 1;
            hour_err[val(&x[11..13]) as usize] += 1;
        }
        let p_user = x.find(" user=").unwrap();
        h_users.insert(&x[p_user + 6..p_ms], true);
    }
    let ok = c.len() == 23765927 && i == n && h_lv["INFO"] == 179990 && h_lv["WARN"] == 29916
        && h_lv["ERROR"] == 30173 && h_lv["DEBUG"] == 59921 && h_mod["auth"] == 50005329 && h_mod["db"] == 50599071
        && h_mod["http"] == 50094170 && h_mod["cache"] == 49851623 && h_mod["queue"] == 49898190
        && h_mod["mail"] == 49855051 && max == 1999 && max_line == 1062 && err5 == 59808 && hour_err[13] == 2503
        && h_users.len() == 5000;
    println!("log_processing {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
