// bench/numeric_formatting.prg in Rust: number formatting: 80,000 numbers with three decimals (-50,000 to 50,000) and
// integers (0 to 999,999) from a fixed seed, each formatted as Str (width and decimals), StrZero, Round, and Transform
// with numeric pictures (thousands separators, @E, @( for negatives in parentheses, @Z) and @R on a string (a CPF
// mask) give them, and read back with parse. The standard library has none of these: Str() is format! with a width
// and decimals, Round() and the Transform() pictures used are written out by xBase's rules (Round adds half a unit and
// truncates, with a margin of one epsilon; a picture's field is Str's, its separators placed between digits, a too
// large number in parentheses turned to stars). The text's length, its separators and the values read back are
// checked; no number lies halfway between two values of two decimals.
use std::time::Instant;

// Str( x, width, dec ): right-aligned, rounded to dec decimals; stars when it does not fit.
fn str_num(x: f64, width: usize, dec: usize) -> String {
    let s = format!("{:>w$.d$}", x, w = width, d = dec);
    if s.len() > width { "*".repeat(width) } else { s }
}

// Round( v, d ) for d >= 0: half a unit added away from zero, then truncated with a margin of one epsilon.
fn round(v: f64, d: i32) -> f64 {
    if v == 0.0 {
        return 0.0;
    }
    let p = 10f64.powi(d);
    let mut tenfold = v * p * 10.0;
    tenfold += if tenfold < 0.0 { -5.0 } else { 5.0 };
    let back = tenfold / 10.0;
    back.mul_add(f64::EPSILON, back).trunc() / p
}

// Transform( x, template ) of a number with the functions E (',' and '.' exchanged), ( (a negative number in
// parentheses) and Z (blanks for zero), for a template of '9', ',' and '.'.
fn transform_num(x: f64, template: &str, british: bool, parens: bool, blank: bool) -> String {
    let pic = template.as_bytes();
    let (mut width, mut dec) = (0usize, 0usize);
    for (i, &c) in pic.iter().enumerate() {
        if c == b'.' {
            dec = pic[i + 1..].iter().filter(|&&p| p == b'9').count();
            width += dec + usize::from(dec > 0);
            break;
        }
        if c == b'9' {
            width += 1;
        }
    }
    let shown = if parens && x < 0.0 { -x } else { x };
    let field = str_num(shown, width, dec);
    let field = field.as_bytes();
    let mut o: Vec<u8> = Vec::with_capacity(pic.len() + 1);
    let mut used = 0;
    for (i, &p) in pic.iter().enumerate() {
        if p == b'9' {
            o.push(if used < width { used += 1; field[used - 1] } else { b' ' });
        } else if p == b'.' && used < width {
            o.push(if british { b',' } else { b'.' });
            used += 1;
        } else if p == b',' && i > 0 && used < width {
            let before = o[i - 1];
            if before.is_ascii_digit() {
                o.push(if british { b'.' } else { b',' });
            } else {
                // before the digits: what precedes it moves right
                o.push(before);
                if before == b'-' {
                    o[i - 1] = if i > 1 && o[i - 2] != b'$' { o[i - 2] } else { b' ' };
                }
            }
        } else {
            o.push(p);
        }
    }
    if x < 0.0 && parens {
        // a digit where the parenthesis goes: no room, the digits become stars
        if (b'1'..=b'9').contains(&o[0]) {
            for (k, c) in o.iter_mut().enumerate().skip(1) {
                if c.is_ascii_digit() && (pic[k] == b'9' || pic[k] != *c) {
                    *c = b'*';
                }
            }
        }
        o[0] = b'(';
        o.push(b')');
    }
    if blank && x == 0.0 {
        o.fill(b' ');
    }
    String::from_utf8(o).unwrap()
}

// Transform( s, "@R " + template ): the template's '9's take the string's characters, its other characters inserted.
fn transform_r(s: &str, template: &str) -> String {
    let mut chars = s.chars();
    let mut o = String::with_capacity(template.len());
    for p in template.chars() {
        if p == '9' {
            match chars.next() {
                Some(c) => o.push(c),
                None => o.push(' '),
            }
        } else {
            o.push(p);
        }
    }
    o
}

fn main() {
    let t = Instant::now();
    let mut seed: i64 = 314159;
    let mut rand = |n: i64| {
        seed = seed * 16807 % 2147483647;
        seed % n
    };
    let mut c = String::new();
    let mut cents: i64 = 0;
    let mut tenths: i64 = 0;
    for _ in 0..80000 {
        let mut xi = rand(100000000) - 50000000;
        if xi.abs() % 10 == 5 {
            xi += 1;
        }
        let x = xi as f64 / 1000.0;
        let n = rand(1000000);
        let s14 = str_num(x, 14, 2);
        c.push_str(&s14);
        c.push_str(&format!("{:08}", n));
        c.push_str(&transform_num(x, "999,999.99", true, false, false));
        c.push_str(&transform_num(x, "999,999.999", false, false, false));
        c.push_str(&transform_r(&format!("{:011}", n), "999.999.999-99"));
        c.push_str(&transform_num(x, "99,999.99", false, true, false));
        c.push_str(&transform_num((n % 3) as f64, "9", false, false, true));
        c.push_str(str_num(round(x, 1), 12, 1).trim_start_matches(' '));
        c.push('\n');
        cents += round(s14.trim().parse::<f64>().unwrap_or(0.0) * 100.0, 0) as i64;
        tenths += round(round(x, 1) * 10.0, 0) as i64;
    }
    let comma = c.matches(',').count();
    let dot = c.matches('.').count();
    let paren = c.matches('(').count();
    // The first line, the counts and the text as a whole were checked against the reference implementation by the
    // .prg; the sums of cents and tenths against a C computation.
    let ok = c.len() == 6065015 && comma == 236856 && dot == 558428 && paren == 41318 && cents == -6496080269
        && tenths == -649608444
        && c.starts_with("      35103.0200005610 35.103,02 35,103.019000.000.056-1035,103.02 35103.0\n");
    println!("numeric_formatting {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
