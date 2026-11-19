// bench/dhrystone.prg in Rust: Dhrystone 2.1 as the classic program has it, 1,000,000 runs. The two records live in
// an array and refer to each other by index (the pointers of the classic); the procedures are not inlined, as when the
// classic is built from its two files and as xbase calls its functions; arrays and strings are indexed from 0, and
// Proc_7 stores its result through a reference as the classic does.
use std::time::Instant;

const RUNS: i64 = 1000000;

// The enumeration's values, as the .prg defines them.
type Enumeration = i64;
const IDENT_1: Enumeration = 0;
const IDENT_2: Enumeration = 1;
const IDENT_3: Enumeration = 2;
const IDENT_4: Enumeration = 3;
const IDENT_5: Enumeration = 4;

#[derive(Clone, Copy)]
struct Record {
    ptr_comp: usize,
    discr: Enumeration,
    enum_comp: Enumeration,
    int_comp: i64,
    str_comp: &'static str,
}

struct Globals {
    recs: [Record; 2],
    ptr_glob: usize,
    int_glob: i64,
    bool_glob: bool,
    ch_1_glob: u8,
    ch_2_glob: u8,
    arr_1_glob: [i64; 50],
    arr_2_glob: [[i64; 50]; 50],
}

#[inline(never)]
fn proc_7(int_1_par_val: i64, int_2_par_val: i64, int_par_ref: &mut i64) {
    let int_loc = int_1_par_val + 2;
    *int_par_ref = int_2_par_val + int_loc;
}

#[inline(never)]
fn func_1(g: &mut Globals, ch_1_par_val: u8, ch_2_par_val: u8) -> Enumeration {
    let ch_1_loc = ch_1_par_val;
    let ch_2_loc = ch_1_loc;
    if ch_2_loc != ch_2_par_val {
        return IDENT_1;
    }
    g.ch_1_glob = ch_1_loc;
    IDENT_2
}

#[inline(never)]
fn func_3(enum_par_val: Enumeration) -> bool {
    let enum_loc = enum_par_val;
    enum_loc == IDENT_3
}

#[inline(never)]
fn proc_6(g: &Globals, enum_val_par: Enumeration) -> Enumeration {
    let mut enum_ref_par = enum_val_par;
    if !func_3(enum_val_par) {
        enum_ref_par = IDENT_4;
    }
    if enum_val_par == IDENT_1 {
        enum_ref_par = IDENT_1;
    } else if enum_val_par == IDENT_2 {
        enum_ref_par = if g.int_glob > 100 { IDENT_1 } else { IDENT_4 };
    } else if enum_val_par == IDENT_3 {
        enum_ref_par = IDENT_2;
    } else if enum_val_par == IDENT_5 {
        enum_ref_par = IDENT_3;
    }
    enum_ref_par
}

#[inline(never)]
fn proc_3(g: &mut Globals, ptr_ref_par: &mut usize) {
    *ptr_ref_par = g.recs[g.ptr_glob].ptr_comp;
    let pg = g.ptr_glob;
    proc_7(10, g.int_glob, &mut g.recs[pg].int_comp);
}

#[inline(never)]
fn proc_1(g: &mut Globals, ptr_val_par: usize) {
    let next_record = g.recs[ptr_val_par].ptr_comp;
    g.recs[next_record] = g.recs[g.ptr_glob];
    g.recs[ptr_val_par].int_comp = 5;
    g.recs[next_record].int_comp = g.recs[ptr_val_par].int_comp;
    g.recs[next_record].ptr_comp = g.recs[ptr_val_par].ptr_comp;
    let mut p = g.recs[next_record].ptr_comp;
    proc_3(g, &mut p);
    g.recs[next_record].ptr_comp = p;
    if g.recs[next_record].discr == IDENT_1 {
        g.recs[next_record].int_comp = 6;
        g.recs[next_record].enum_comp = proc_6(g, g.recs[ptr_val_par].enum_comp);
        g.recs[next_record].ptr_comp = g.recs[g.ptr_glob].ptr_comp;
        let v = g.recs[next_record].int_comp;
        proc_7(v, 10, &mut g.recs[next_record].int_comp);
    } else {
        g.recs[ptr_val_par] = g.recs[g.recs[ptr_val_par].ptr_comp];
    }
}

#[inline(never)]
fn proc_2(g: &Globals, int_par_ref: &mut i64) {
    let mut int_loc = *int_par_ref + 10;
    let mut enum_loc = IDENT_2;
    loop {
        if g.ch_1_glob == b'A' {
            int_loc -= 1;
            *int_par_ref = int_loc - g.int_glob;
            enum_loc = IDENT_1;
        }
        if enum_loc == IDENT_1 {
            break;
        }
    }
}

#[inline(never)]
fn proc_4(g: &mut Globals) {
    let bool_loc = g.ch_1_glob == b'A';
    g.bool_glob = bool_loc || g.bool_glob;
    g.ch_2_glob = b'B';
}

#[inline(never)]
fn proc_5(g: &mut Globals) {
    g.ch_1_glob = b'A';
    g.bool_glob = false;
}

#[inline(never)]
fn proc_8(g: &mut Globals, int_1_par_val: i64, int_2_par_val: i64) {
    let int_loc = (int_1_par_val + 5) as usize;
    g.arr_1_glob[int_loc] = int_2_par_val;
    g.arr_1_glob[int_loc + 1] = g.arr_1_glob[int_loc];
    g.arr_1_glob[int_loc + 30] = int_loc as i64;
    for int_index in int_loc..=int_loc + 1 {
        g.arr_2_glob[int_loc][int_index] = int_loc as i64;
    }
    g.arr_2_glob[int_loc][int_loc - 1] += 1;
    g.arr_2_glob[int_loc + 20][int_loc] = g.arr_1_glob[int_loc];
    g.int_glob = 5;
}

#[inline(never)]
fn func_2(g: &mut Globals, str_1_par_ref: &str, str_2_par_ref: &str) -> bool {
    let mut int_loc: usize = 2;
    let mut ch_loc: u8 = 0;
    while int_loc <= 2 {
        if func_1(g, str_1_par_ref.as_bytes()[int_loc], str_2_par_ref.as_bytes()[int_loc + 1]) == IDENT_1 {
            ch_loc = b'A';
            int_loc += 1;
        }
    }
    if (b'W'..b'Z').contains(&ch_loc) {
        int_loc = 7;
    }
    if ch_loc == b'R' {
        return true;
    }
    if str_1_par_ref > str_2_par_ref {
        int_loc += 7;
        g.int_glob = int_loc as i64;
        return true;
    }
    false
}

fn main() {
    let t = Instant::now();
    let some = "DHRYSTONE PROGRAM, SOME STRING";
    let blank = Record { ptr_comp: 1, discr: IDENT_1, enum_comp: IDENT_1, int_comp: 0, str_comp: "" };
    let mut g = Globals {
        recs: [Record { ptr_comp: 1, discr: IDENT_1, enum_comp: IDENT_3, int_comp: 40, str_comp: some }, blank],
        ptr_glob: 0,
        int_glob: 0,
        bool_glob: false,
        ch_1_glob: b' ',
        ch_2_glob: b' ',
        arr_1_glob: [0; 50],
        arr_2_glob: [[0; 50]; 50],
    };
    let next_ptr_glob: usize = 1;
    let (mut int_1_loc, mut int_2_loc, mut int_3_loc) = (0i64, 0i64, 0i64);
    let mut enum_loc = IDENT_1;
    let str_1_loc = "DHRYSTONE PROGRAM, 1'ST STRING";
    let mut str_2_loc = "";
    g.arr_2_glob[8][7] = 10;
    for run_index in 1..=RUNS {
        proc_5(&mut g);
        proc_4(&mut g);
        int_1_loc = 2;
        int_2_loc = 3;
        str_2_loc = "DHRYSTONE PROGRAM, 2'ND STRING";
        enum_loc = IDENT_2;
        g.bool_glob = !func_2(&mut g, str_1_loc, str_2_loc);
        while int_1_loc < int_2_loc {
            int_3_loc = 5 * int_1_loc - int_2_loc;
            proc_7(int_1_loc, int_2_loc, &mut int_3_loc);
            int_1_loc += 1;
        }
        proc_8(&mut g, int_1_loc, int_3_loc);
        let pg = g.ptr_glob;
        proc_1(&mut g, pg);
        let mut ch_index = b'A';
        while ch_index <= g.ch_2_glob {
            if enum_loc == func_1(&mut g, ch_index, b'C') {
                enum_loc = proc_6(&g, IDENT_1);
                str_2_loc = "DHRYSTONE PROGRAM, 3'RD STRING";
                int_2_loc = run_index;
                g.int_glob = run_index;
            }
            ch_index += 1;
        }
        int_2_loc *= int_1_loc;
        int_1_loc = int_2_loc / int_3_loc;
        int_2_loc = 7 * (int_2_loc - int_3_loc) - int_1_loc;
        proc_2(&g, &mut int_1_loc);
    }
    let pg = g.recs[g.ptr_glob];
    let np = g.recs[next_ptr_glob];
    let ok = g.int_glob == 5 && g.bool_glob && g.ch_1_glob == b'A' && g.ch_2_glob == b'B' && g.arr_1_glob[8] == 7
        && g.arr_2_glob[8][7] == RUNS + 10 && pg.ptr_comp == next_ptr_glob && pg.discr == IDENT_1
        && pg.enum_comp == IDENT_3 && pg.int_comp == 17 && pg.str_comp == some && np.ptr_comp == next_ptr_glob
        && np.discr == IDENT_1 && np.enum_comp == IDENT_2 && np.int_comp == 18 && np.str_comp == some
        && int_1_loc == 5 && int_2_loc == 13 && int_3_loc == 7 && enum_loc == IDENT_2
        && str_1_loc == "DHRYSTONE PROGRAM, 1'ST STRING" && str_2_loc == "DHRYSTONE PROGRAM, 2'ND STRING";
    println!("dhrystone {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
