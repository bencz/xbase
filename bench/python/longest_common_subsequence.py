# bench/longest_common_subsequence.prg in Python: longest common subsequence, its length for 600 pairs of 200 to 300
# letters from a 4-letter alphabet (the second related to the first), by dynamic programming over two rows, the letters
# split into lists first.
from bench import report, start

seed = 31


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def lcs(s, u):
    m, n = len(s), len(u)
    a_s, a_u = [None] * m, [None] * n
    for i in range(m):
        a_s[i] = s[i]
    for j in range(n):
        a_u[j] = u[j]
    prev, cur = [0] * (n + 1), [0] * (n + 1)
    for i in range(m):
        c = a_s[i]
        for j in range(n):
            if c == a_u[j]:
                cur[j + 1] = prev[j] + 1
            else:
                cur[j + 1] = max(prev[j + 1], cur[j])
        prev, cur = cur, prev
    return prev[n]


def pair(n):
    s = u = ""
    for _ in range(n):
        s += chr(65 + rand(4))
    for i in range(n):
        if rand(4) != 0:
            u += s[i]
        if rand(5) == 0:
            u += chr(65 + rand(4))
    return s, u


def run(pairs):
    n = 0
    for p in pairs:
        n += lcs(p[0], p[1])
    return n == 118188 and lcs("ABCBDAB", "BDCABA") == 4


pairs = [pair(200 + rand(101)) for _ in range(600)]
t = start()
report("longest_common_subsequence", t, run(pairs))
