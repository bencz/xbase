# bench/relatorio_agregacao.prg in Python: a business report, 1,000,000 sales records (region, product, quantity, unit
# price in cents) made from a fixed seed, then timed: aggregated by region and by product in dicts keyed by code, the
# 500 products ordered by amount with the list's sort and a comparison function (ties by code; the .prg's block,
# through functools.cmp_to_key), and a report built with format() and its thousands separators exchanged for the
# .prg's Transform() @E pictures: a line per region (sales, quantity, amount) and the top 20 products.
from functools import cmp_to_key

from bench import report, start

seed = 12345
EUROPEAN = str.maketrans(",.", ".,")


def rand(n):
    global seed
    seed = seed * 16807 % 2147483647
    return seed % n


def less(p, q):
    return p[1] > q[1] or (p[1] == q[1] and p[0] < q[0])


BY_AMOUNT = cmp_to_key(lambda p, q: -1 if less(p, q) else (1 if less(q, p) else 0))


def european(v, width, dec):
    return format(v, ",." + str(dec) + "f").translate(EUROPEAN).rjust(width)


def run(sales):
    h_reg, h_prod, prod, rep, total = {}, {}, [], "", 0
    for r in sales:
        x = r[2] * r[3]
        if r[0] in h_reg:
            a = h_reg[r[0]]
            a[0] += 1
            a[1] += r[2]
            a[2] += x
        else:
            h_reg[r[0]] = [1, r[2], x]
        h_prod[r[1]] = h_prod.get(r[1], 0) + x
        total += x
    for k, x in h_prod.items():
        prod.append([k, x])
    prod.sort(key=BY_AMOUNT)
    h_reg = dict(sorted(h_reg.items()))
    for k, a in h_reg.items():
        rep += k + european(a[0], 9, 0) + european(a[1], 10, 0) + european(a[2] / 100, 18, 2) + "\n"
    for i in range(20):
        rep += prod[i][0] + european(prod[i][1] / 100, 18, 2) + "\n"
    lines = rep.split("\n")
    return (total == 525748851783 and len(rep) == 8 * 41 + 20 * 24 and len(h_prod) == 500
            and lines[0] == "R01  125.691 1.320.222    662.846.057,84"
            and lines[7] == "R08  124.683 1.310.936    656.013.155,38"
            and lines[8] == "P0303     11.438.921,18" and lines[27] == "P0360     11.021.303,35")


records = [["R" + "%02d" % (rand(8) + 1), "P" + "%04d" % (rand(500) + 1), rand(20) + 1, rand(100000) + 100]
           for _ in range(1000000)]
t = start()
report("relatorio_agregacao", t, run(records))
