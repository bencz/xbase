# bench/indexacao_simulada.prg in Python: an index kept in memory, as an order is, 150,000 entries (an 8-digit key and
# the record number, one string each) made from a fixed seed, then timed: sorted with the list's sort, 100,000 seeks by
# binary search written out (the first entry not below the key, compared over the key's length as the .prg's < does
# with SET EXACT OFF; a hit when its key is the one sought), 3,000 entries inserted in place (binary search and
# insert()), and the order checked.
from bench import report, start

seed = 99991


def rand(n):
    global seed
    seed = seed * 16807 % 2147483647
    return seed % n


def lower_bound(a, k):
    lo, hi, n = 1, len(a) + 1, len(k)
    while lo < hi:
        m = (lo + hi) // 2
        if a[m - 1][:n] < k:
            lo = m + 1
        else:
            hi = m
    return lo


def run(a, n_recs):
    hits = total = chk = 0
    ok = True
    a.sort()
    for _ in range(100000):
        k = "C" + "%08d" % rand(10000000)
        p = lower_bound(a, k)
        if p <= len(a) and a[p - 1][:9] == k:
            hits += 1
            total += int(a[p - 1][-6:])
    for i in range(1, 3001):
        k = "C" + "%08d" % rand(10000000) + "%06d" % (n_recs + i)
        a.insert(lower_bound(a, k) - 1, k)
    for i in range(1, len(a)):
        ok = ok and a[i - 1] < a[i]
    for i in range(0, len(a), 997):
        chk = (chk * 31 + int(a[i][-6:])) % 1000000007
    return (ok and hits == 1426 and total == 107771969 and chk == 409559053 and a[0] == "C00000040027737"
            and a[-1] == "C09999947069110")


recs = 150000
entries = ["C" + "%08d" % rand(10000000) + "%06d" % i for i in range(1, recs + 1)]
t = start()
report("indexacao_simulada", t, run(entries, recs))
