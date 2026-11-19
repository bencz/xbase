# bench/substr_massivo.prg in Python: slices of a large text, 6,000,000 pieces of 1 to 20 characters cut at spread
# positions of a 1,000,000-character text (made a character at a time in a list joined), their first and last
# characters read.
from bench import report, start

seed = 5


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def run(c):
    n = 0
    for i in range(1, 6000001):
        p = (i * 7919) % 999970
        s = c[p:p + i % 20 + 1]
        n += ord(s[0]) + ord(s[-1]) + len(s)
    return n == 994529563


text = "".join([chr(33 + rand(90)) for _ in range(1000000)])
t = start()
report("substr_massivo", t, run(text))
