# bench/fasta.prg in Python: the fasta benchmark for n = 1,500,000, the ALU sequence repeated over 2n characters, then
# 3n IUB codes and 5n Homo sapiens bases picked from cumulative probabilities with the benchmark's LCG (seed 42), in
# lines of 60, appended to one string of 15 MB. Each section is built a character at a time in a list joined at its
# end (PyPy copies a string on every +=). Checked: its length, the generator's last state, the sum of the indexes
# picked, the end of the first and of the last section.
from bench import report, start

IM, IA, IC, LINE = 139968, 3877, 29573, 60
ALU = ("GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTC"
       "AGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCG"
       "TGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGG"
       "AGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA")


def repeat_seq(n):
    k, pos, out = len(ALU), 0, []
    for done in range(0, n, LINE):
        for _ in range(min(n - done, LINE)):
            out.append(ALU[pos])
            pos += 1
            if pos >= k:
                pos = 0
        out.append("\n")
    return "".join(out)


def random_seq(state, sym, prob, n):
    n_sym, cum, acc, seed, picks, out = len(prob), [], 0, state[0], 0, []
    for p in prob:
        acc += p
        cum.append(acc)
    for done in range(0, n, LINE):
        for _ in range(min(n - done, LINE)):
            seed = (seed * IA + IC) % IM
            r = seed / IM
            j = 0
            while j < n_sym - 1 and r >= cum[j]:
                j += 1
            picks += j
            out.append(sym[j])
        out.append("\n")
    state[0] = seed
    state[1] += picks
    return "".join(out)


def run():
    n, state, out = 1500000, [42, 0], ""
    out += ">ONE Homo sapiens alu\n"
    out += repeat_seq(2 * n)
    out += ">TWO IUB ambiguity codes\n"
    out += random_seq(state, "acgtBDHKMNRSVWY",
                      [0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02], 3 * n)
    out += ">THREE Homo sapiens frequency\n"
    out += random_seq(state, "acgt", [0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008], 5 * n)
    two = out.find(">TWO")
    return (len(out) == 15250077 and state[0] == 28650 and state[1] == 25406718 and two + 1 == 3050023
            and out[two - 21:two] == "TGGGCGACAGAGCGAGACTC\n" and out[-21:] == "acacatatagttttatacaa\n")


t = start()
report("fasta", t, run())
