// bench/ini_parse.prg in Rust: INI files: a text of 400 sections of 25 keys (comments, blank lines, spaces around
// "=", quoted values, section names in either case) made from a fixed seed, then timed: parsed 80 times into a
// HashMap of sections, each a HashMap of keys (lines split with split collected into a vector of slices, trimmed,
// names lowered, quotes taken off); the numbers summed and values looked up.
use std::collections::HashMap;
use std::time::Instant;

type Ini = HashMap<String, HashMap<String, String>>;

// The sections of the text, each a map of its keys, and the count of quoted values.
fn parse_ini(c: &str) -> (Ini, i64) {
    let mut ini: Ini = HashMap::new();
    let mut section: Option<(String, HashMap<String, String>)> = None;
    let mut quoted = 0;
    let lines: Vec<&str> = c.split('\n').collect();
    for line in lines {
        let line = line.trim_matches(' ');
        if line.is_empty() || line.starts_with(';') || line.starts_with('#') {
            continue;
        }
        if line.starts_with('[') {
            if let Some((name, keys)) = section.take() {
                ini.insert(name, keys);
            }
            section = Some((line[1..line.len() - 1].to_ascii_lowercase(), HashMap::new()));
        } else if let Some(eq) = line.find('=') {
            let key = line[..eq].trim_end_matches(' ').to_ascii_lowercase();
            let mut val = line[eq + 1..].trim_start_matches(' ');
            if val.len() >= 2 && val.starts_with('"') && val.ends_with('"') {
                val = &val[1..val.len() - 1];
                quoted += 1;
            }
            if let Some((_, keys)) = section.as_mut() {
                keys.insert(key, val.to_string());
            }
        }
    }
    if let Some((name, keys)) = section.take() {
        ini.insert(name, keys);
    }
    (ini, quoted)
}

fn main() {
    let mut seed: i64 = 7777;
    let mut rand = |n: i64| {
        seed = seed * 16807 % 2147483647;
        seed % n
    };
    let mut lines: Vec<String> = Vec::new();
    for s in 1..=400 {
        lines.push(format!("; section {}", s));
        lines.push(format!("[{}{}]", if rand(2) == 1 { "Section" } else { "SECTION" }, s));
        for k in 1..=25 {
            let sp = rand(3);
            let n = rand(100000).to_string();
            let val = if k % 5 == 0 { n } else if k % 5 == 1 { format!("\"text {}\"", n) } else { format!("w{}", n) };
            let (indent, eq) = (if sp == 2 { "  " } else { "" }, if sp > 0 { " = " } else { "=" });
            lines.push(format!("{}Key{:02}{}{}", indent, k, eq, val));
            if k % 7 == 0 {
                lines.push(String::from("# comment"));
            }
        }
        lines.push(String::new());
    }
    let mut c = String::new();
    for x in &lines {
        c += x;
        c.push('\n');
    }
    let t = Instant::now();
    let mut ok = true;
    for _ in 0..80 {
        let (h, quoted) = parse_ini(&c);
        let mut sum: i64 = 0;
        for x in h.values() {
            for k in (5..=25).step_by(5) {
                sum += x[&format!("key{:02}", k)].parse::<i64>().unwrap();
            }
        }
        ok = ok && h.len() == 400 && sum == 102092980 && quoted == 2000 && h["section77"]["key11"] == "text 72557"
            && h["section400"].len() == 25;
    }
    ok = ok && c.len() == 181694;
    println!("ini_parse {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
