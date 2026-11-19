# bench/string_concatenation.prg in Python: short strings joined with +, 3,000,000 strings of seven parts picked by
# index from small tables, each one's length and last character read; every string dies at once.
from bench import report, start

A = ["alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta"]
B = ["red", "green", "blue", "cyan", "magenta"]


def run():
    n = 0
    for i in range(1, 3000001):
        s = A[i % 7] + "-" + B[i % 5] + "/" + A[i % 3] + ":" + B[i % 4]
        n += len(s) + ord(s[-1])
    return n == 378692857


t = start()
report("string_concatenation", t, run())
