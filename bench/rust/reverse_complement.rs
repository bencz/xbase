// bench/reverse_complement.prg in Rust: the output of fasta for n = 2,500,000 (25 MB, made with the benchmark's LCG
// before the clock starts) read section by section a byte at a time, each sequence joined into a String,
// reverse-complemented through the IUB table (indexed by the byte) and written again in lines of 60 after its header.
use std::time::Instant;

const IM: i64 = 139968;
const IA: i64 = 3877;
const IC: i64 = 29573;
const LINE: usize = 60;

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

fn random_seq(seed: &mut i64, sym: &str, prob: &[f64], n: usize) -> String {
    let sym = sym.as_bytes();
    let mut cum = Vec::with_capacity(prob.len());
    let mut acc = 0.0;
    for p in prob {
        acc += p;
        cum.push(acc);
    }
    let mut out = String::new();
    for done in (0..n).step_by(LINE) {
        for _ in 0..(n - done).min(LINE) {
            *seed = (*seed * IA + IC) % IM;
            let r = *seed as f64 / IM as f64;
            let mut j = 0;
            while j < cum.len() - 1 && r >= cum[j] {
                j += 1;
            }
            out.push(char::from(sym[j]));
        }
        out.push('\n');
    }
    out
}

// The three sections of fasta for n: the ALU sequence over 2n characters, 3n IUB codes, 5n Homo sapiens bases.
fn fasta(n: usize) -> String {
    let mut seed = 42;
    ">ONE Homo sapiens alu\n".to_string() + &repeat_seq(2 * n) + ">TWO IUB ambiguity codes\n"
        + &random_seq(&mut seed, "acgtBDHKMNRSVWY",
            &[0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02], 3 * n)
        + ">THREE Homo sapiens frequency\n"
        + &random_seq(&mut seed, "acgt", &[0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008],
            5 * n)
}

// The sequence reverse-complemented, in lines of 60.
fn rev_comp(seq: &str, comp: &[u8; 256]) -> String {
    let mut out = String::new();
    let mut col = 0;
    for &b in seq.as_bytes().iter().rev() {
        out.push(char::from(comp[b as usize]));
        col += 1;
        if col == LINE {
            out.push('\n');
            col = 0;
        }
    }
    if col > 0 {
        out.push('\n');
    }
    out
}

fn main() {
    let input = fasta(2500000);
    let t = Instant::now();
    let mut comp = [0u8; 256];
    for (i, c) in comp.iter_mut().enumerate() {
        *c = i as u8;
    }
    for (f, to) in "ACBDGHKMNSRUTWVYacbdghkmnsrutwvy".bytes().zip("TGVHCDMKNSYAABWRTGVHCDMKNSYAABWR".bytes()) {
        comp[f as usize] = to;
    }
    let bytes = input.as_bytes();
    let mut out = String::new();
    let mut seq = String::new();
    let mut i = 0;
    while i < bytes.len() {
        let c = bytes[i];
        if c == b'>' {
            if !seq.is_empty() {
                out += &rev_comp(&seq, &comp);
                seq = String::new();
            }
            loop {
                let c = bytes[i];
                i += 1;
                out.push(char::from(c));
                if c == b'\n' {
                    break;
                }
            }
        } else {
            if c != b'\n' {
                seq.push(char::from(c));
            }
            i += 1;
        }
    }
    out += &rev_comp(&seq, &comp);
    let two = out.find(">TWO").unwrap_or(0);
    let three = out.find(">THREE").unwrap_or(0);
    let ok = out.len() == 25416745
        && &out[22..82] == "GTAGCTGGGATTACAGGCGCGCGCCACCACGCCCGGCTAATTTTTGTATTTTTAGTAGAG"
        && &out[two + 25..two + 85] == "TTTATTYSTTGTATCCCTMWCGAACTAVGYHTATANTAWCARAWTTTVCTSTKDNTATHA"
        && &out[three + 30..three + 90] == "GTAATTTCTCTCGGCTATTAGATTTGAACATATACACAGGCCAGATGATCTGGAAAAATC"
        && out.ends_with("CTATAACTAATATATTCGTC\n");
    println!("reverse_complement {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
