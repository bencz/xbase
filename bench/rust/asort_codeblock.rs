// bench/asort_codeblock.prg in Rust: the library's stable sort (sort_by) with a comparison closure over records:
// 150,000 employees { name, department, salary, id } made from a fixed seed, ordered by department, then salary
// descending, then name. The order is checked with a running hash of the ids.
use std::time::Instant;

struct Employee {
    name: String,
    dept: i64,
    salary: i64,
    id: i64,
}

fn main() {
    let mut seed: i64 = 4242;
    let mut rand = |n: i64| {
        seed = seed * 16807 % 2147483647;
        seed % n
    };
    let mut a: Vec<Employee> = Vec::with_capacity(150000);
    for i in 1..=150000i64 {
        let mut c = String::new();
        for _ in 0..8 {
            c.push((65 + rand(26)) as u8 as char);
        }
        let dept = rand(40) + 1;
        let salary = rand(90000) + 1000;
        a.push(Employee { name: c, dept, salary, id: i });
    }
    let t = Instant::now();
    a.sort_by(|p, q| {
        if p.dept != q.dept {
            p.dept.cmp(&q.dept)
        } else if p.salary != q.salary {
            q.salary.cmp(&p.salary)
        } else {
            p.name.cmp(&q.name)
        }
    });
    let mut chk: i64 = 0;
    for e in &a {
        chk = (chk * 31 + e.id) % 1000000007;
    }
    let ok = chk == 773935306 && a[0].id == 84060 && a[0].name == "SOFHDANH" && a[75000].id == 10853
        && a[149999].id == 11664;
    println!("asort_codeblock {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
