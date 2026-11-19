# bench/index.prg in Python: a table of 200,000 records with random 10-digit keys, an order built on them, 100,000
# random seeks (soft: to the first key not below), a pass through the order, 20,000 records appended with the order
# kept up to date. Python has no DBF or NTX: the records are fixed-width (a deletion flag, K of 10 characters, N of
# 10) in "bench_index.dat", a file the program creates in its current directory (written through a buffer, read back a
# record at a time by position) and removes at the end; the order is a list of (key, record number) kept sorted in
# memory (a seek is bisect_left(), an append is insort()).
import bisect
import os

from bench import report, start

PATH = "bench_index.dat"
REC = 21
seed = 7


def rand(n):
    global seed
    seed = (seed * 1103515245 + 12345) % 2147483648
    return seed // 65536 % n


def record(f, recno):
    f.seek((recno - 1) * REC)
    return f.read(REC)


def run():
    n_recs, hits, count, prev, ok = 200000, 0, 0, b"", True
    with open(PATH, "w+b") as f:
        for i in range(1, n_recs + 1):
            f.write(b" %010d%10d" % (rand(100000000), i))
        f.flush()
        order = [(record(f, r)[1:11], r) for r in range(1, n_recs + 1)]
        order.sort()
        for _ in range(100000):
            key = b"%010d" % rand(100000000)
            p = bisect.bisect_left(order, (key,))
            if p < len(order):
                record(f, order[p][1])
                if order[p][0] == key:
                    hits += 1
        for _, r in order:
            k = record(f, r)[1:11]
            ok = ok and k >= prev
            prev = k
            count += 1
        ok = ok and count == n_recs
        f.seek(0, os.SEEK_END)
        for i in range(1, 20001):
            key = b"%010d" % rand(100000000)
            f.write(b" " + key + b"%10d" % (n_recs + i))
            bisect.insort(order, (key, n_recs + i))
        f.flush()
        count = 0
        for _, r in order:
            record(f, r)
            count += 1
    return ok and count == n_recs + 20000 and hits >= 0


t = start()
try:
    result = run()
finally:
    os.remove(PATH)
report("index", t, result)
