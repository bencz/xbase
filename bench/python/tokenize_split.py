# bench/tokenize_split.prg in Python: splitting lines into fields, 200,000 comma-separated lines of 6 fields cut with
# find() and slices (the .prg's At() and SubStr(), its own Split) into lists, the numeric fields read with float() (its
# Val(); its Rand() numbers print with two decimals, "12345.00"); 2 rounds.
from bench import report, start

seed = 64


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def split(c, sep):
    a = []
    while (n := c.find(sep) + 1) > 0:
        a.append(c[:n - 1])
        c = c[n:]
    a.append(c)
    return a


def run(lines):
    n = 0
    for _ in range(2):
        for c in lines:
            a = split(c, ",")
            n += float(a[0]) + float(a[2]) + len(a) + len(a[4])
    return n == 20190509958


rows = [None] * 200000
for _i in range(len(rows)):
    rows[_i] = ("%.2f" % rand(100000) + ",name" + str(_i + 1) + "," + "%.2f" % rand(1000) + ",," + "z" * rand(12)
                + ",end")
t = start()
report("tokenize_split", t, run(rows))
