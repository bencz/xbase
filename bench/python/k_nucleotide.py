# bench/k_nucleotide.prg in Python: dicts keyed by strings, the Homo sapiens section of fasta for n = 100,000 (500,000
# bases, upper case, made with the benchmark's LCG before the clock starts), every k-mer of it counted in a dict for
# k = 1, 2, 3, 4, 6, 12 and 18 (a slice per position as the key), the 1- and 2-mers sorted by count with the list's
# sort, and the counts of five given sequences.
from bench import report, start

IM, IA, IC = 139968, 3877, 29573


def frames(s, k):
    h = {}
    for i in range(len(s) - k + 1):
        key = s[i:i + k]
        if key in h:
            h[key] += 1
        else:
            h[key] = 1
    return h


def count_of(s, seq):
    return frames(s, len(seq)).get(seq, 0)


def sorted_frames(s, k):
    pairs = list(frames(s, k).items())
    pairs.sort(key=lambda p: (-p[1], p[0]))
    return " ".join(key + " " + str(count) for key, count in pairs)


def sequence(n):
    seed, cum, acc, out = 42, [], 0, []
    for p in (0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008):
        acc += p
        cum.append(acc)
    for _ in range(3 * n):
        seed = (seed * IA + IC) % IM
    for _ in range(5 * n):
        seed = (seed * IA + IC) % IM
        r = seed / IM
        j = 0
        while j < 3 and r >= cum[j]:
            j += 1
        out.append("ACGT"[j])
    return "".join(out)


s = sequence(100000)
t = start()
ok = (sorted_frames(s, 1) == "A 151655 T 150452 C 99156 G 98737"
      and sorted_frames(s, 2) == "AA 45992 TA 45599 AT 45574 TT 45299 CA 30129 AC 30096 AG 29993 GA 29935 CT 29857 "
      "TC 29832 GT 29722 TG 29721 GC 19617 CC 19611 CG 19559 GG 19463"
      and count_of(s, "GGT") == 5871 and count_of(s, "GGTA") == 1769 and count_of(s, "GGTATT") == 185
      and count_of(s, "GGTATTTTAATT") == 4 and count_of(s, "GGTATTTTAATTTATAGT") == 4)
report("k_nucleotide", t, ok)
