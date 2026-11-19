# bench/hash_chaves_longas.prg in Python: dicts with long string keys, 300,000 keys of 82 to 131 bytes that share a
# 66-byte prefix (so equal hashes compare whole keys), inserted, then looked up through other strings of the same text,
# and 300,000 missing keys of the same shape looked up.
from bench import report, start

seed = 99
PREFIX = "https://example.com/catalog/products/category/electronics/item?id="


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def run(keys, same, miss):
    h, total, n_miss = {}, 0, 0
    for i in range(len(keys)):
        h[keys[i]] = i + 1
    for i in range(len(same) - 1, -1, -1):
        total += h[same[i]]
    for k in miss:
        if k not in h:
            n_miss += 1
    return len(h) == 300000 and total == 45000150000 and n_miss == 300000


keys, same, miss = [None] * 300000, [None] * 300000, [None] * 300000
for _i in range(1, 300001):
    _n = rand(1000000)
    keys[_i - 1] = PREFIX + "%07d" % _i + "&v=" + "%06d" % _n + "x" * (_i % 50)
    same[_i - 1] = PREFIX + "%07d" % _i + "&v=" + "%06d" % _n + "x" * (_i % 50)
    miss[_i - 1] = PREFIX + "%07d" % _i + "&w=" + "%06d" % _n + "x" * (_i % 50)
t = start()
report("hash_chaves_longas", t, run(keys, same, miss))
