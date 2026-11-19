# bench/reverse_complement.prg in Python: strings read and built a character at a time, the output of fasta for
# n = 2,500,000 (25 MB, made with the benchmark's LCG before the clock starts) read section by section, each sequence
# joined, reverse-complemented through the IUB table (a list indexed by ord()) and written again in lines of 60 after
# its header. The strings built a character at a time are lists of characters joined (PyPy copies a string on every
# +=).
from bench import report, start

IM, IA, IC, LINE = 139968, 3877, 29573, 60
ALU = ("GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTC"
       "AGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCG"
       "TGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGG"
       "AGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA")


def rev_comp(seq, comp, out):
    col = 0
    for i in range(len(seq) - 1, -1, -1):
        out.append(comp[ord(seq[i])])
        col += 1
        if col == LINE:
            out.append("\n")
            col = 0
    if col > 0:
        out.append("\n")


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


def random_seq(seed_box, sym, prob, n):
    n_sym, cum, acc, seed, out = len(prob), [], 0, seed_box[0], []
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
            out.append(sym[j])
        out.append("\n")
    seed_box[0] = seed
    return "".join(out)


def fasta(n):
    seed = [42]
    return (">ONE Homo sapiens alu\n" + repeat_seq(2 * n) + ">TWO IUB ambiguity codes\n"
            + random_seq(seed, "acgtBDHKMNRSVWY",
                         [0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02],
                         3 * n)
            + ">THREE Homo sapiens frequency\n"
            + random_seq(seed, "acgt", [0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008], 5 * n))


def run(src):
    comp, out, seq = [chr(i) for i in range(256)], [], []
    for f, to in zip("ACBDGHKMNSRUTWVYacbdghkmnsrutwvy", "TGVHCDMKNSYAABWRTGVHCDMKNSYAABWR"):
        comp[ord(f)] = to
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == ">":
            if seq:
                rev_comp("".join(seq), comp, out)
                seq = []
            while True:
                c = src[i]
                i += 1
                if c == "\n":
                    break
                out.append(c)
            out.append(c)
        else:
            if c != "\n":
                seq.append(c)
            i += 1
    rev_comp("".join(seq), comp, out)
    res = "".join(out)
    two, three = res.find(">TWO"), res.find(">THREE")
    return (len(res) == 25416745 and res[22:82] == "GTAGCTGGGATTACAGGCGCGCGCCACCACGCCCGGCTAATTTTTGTATTTTTAGTAGAG"
            and res[two + 25:two + 85] == "TTTATTYSTTGTATCCCTMWCGAACTAVGYHTATANTAWCARAWTTTVCTSTKDNTATHA"
            and res[three + 30:three + 90] == "GTAATTTCTCTCGGCTATTAGATTTGAACATATACACAGGCCAGATGATCTGGAAAAATC"
            and res[-21:] == "CTATAACTAATATATTCGTC\n")


data = fasta(2500000)
t = start()
report("reverse_complement", t, run(data))
