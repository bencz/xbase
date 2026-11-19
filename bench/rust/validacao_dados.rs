// bench/validacao_dados.prg in Rust: data validation: 250,000 records (CPF, e-mail, date text, amount text) made from
// a fixed seed, with errors mixed in (a wrong check digit, an e-mail with no "@", no dot or a bad character,
// impossible dates, a letter among the digits). Timed: each field validated by its rule (the CPF's two mod-11 check
// digits, the e-mail's parts and characters, the date as CToD() reads dd/mm/yyyy under SET DATE BRITISH and SET
// CENTURY ON, written out: its fields and the days of its month; the amount's digits and range) and the valid ones
// counted.
use std::time::Instant;

fn cpf_ok(c: &str) -> bool {
    let d = c.replace('.', "").replace('-', "");
    let d = d.as_bytes();
    if d.len() != 11 || !d.iter().all(|b| b.is_ascii_digit()) {
        return false;
    }
    let digit = |i: usize| i64::from(d[i] - b'0');
    let mut s: i64 = 0;
    for i in 0..9 {
        s += digit(i) * (10 - i as i64);
    }
    if s * 10 % 11 % 10 != digit(9) {
        return false;
    }
    s = 0;
    for i in 0..10 {
        s += digit(i) * (11 - i as i64);
    }
    s * 10 % 11 % 10 == digit(10)
}

fn email_ok(e: &str) -> bool {
    let at = match e.find('@') {
        Some(p) if p >= 1 => p,
        _ => return false,
    };
    let dom = &e[at + 1..];
    if dom.contains('@') {
        return false;
    }
    if !e[..at].chars().all(|c| "abcdefghijklmnopqrstuvwxyz0123456789._".contains(c)) {
        return false;
    }
    if dom.len() < 3 || !dom.contains('.') || dom.starts_with('.') || dom.ends_with('.') {
        return false;
    }
    dom.chars().all(|c| "abcdefghijklmnopqrstuvwxyz0123456789.-".contains(c))
}

fn amount_ok(c: &str) -> bool {
    if !c.bytes().all(|b| b.is_ascii_digit()) {
        return false;
    }
    let n: i64 = c.parse().unwrap_or(0);
    (1..=999999).contains(&n)
}

// CToD( "dd/mm/yyyy" ) under SET DATE BRITISH and SET CENTURY ON: whether it is a date (not an empty one).
fn date_ok(s: &str) -> bool {
    let mut parts = s.split('/').map(|p| p.parse::<i64>().ok());
    let (Some(Some(d)), Some(Some(m)), Some(Some(y)), None) = (parts.next(), parts.next(), parts.next(), parts.next())
    else {
        return false;
    };
    let leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    let days = match m {
        1 | 3 | 5 | 7 | 8 | 10 | 12 => 31,
        4 | 6 | 9 | 11 => 30,
        2 if leap => 29,
        2 => 28,
        _ => return false,
    };
    (1..=9999).contains(&y) && d >= 1 && d <= days
}

struct Rng(i64);
impl Rng {
    fn rand(&mut self, n: i64) -> i64 {
        self.0 = self.0 * 16807 % 2147483647;
        self.0 % n
    }
}

// A record; its errors come from the seed as the checks' constants were computed.
fn make_record(g: &mut Rng) -> [String; 4] {
    let mut d = [0i64; 11];
    for v in d.iter_mut().take(9) {
        *v = g.rand(10);
    }
    let mut s: i64 = (0..9).map(|k| d[k] * (10 - k as i64)).sum();
    d[9] = s * 10 % 11 % 10;
    s = (0..10).map(|k| d[k] * (11 - k as i64)).sum();
    d[10] = s * 10 % 11 % 10;
    if g.rand(4) == 0 {
        d[10] = (d[10] + 1) % 10;
    }
    let mut cpf = String::new();
    for (k, v) in d.iter().enumerate() {
        cpf.push_str(&format!("{:1}", v));
        cpf.push_str(if k == 2 || k == 5 { "." } else if k == 8 { "-" } else { "" });
    }
    let chars = b"abcdefghijklmnopqrstuvwxyz0123456789";
    let mut mail = String::new();
    let len = g.rand(8) + 3;
    for _ in 0..len {
        mail.push(chars[g.rand(36) as usize] as char);
    }
    let kind = g.rand(20);
    let dom = ["mail.com", "example.com.br", "x.org"][g.rand(3) as usize];
    match kind {
        0 => mail.push_str(dom),
        1 => mail.push_str("@examplecom"),
        2 => {
            mail.push_str("!@");
            mail.push_str(dom);
        }
        _ => {
            mail.push('@');
            mail.push_str(dom);
        }
    }
    let day = g.rand(31) + 1;
    let month = g.rand(12) + 1;
    let date = format!("{:02}/{:02}/{:4}", day, month, 1990 + g.rand(41));
    let len = g.rand(7) + 1;
    let mut amt = String::new();
    for _ in 0..len {
        amt.push_str(&format!("{:1}", g.rand(10)));
    }
    if g.rand(10) == 0 {
        let p = g.rand(len) as usize;
        amt.replace_range(p..p + 1, "x");
    }
    [cpf, mail, date, amt]
}

fn main() {
    let mut g = Rng(8086);
    let recs: Vec<[String; 4]> = std::hint::black_box((0..250000).map(|_| make_record(&mut g)).collect());
    let t = Instant::now();
    let (mut nc, mut ne, mut nd, mut na, mut all, mut sum) = (0i64, 0i64, 0i64, 0i64, 0i64, 0i64);
    for r in &recs {
        let lc = cpf_ok(&r[0]);
        let le = email_ok(&r[1]);
        let ld = date_ok(&r[2]);
        let la = amount_ok(&r[3]);
        if la {
            sum += r[3].parse::<i64>().unwrap_or(0);
        }
        nc += i64::from(lc);
        ne += i64::from(le);
        nd += i64::from(ld);
        na += i64::from(la);
        all += i64::from(lc && le && ld && la);
    }
    let ok = nc == 187295 && ne == 212602 && nd == 245457 && na == 192603 && all == 120445 && sum == 19490512431;
    println!("validacao_dados {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
