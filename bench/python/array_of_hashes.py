# bench/array_of_hashes.prg in Python: records as dicts, 120,000 dicts of four fields made from a fixed-seed generator,
# totals per group read by field name, a count of the records that pass a test, and the list sorted by a field with a
# comparison function (the .prg's block, through functools.cmp_to_key).
from functools import cmp_to_key

from bench import report, start

seed = 31337
BY_SCORE = cmp_to_key(lambda x, y: -1 if x["score"] < y["score"] else (1 if y["score"] < x["score"] else 0))


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def run():
    a, totals, n_pass, total = [None] * 120000, [0] * 50, 0, 0
    for i in range(len(a)):
        a[i] = {"id": i + 1, "name": "N" + "%05d" % rand(100000), "score": rand(1000), "group": rand(50) + 1}
    for r in a:
        totals[r["group"] - 1] += r["score"]
        if r["score"] >= 500 and r["name"][:2] == "N1":
            n_pass += 1
    a.sort(key=BY_SCORE)
    for i in range(0, len(a), 1000):
        total += a[i]["score"]
    return (totals[0] == 1239678 and totals[49] == 1170281 and n_pass == 5950 and total == 59429
            and a[0]["score"] == 0 and a[119999]["score"] == 999)


t = start()
report("array_of_hashes", t, run())
