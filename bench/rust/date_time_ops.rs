// bench/date_time_ops.prg in Rust: 200,000 dates between 1900 and 2099 and times of day from a fixed seed. For each:
// year, month, day against the DToS text; day of week and its name (Sundays, Fridays the 13th); the British text
// (dd/mm/yyyy) and back; 13 months added (the day kept within the month, through the date read from text and day
// arithmetic); the days to the year's end; a time of day to "hh:mm:ss" and back. Std has no calendar: the date
// functions are written here, on day numbers (days since 1970-01-01), with the text forms the .prg uses.
use std::time::Instant;

// The day number of y-m-d (proleptic Gregorian).
fn days_from_civil(y: i64, m: i64, d: i64) -> i64 {
    let y = if m <= 2 { y - 1 } else { y };
    let era = y.div_euclid(400);
    let yoe = y - era * 400;
    let doy = (153 * (if m > 2 { m - 3 } else { m + 9 }) + 2) / 5 + d - 1;
    let doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    era * 146097 + doe - 719468
}

// The (year, month, day) of a day number.
fn civil_from_days(z: i64) -> (i64, i64, i64) {
    let z = z + 719468;
    let era = z.div_euclid(146097);
    let doe = z - era * 146097;
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = doy - (153 * mp + 2) / 5 + 1;
    let m = if mp < 10 { mp + 3 } else { mp - 9 };
    (yoe + era * 400 + i64::from(m <= 2), m, d)
}

fn days_in_month(y: i64, m: i64) -> i64 {
    const DAYS: [i64; 12] = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
    if m == 2 && (y % 4 == 0 && y % 100 != 0 || y % 400 == 0) { 29 } else { DAYS[(m - 1) as usize] }
}

// The day number of a valid date, None for an invalid one.
fn make_date(y: i64, m: i64, d: i64) -> Option<i64> {
    if (1..=12).contains(&m) && d >= 1 && d <= days_in_month(y, m) { Some(days_from_civil(y, m, d)) } else { None }
}

fn dtos(d: i64) -> String {
    let (y, m, dd) = civil_from_days(d);
    format!("{:04}{:02}{:02}", y, m, dd)
}

fn stod(s: &str) -> Option<i64> {
    let num = |a: usize, b: usize| s.get(a..b).and_then(|p| p.trim().parse::<i64>().ok());
    make_date(num(0, 4)?, num(4, 6)?, num(6, 8)?)
}

// SET DATE BRITISH, SET CENTURY ON
fn dtoc(d: i64) -> String {
    let (y, m, dd) = civil_from_days(d);
    format!("{:02}/{:02}/{:04}", dd, m, y)
}

fn ctod(s: &str) -> Option<i64> {
    let mut parts = s.split('/').map(|p| p.trim().parse::<i64>().ok());
    let dd = parts.next()??;
    let m = parts.next()??;
    let y = parts.next()??;
    make_date(y, m, dd)
}

// 1 for Sunday .. 7 for Saturday (1970-01-01 was a Thursday).
fn dow(d: i64) -> i64 {
    (d + 4).rem_euclid(7) + 1
}

fn cdow(d: i64) -> &'static str {
    ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"][(dow(d) - 1) as usize]
}

fn tstring(s: i64) -> String {
    format!("{:02}:{:02}:{:02}", s / 3600 % 24, s / 60 % 60, s % 60)
}

fn secs(t: &str) -> i64 {
    t.split(':').take(3).zip([3600, 60, 1]).map(|(p, w)| p.trim().parse::<i64>().unwrap_or(0) * w).sum()
}

// The date n months after y-m-d, its day made the month's last when the month is shorter.
fn add_months(y: i64, m: i64, day: i64, n: i64) -> i64 {
    let mm = m + n;
    let ny = y + (mm - 1) / 12;
    let mm = (mm - 1) % 12 + 1;
    let first = stod(&format!("{:04}{:02}01", ny, mm)).unwrap();
    let next = first + 31;
    first + day.min(civil_from_days(next - civil_from_days(next).2).2) - 1
}

fn main() {
    let mut seed: i64 = 1999;
    let mut rand = |n: i64| { seed = seed * 16807 % 2147483647; seed % n };
    let base = stod("19000101").unwrap();
    let (mut ymd, mut sun, mut fri13, mut add, mut to_end, mut sec, mut bad) = (0i64, 0i64, 0i64, 0i64, 0i64, 0i64,
        0i64);
    let t = Instant::now();
    for _ in 0..200000 {
        let d = base + rand(73000);
        let (y, m, day) = civil_from_days(d);
        let n = y * 10000 + m * 100 + day;
        ymd += n;
        if dtos(d).parse::<i64>().unwrap_or(0) != n || ctod(&dtoc(d)) != Some(d) {
            bad += 1;
        }
        if dow(d) == 1 {
            sun += 1;
        }
        if day == 13 && cdow(d) == "Friday" {
            fri13 += 1;
        }
        add += dtos(add_months(y, m, day, 13)).parse::<i64>().unwrap_or(0);
        to_end += stod(&format!("{:4}1231", y)).unwrap() - d;
        let s = rand(86400);
        if secs(&tstring(s)) != s {
            bad += 1;
        }
        sec += s;
    }
    let ok = bad == 0 && ymd == 3998853334023 && sun == 28567 && fri13 == 951 && add == 4001021793716
        && to_end == 36531947 && sec == 8632784536;
    println!("date_time_ops {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
