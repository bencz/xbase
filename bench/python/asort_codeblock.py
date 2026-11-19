# bench/asort_codeblock.prg in Python: the list's sort with a comparison function over records, 150,000 employees
# [name, department, salary, id] made from a fixed seed, ordered by department, then salary descending, then name,
# through a function that compares three keys (the .prg's block, through functools.cmp_to_key; the sort is stable, so
# equal records keep their order). The order is checked with a running hash of the ids.
from functools import cmp_to_key

from bench import report, start

seed = 4242


def rand(n):
    global seed
    seed = seed * 16807 % 2147483647
    return seed % n


def less(p, q):
    if p[1] != q[1]:
        return p[1] < q[1]
    if p[2] != q[2]:
        return p[2] > q[2]
    return p[0] < q[0]


ORDER = cmp_to_key(lambda p, q: -1 if less(p, q) else (1 if less(q, p) else 0))


def make():
    a = []
    for i in range(1, 150001):
        c = ""
        for _ in range(8):
            c += chr(65 + rand(26))
        a.append([c, rand(40) + 1, rand(90000) + 1000, i])
    return a


a = make()
t = start()
a.sort(key=ORDER)
chk = 0
for r in a:
    chk = (chk * 31 + r[3]) % 1000000007
report("asort_codeblock", t, chk == 773935306 and a[0][3] == 84060 and a[0][0] == "SOFHDANH" and a[75000][3] == 10853
       and a[149999][3] == 11664)
