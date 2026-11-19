# bench/set_operations.prg in Python: sets as dicts (each key its own value), two sets of 200,000 draws each from
# 0 .. 399,999, their union, intersection, difference and symmetric difference built as new dicts; 2 rounds with new
# members.
from bench import report, start

seed = 777


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def run():
    check = 0
    for _ in range(2):
        ha, hb = {}, {}
        for _ in range(200000):
            k = rand(400000)
            ha[k] = k
            k = rand(400000)
            hb[k] = k
        union, inter, diff, sym = {}, {}, {}, {}
        for k in ha.values():
            union[k] = k
            if k in hb:
                inter[k] = k
            else:
                diff[k] = k
                sym[k] = k
        for k in hb.values():
            union[k] = k
            if k not in ha:
                sym[k] = k
        check += len(ha) + len(hb) * 2 + len(union) * 3 + len(inter) * 5 + len(diff) * 7 + len(sym) * 11
    return (check == 8609885 and len(union) == 252664 and len(union) == len(inter) + len(sym)
            and len(ha) == len(inter) + len(diff))


t = start()
report("set_operations", t, run())
