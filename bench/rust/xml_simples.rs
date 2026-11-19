// bench/xml_simples.prg in Rust: a catalog of 50,000 books { title, author, price, year } (titles and authors with
// & < > and ' in them) made from a fixed seed, then timed: written as one document (the text escaped), split into
// elements with str::split, and read back (each child's text found with find between its tags, the id attribute, the
// entities decoded with replace, the price parsed). The values read back add up as made.
use std::time::Instant;

fn rand(seed: &mut i64, n: i64) -> i64 {
    *seed = *seed * 16807 % 2147483647;
    *seed % n
}

// The text between <tag> and </tag> in c.
fn inner<'a>(c: &'a str, tag: &str) -> &'a str {
    let start = c.find(&format!("<{}>", tag)).map_or(0, |p| p + tag.len() + 2);
    let end = c.find(&format!("</{}>", tag)).unwrap_or(start);
    &c[start..end.max(start)]
}

fn escape(c: &str) -> String {
    c.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;").replace('\'', "&apos;")
}

fn unescape(c: &str) -> String {
    c.replace("&apos;", "'").replace("&gt;", ">").replace("&lt;", "<").replace("&amp;", "&")
}

// Val(): the number at the start of the text (digits, then a point and digits), 0 when there is none.
fn val(c: &str) -> f64 {
    let b = c.as_bytes();
    let mut end = 0;
    while end < b.len() && (b[end].is_ascii_digit() || b[end] == b'.') {
        end += 1;
    }
    c[..end].parse().unwrap_or(0.0)
}

fn main() {
    let mut seed: i64 = 5150;
    let words =
        ["Rivers", "Salt & Pepper", "<Night>", "Stone", "Tales", "Tom & Jerry", "Glass", "Winter", "Red", "Old"];
    let authors = ["Ana Lima", "Bruno Costa", "Carla Dias", "Diego Reis", "Eva O'Neil"];
    let mut books: Vec<(String, &str, i64, i64)> = Vec::with_capacity(50000);
    for _ in 0..50000 {
        let n = rand(&mut seed, 2) + 2;
        let mut title = String::new();
        for k in 1..=n {
            if k > 1 {
                title.push(' ');
            }
            title += words[rand(&mut seed, 10) as usize];
        }
        let author = authors[rand(&mut seed, 5) as usize];
        let price = rand(&mut seed, 10000) + 100;
        let year = 1950 + rand(&mut seed, 75);
        books.push((title, author, price, year));
    }
    let books = std::hint::black_box(books);
    let t = Instant::now();
    let mut c = String::from("<?xml version=\"1.0\"?>\n<catalog>\n");
    for (i, b) in books.iter().enumerate() {
        let book = format!(
            "<book id=\"{}\"><title>{}</title><author>{}</author><price>{:.2}</price><year>{:4}</year></book>\n",
            i + 1,
            escape(&b.0),
            escape(b.1),
            b.2 as f64 / 100.0,
            b.3
        );
        c += &book;
    }
    c += "</catalog>\n";
    let mut ok = c.len() == 6276273;
    let parts: Vec<&str> = c.split("<book ").collect();
    let (mut cents, mut amp, mut years, mut eva, mut ids) = (0i64, 0i64, 0i64, 0i64, 0i64);
    let mut c12345 = String::new();
    for (i, book) in parts.iter().enumerate().skip(1) {
        ids += val(&book[4..]) as i64;
        let title = unescape(inner(book, "title"));
        if title.contains('&') {
            amp += 1;
        }
        if i + 1 == 12346 {
            c12345 = title;
        }
        if unescape(inner(book, "author")) == "Eva O'Neil" {
            eva += 1;
        }
        cents += (val(inner(book, "price")) * 100.0).round() as i64;
        years += val(inner(book, "year")) as i64;
    }
    ok = ok && parts.len() == 50001 && ids == 50000 * 50001 / 2 && cents == 255248286 && amp == 21225
        && years == 99351507 && eva == 10152 && c12345 == "Winter Glass Tom & Jerry";
    println!("xml_simples {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
