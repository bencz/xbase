// bench/json_roundtrip.prg in Rust: JSON written and read back by hand-written code over an enum value: 20,000 records
// as hashes (an id, a name with quotes, backslashes, tabs and line feeds, an array of tags, a bool, a number, a null,
// a nested hash) made from a fixed seed, then timed: serialized to text, parsed back by a recursive descent parser,
// and serialized again. The two texts must be equal, and the values read back add up as made. A hash is a HashMap
// from key to slot plus a vector of its entries, so that it keeps the order of its keys as xBase hashes do.
use std::collections::HashMap;
use std::time::Instant;

enum Json {
    Null,
    Bool(bool),
    Num(i64),
    Str(String),
    Arr(Vec<Json>),
    Obj(Hash),
}

#[derive(Default)]
struct Hash {
    slots: HashMap<String, usize>,
    entries: Vec<(String, Json)>,
}

impl Hash {
    fn set(&mut self, k: String, v: Json) {
        match self.slots.get(&k) {
            Some(&i) => self.entries[i].1 = v,
            None => {
                self.slots.insert(k.clone(), self.entries.len());
                self.entries.push((k, v));
            }
        }
    }
    fn get(&self, k: &str) -> &Json { self.slots.get(k).map_or(&Json::Null, |&i| &self.entries[i].1) }
}

fn quote(c: &str) -> String {
    format!("\"{}\"", c.replace('\\', "\\\\").replace('"', "\\\"").replace('\n', "\\n").replace('\t', "\\t"))
}

fn to_json(x: &Json) -> String {
    match x {
        Json::Obj(h) => {
            let mut c = String::from("{");
            let mut sep = "";
            for (k, v) in &h.entries {
                let item = quote(k) + ":" + &to_json(v);
                c += sep;
                c += &item;
                sep = ",";
            }
            c + "}"
        }
        Json::Arr(a) => {
            let mut c = String::from("[");
            let mut sep = "";
            for v in a {
                let item = to_json(v);
                c += sep;
                c += &item;
                sep = ",";
            }
            c + "]"
        }
        Json::Str(s) => quote(s),
        Json::Num(n) => n.to_string(),
        Json::Bool(b) => String::from(if *b { "true" } else { "false" }),
        Json::Null => String::from("null"),
    }
}

// The value that starts at pos, which it leaves after the value.
fn parse(c: &[u8], pos: &mut usize) -> Json {
    match c[*pos] {
        b'{' => {
            let mut h = Hash::default();
            *pos += 1;
            if c[*pos] == b'}' {
                *pos += 1;
                return Json::Obj(h);
            }
            loop {
                let k = parse_string(c, pos);
                *pos += 1;
                let v = parse(c, pos);
                h.set(k, v);
                let ch = c[*pos];
                *pos += 1;
                if ch == b'}' {
                    break;
                }
            }
            Json::Obj(h)
        }
        b'[' => {
            let mut a = Vec::new();
            *pos += 1;
            if c[*pos] == b']' {
                *pos += 1;
                return Json::Arr(a);
            }
            loop {
                a.push(parse(c, pos));
                let ch = c[*pos];
                *pos += 1;
                if ch == b']' {
                    break;
                }
            }
            Json::Arr(a)
        }
        b'"' => Json::Str(parse_string(c, pos)),
        b't' => {
            *pos += 4;
            Json::Bool(true)
        }
        b'f' => {
            *pos += 5;
            Json::Bool(false)
        }
        b'n' => {
            *pos += 4;
            Json::Null
        }
        _ => {
            let k = *pos;
            while *pos < c.len() && (c[*pos] == b'-' || c[*pos].is_ascii_digit()) {
                *pos += 1;
            }
            Json::Num(std::str::from_utf8(&c[k..*pos]).unwrap_or("0").parse().unwrap_or(0))
        }
    }
}

fn parse_string(c: &[u8], pos: &mut usize) -> String {
    let mut out: Vec<u8> = Vec::new();
    *pos += 1;
    let mut start = *pos;
    while c[*pos] != b'"' {
        if c[*pos] == b'\\' {
            out.extend_from_slice(&c[start..*pos]);
            let ch = c[*pos + 1];
            out.push(match ch {
                b'n' => b'\n',
                b't' => b'\t',
                _ => ch,
            });
            *pos += 2;
            start = *pos;
        } else {
            *pos += 1;
        }
    }
    out.extend_from_slice(&c[start..*pos]);
    *pos += 1;
    String::from_utf8(out).unwrap_or_default()
}

fn rand(seed: &mut i64, n: i64) -> i64 {
    *seed = *seed * 16807 % 2147483647;
    *seed % n
}

// A record, drawn from the seed in the order the checks' constants were computed.
fn make_record(seed: &mut i64, i: i64) -> Json {
    let chars = b"abcdefghijk ABC\"\\\n\txyz";
    let mut name = String::new();
    let n = rand(seed, 11) + 5;
    for _ in 0..n {
        name.push(chars[rand(seed, 22) as usize] as char);
    }
    let n = rand(seed, 4);
    let active = rand(seed, 2) == 1;
    let qty = rand(seed, 2001) - 1000;
    let parent = if rand(seed, 3) != 0 { Json::Num(rand(seed, i) + 1) } else { Json::Null };
    let city = format!("City {}", rand(seed, 50));
    let zip = rand(seed, 90000) + 10000;
    let tags: Vec<Json> = (0..n).map(|_| Json::Str(format!("tag{}", rand(seed, 100)))).collect();
    let mut address = Hash::default();
    address.set("city".into(), Json::Str(city));
    address.set("zip".into(), Json::Num(zip));
    let mut h = Hash::default();
    h.set("id".into(), Json::Num(i));
    h.set("name".into(), Json::Str(name));
    h.set("tags".into(), Json::Arr(tags));
    h.set("active".into(), Json::Bool(active));
    h.set("qty".into(), Json::Num(qty));
    h.set("parent".into(), parent);
    h.set("address".into(), Json::Obj(address));
    Json::Obj(h)
}

fn num(x: &Json) -> i64 { if let Json::Num(n) = x { *n } else { 0 } }

fn main() {
    let mut seed: i64 = 1618;
    let data = Json::Arr((1..=20000).map(|i| make_record(&mut seed, i)).collect());
    let data = std::hint::black_box(data);
    let t = Instant::now();
    let c1 = to_json(&data);
    let mut pos = 0usize;
    let back = parse(c1.as_bytes(), &mut pos);
    let c2 = to_json(&back);
    let (mut qty, mut active, mut tags, mut null, mut zip) = (0i64, 0i64, 0i64, 0i64, 0i64);
    let mut names = (String::new(), String::new());
    let mut count = 0;
    if let Json::Arr(a) = &back {
        count = a.len();
        for (i, r) in a.iter().enumerate() {
            if let Json::Obj(h) = r {
                qty += num(h.get("qty"));
                active += if let Json::Bool(true) = h.get("active") { 1 } else { 0 };
                tags += if let Json::Arr(t) = h.get("tags") { t.len() as i64 } else { 0 };
                null += if let Json::Null = h.get("parent") { 1 } else { 0 };
                zip += if let Json::Obj(ad) = h.get("address") { num(ad.get("zip")) } else { 0 };
                if let (Json::Str(s), true) = (h.get("name"), i < 2) {
                    if i == 0 { names.0 = s.clone() } else { names.1 = s.clone() }
                }
            }
        }
    }
    let ok = c1 == c2 && c1.len() == 2697804 && pos == c1.len() && count == 20000 && qty == -88759
        && active == 9960 && tags == 29729 && null == 6525 && zip == 1098378340 && names.0 == "ykAeACczkccifkx"
        && names.1 == "Cdkk\"\\Ae\nx\nz" && c1.starts_with("[{\"id\":1,\"name\":\"ykAeACczkccifkx\"");
    println!("json_roundtrip {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
