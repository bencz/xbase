// bench/geracao_html.prg in Rust: a table of 100,000 items { name of two words, quantity, price in cents } made from
// a fixed seed, then timed: one page built by appending to a String, each row with its id, a zebra class, the name
// escaped (four replaces of & < > "), and the price and total formatted with two decimals.
use std::time::Instant;

fn escape(c: &str) -> String {
    c.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;").replace('"', "&quot;")
}

fn main() {
    let words = ["Alpha", "Beta & Sons", "<Gamma>", "Delta", "Echo \"Q\"", "Foxtrot", "Golf", "H&M"];
    let mut seed: i64 = 4711;
    let mut rand = |n: i64| { seed = seed * 16807 % 2147483647; seed % n };
    let mut items: Vec<(String, i64, i64)> = Vec::with_capacity(100000);
    for _ in 0..100000 {
        let name = words[rand(8) as usize].to_string() + " " + words[rand(8) as usize];
        let qty = rand(100) + 1;
        let price = rand(1000000) + 1;
        items.push((name, qty, price));
    }
    let t = Instant::now();
    let mut c = "<html><head><title>Items</title></head><body>".to_string() + "\n" + "<table>" + "\n"
        + "<tr><th>Id</th><th>Name</th><th>Qty</th><th>Price</th><th>Total</th></tr>" + "\n";
    let mut pos77 = 0;
    for (i, r) in items.iter().enumerate() {
        let i = i + 1;
        let id = i.to_string();
        if i == 77777 {
            pos77 = c.len() + 1;
        }
        let row = "<tr id=\"r".to_string() + &id + "\" class=\"" + if i % 2 == 1 { "odd" } else { "even" }
            + "\"><td>" + &id + "</td><td>" + &escape(&r.0) + "</td><td class=\"n\">" + &r.1.to_string()
            + "</td><td class=\"n\">" + &format!("{:.2}", r.2 as f64 / 100.0) + "</td><td class=\"n\">"
            + &format!("{:.2}", (r.1 * r.2) as f64 / 100.0) + "</td></tr>" + "\n";
        c += &row;
    }
    c += &("</table>".to_string() + "\n" + "</body></html>" + "\n");
    let row77 = "<tr id=\"r77777\" class=\"odd\"><td>77777</td><td>&lt;Gamma&gt; Delta</td><td class=\"n\">56</td>\
        <td class=\"n\">3992.87</td><td class=\"n\">223600.72</td></tr>\n";
    let tail = "<tr id=\"r100000\" class=\"even\"><td>100000</td><td>Foxtrot Alpha</td><td class=\"n\">57</td>\
        <td class=\"n\">4506.67</td><td class=\"n\">256880.19</td></tr>\n</table>\n</body></html>\n";
    let ok = c.len() == 15121698 && pos77 == 11756338 && c[pos77 - 1..].starts_with(row77) && c.ends_with(tail);
    println!("geracao_html {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
