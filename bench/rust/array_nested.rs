// bench/array_nested.prg in Rust: a 100 x 100 x 100 cube of nested Vecs, filled through a[i][j][k], then 10 passes
// that change the cells from their neighbors along the last two dimensions and sum them.
use std::hint::black_box;
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let size: usize = black_box(100);
    let mut a = vec![vec![vec![0i64; size]; size]; size];
    for i in 1..=size {
        for j in 1..=size {
            for k in 1..=size {
                a[i - 1][j - 1][k - 1] = ((i * 31 + j * 17 + k * 7) % 101) as i64;
            }
        }
    }
    let mut sum: i64 = 0;
    for _ in 0..10 {
        for plane in a.iter_mut() {
            for j in 1..size {
                let (prev, cur) = plane.split_at_mut(j);
                let above = &prev[j - 1];
                let row = &mut cur[0];
                for k in 1..size {
                    row[k] = (row[k] + above[k] + row[k - 1]) % 1000;
                }
            }
        }
        for i in 0..size {
            for j in 0..size {
                for k in 0..size {
                    sum += a[i][j][k];
                }
            }
        }
    }
    let ok = sum == 4905893027;
    println!("array_nested {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
