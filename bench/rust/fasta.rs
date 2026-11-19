// bench/fasta.prg in Rust: the fasta benchmark for n = 1,500,000: the ALU sequence repeated over 2n characters, then
// 3n IUB codes and 5n Homo sapiens bases picked from cumulative probabilities with the benchmark's LCG (seed 42), in
// lines of 60, each section a String appended byte by byte, then appended to one String of 15 MB.
use std::hint::black_box;
use std::time::Instant;

const IM: i64 = 139968;
const IA: i64 = 3877;
const IC: i64 = 29573;
const LINE: usize = 60;

// n characters of the ALU sequence, cyclically, in lines of 60.
fn repeat_seq(n: usize) -> String {
    let alu = "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTC\
        AGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCG\
        TGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGG\
        AGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA"
        .as_bytes();
    let mut pos = 0;
    let mut out = String::new();
    for done in (0..n).step_by(LINE) {
        for _ in 0..(n - done).min(LINE) {
            out.push(char::from(alu[pos]));
            pos += 1;
            if pos == alu.len() {
                pos = 0;
            }
        }
        out.push('\n');
    }
    out
}

// n symbols drawn with the LCG in state.0 (the sum of their indexes, from 0, added to state.1), in lines of 60.
fn random_seq(state: &mut (i64, i64), sym: &str, prob: &[f64], n: usize) -> String {
    let sym = sym.as_bytes();
    let mut cum = Vec::with_capacity(prob.len());
    let mut acc = 0.0;
    for p in prob {
        acc += p;
        cum.push(acc);
    }
    let (mut seed, mut picks) = (state.0, 0i64);
    let mut out = String::new();
    for done in (0..n).step_by(LINE) {
        for _ in 0..(n - done).min(LINE) {
            seed = (seed * IA + IC) % IM;
            let r = seed as f64 / IM as f64;
            let mut j = 0;
            while j < cum.len() - 1 && r >= cum[j] {
                j += 1;
            }
            picks += j as i64;
            out.push(char::from(sym[j]));
        }
        out.push('\n');
    }
    state.0 = seed;
    state.1 += picks;
    out
}

fn main() {
    let t = Instant::now();
    let n: usize = black_box(1500000);
    let mut state = (42i64, 0i64);
    let mut out = String::new();
    out += ">ONE Homo sapiens alu\n";
    out += &repeat_seq(2 * n);
    out += ">TWO IUB ambiguity codes\n";
    out += &random_seq(&mut state, "acgtBDHKMNRSVWY",
        &[0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02], 3 * n);
    out += ">THREE Homo sapiens frequency\n";
    out += &random_seq(&mut state, "acgt", &[0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008],
        5 * n);
    let two = out.find(">TWO").map_or(0, |p| p + 1);
    let ok = out.len() == 15250077 && state.0 == 28650 && state.1 == 25406718 && two == 3050023
        && &out[two - 22..two - 1] == "TGGGCGACAGAGCGAGACTC\n" && out.ends_with("acacatatagttttatacaa\n");
    println!("fasta {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
