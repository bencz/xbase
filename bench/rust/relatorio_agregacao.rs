// bench/relatorio_agregacao.prg in Rust: a business report: 1,000,000 sales records (region, product, quantity, unit
// price in cents) made from a fixed seed, then timed: aggregated by region and by product in HashMaps keyed by code,
// the 500 products ordered by amount with sort_by (ties by code), and a report built with the @E pictures of
// Transform() written out (a formatter of grouped digits): a line per region (sales, quantity, amount, the regions in
// key order) and the top 20 products.
use std::collections::HashMap;
use std::time::Instant;

// Transform( n / 10^dec, "@E <int picture>[.<dec 9s>]" ) for n >= 0: '.' between thousands, ',' before the
// decimals, right-aligned in the picture's width.
fn transform_e(n: i64, dec: u32, int_width: usize, out: &mut String) {
    let scale = 10i64.pow(dec);
    let digits = (n / scale).to_string();
    let mut grouped = String::new();
    for (i, c) in digits.chars().enumerate() {
        if i > 0 && (digits.len() - i) % 3 == 0 {
            grouped.push('.');
        }
        grouped.push(c);
    }
    for _ in grouped.len()..int_width {
        out.push(' ');
    }
    out.push_str(&grouped);
    if dec > 0 {
        out.push(',');
        out.push_str(&format!("{:0w$}", n % scale, w = dec as usize));
    }
}

fn main() {
    let mut seed: i64 = 12345;
    let mut rand = |n: i64| {
        seed = seed * 16807 % 2147483647;
        seed % n
    };
    let mut sales: Vec<(String, String, i64, i64)> = Vec::with_capacity(1000000);
    for _ in 0..1000000 {
        let reg = format!("R{:02}", rand(8) + 1);
        let prod = format!("P{:04}", rand(500) + 1);
        let qty = rand(20) + 1;
        let price = rand(100000) + 100;
        sales.push((reg, prod, qty, price));
    }
    let t = Instant::now();
    let mut h_reg: HashMap<String, [i64; 3]> = HashMap::new();
    let mut h_prod: HashMap<String, i64> = HashMap::new();
    let mut total: i64 = 0;
    for r in &sales {
        let x = r.2 * r.3;
        if let Some(a) = h_reg.get_mut(&r.0) {
            a[0] += 1;
            a[1] += r.2;
            a[2] += x;
        } else {
            h_reg.insert(r.0.clone(), [1, r.2, x]);
        }
        if let Some(v) = h_prod.get_mut(&r.1) {
            *v += x;
        } else {
            h_prod.insert(r.1.clone(), x);
        }
        total += x;
    }
    let mut prod: Vec<(&String, i64)> = h_prod.iter().map(|(k, &x)| (k, x)).collect();
    prod.sort_by(|p, q| q.1.cmp(&p.1).then_with(|| p.0.cmp(q.0)));
    let mut regs: Vec<(&String, &[i64; 3])> = h_reg.iter().collect();
    regs.sort_by(|p, q| p.0.cmp(q.0));
    let mut rep = String::new();
    for (k, a) in &regs {
        rep.push_str(k);
        transform_e(a[0], 0, 9, &mut rep);
        transform_e(a[1], 0, 10, &mut rep);
        transform_e(a[2], 2, 15, &mut rep);
        rep.push('\n');
    }
    for p in prod.iter().take(20) {
        rep.push_str(p.0);
        transform_e(p.1, 2, 15, &mut rep);
        rep.push('\n');
    }
    let lines: Vec<&str> = rep.split('\n').collect();
    let ok = total == 525748851783 && rep.len() == 8 * 41 + 20 * 24 && h_prod.len() == 500
        && lines[0] == "R01  125.691 1.320.222    662.846.057,84"
        && lines[7] == "R08  124.683 1.310.936    656.013.155,38"
        && lines[8] == "P0303     11.438.921,18"
        && lines[27] == "P0360     11.021.303,35";
    println!("relatorio_agregacao {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
