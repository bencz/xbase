// bench/pivot_crosstab.prg in Rust: 2,000,000 sales records { seller, date, amount } made from a fixed seed (60
// sellers, the days of 2025), then timed: the amounts summed per seller and month in a HashMap of rows, and per
// weekday and month; row and column totals, the sellers sorted, and the table rendered as text (padded and
// right-aligned numbers) with a totals line. Std has no calendar: dates are day numbers (days since 1970-01-01), their
// month and weekday computed by the civil-calendar functions written here.
use std::collections::HashMap;
use std::time::Instant;

fn days_from_civil(y: i64, m: i64, d: i64) -> i64 {
    let y = if m <= 2 { y - 1 } else { y };
    let era = y.div_euclid(400);
    let yoe = y - era * 400;
    let doy = (153 * (if m > 2 { m - 3 } else { m + 9 }) + 2) / 5 + d - 1;
    let doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    era * 146097 + doe - 719468
}

// The month of a day number.
fn month(z: i64) -> i64 {
    let z = z + 719468;
    let doe = z - z.div_euclid(146097) * 146097;
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    if mp < 10 { mp + 3 } else { mp - 9 }
}

// 1 for Sunday .. 7 for Saturday (1970-01-01 was a Thursday).
fn dow(d: i64) -> i64 {
    (d + 4).rem_euclid(7) + 1
}

fn main() {
    let mut seed: i64 = 16180;
    let mut rand = |n: i64| { seed = seed * 16807 % 2147483647; seed % n };
    let base = days_from_civil(2025, 1, 1);
    let mut sales: Vec<(String, i64, i64)> = Vec::with_capacity(2000000);
    for _ in 0..2000000 {
        let seller = format!("SELLER{:03}", rand(60) + 1);
        let date = base + rand(365);
        sales.push((seller, date, rand(50000) + 1));
    }
    let t = Instant::now();
    let mut rows: HashMap<String, [i64; 12]> = HashMap::new();
    let mut col = [0i64; 12];
    let mut by_dow = [[0i64; 12]; 7];
    for r in &sales {
        let a = match rows.get_mut(&r.0) {
            Some(a) => a,
            None => rows.entry(r.0.clone()).or_insert([0; 12]),
        };
        let m = (month(r.1) - 1) as usize;
        a[m] += r.2;
        by_dow[(dow(r.1) - 1) as usize][m] += r.2;
    }
    let mut sorted: Vec<(&String, &[i64; 12])> = rows.iter().collect();
    sorted.sort_by(|p, q| p.0.cmp(q.0));
    let mut tab = String::new();
    let (mut best, mut n_best, mut s07) = (String::new(), 0i64, String::new());
    for (k, a) in sorted {
        let mut line = format!("{:<10}", k);
        let mut row = 0i64;
        for i in 0..12 {
            line += &format!("{:>11}", a[i]);
            row += a[i];
            col[i] += a[i];
        }
        line += &format!("{:>13}", row);
        if row > n_best {
            best = k.clone();
            n_best = row;
        }
        if k == "SELLER007" {
            s07 = line.clone();
        }
        tab += &line;
        tab.push('\n');
    }
    let mut line = format!("{:<10}", "TOTAL");
    let mut grand = 0i64;
    for c in col {
        line += &format!("{:>11}", c);
        grand += c;
    }
    tab += &(line + &format!("{:>13}", grand) + "\n");
    let sat: i64 = by_dow[6].iter().sum();
    let ok = grand == 50028051852 && best == "SELLER015" && n_best == 844241139 && col[0] == 4275964399
        && col[1] == 3837132322 && col[11] == 4246237533 && by_dow[0][0] == 547728957 && sat == 7111132032
        && tab.len() == 61 * (10 + 12 * 11 + 13 + 1)
        && s07 == "SELLER007 ".to_string()
            + "   71382096   62515370   68284183   68152167   70863968   69201960"
            + "   71447433   71672784   67917922   68878748   65718083   68477906    824512620";
    println!("pivot_crosstab {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
