# bench/levenshtein.prg in Python: edit distance, the Levenshtein distance of 10,000 pairs of strings of 30 to 60
# letters (the second one the first with random edits), by dynamic programming over two rows, the letters read by
# index.
from bench import report, start

seed = 1001


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def distance(s, u):
    m, n = len(s), len(u)
    prev, cur = list(range(n + 1)), [0] * (n + 1)
    for i in range(1, m + 1):
        cur[0] = i
        c = s[i - 1]
        for j in range(1, n + 1):
            cur[j] = min(min(prev[j], cur[j - 1]) + 1, prev[j - 1] + (0 if c == u[j - 1] else 1))
        prev, cur = cur, prev
    return prev[n]


def pair(n):
    s = u = ""
    for _ in range(n):
        s += chr(97 + rand(4))
    for i in range(n):
        k = rand(10)
        if k == 0:
            u += chr(97 + rand(4))
        elif k == 1:
            u += s[i] + chr(97 + rand(4))
        elif k != 2:
            u += s[i]
    return s, u


def run(pairs):
    n = 0
    for p in pairs:
        n += distance(p[0], p[1])
    return n == 108309 and distance("kitten", "sitting") == 3


pairs = [pair(30 + rand(31)) for _ in range(10000)]
t = start()
report("levenshtein", t, run(pairs))
