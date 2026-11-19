# bench/binary_trees.prg in Python: binary trees of two-element lists made and walked, depths 4 to 14 (a stretch tree
# of depth 15, one of depth 14 that lives throughout, 2^(14 - d + 4) trees of each even depth d).
from bench import report, start


def make(d):
    if d == 0:
        return [None, None]
    return [make(d - 1), make(d - 1)]


def check(node):
    if node[0] is None:
        return 1
    return 1 + check(node[0]) + check(node[1])


def run():
    max_depth = 14
    total = check(make(max_depth + 1))
    long_lived = make(max_depth)
    for d in range(4, max_depth + 1, 2):
        for _ in range(1 << (max_depth - d + 4)):
            total += check(make(d))
    return total + check(long_lived)


t = start()
report("binary_trees", t, run() == 3222190)
