# bench/towers_of_hanoi.prg in Python: recursion and nested lists, 20 disks moved between three pegs (lists with a
# height each), every move checked to put a smaller disk on a larger one, 4 times.
from bench import report, start


def hanoi(n, src, dst, via, peg, top, bad):
    moves = 0
    if n > 0:
        moves = hanoi(n - 1, src, via, dst, peg, top, bad)
        d = peg[src][top[src] - 1]
        top[src] -= 1
        if top[dst] > 0 and peg[dst][top[dst] - 1] < d:
            bad[0] += 1
        top[dst] += 1
        peg[dst][top[dst] - 1] = d
        moves += 1 + hanoi(n - 1, via, dst, src, peg, top, bad)
    return moves


def run():
    disks, ok = 20, True
    for _ in range(4):
        peg = [[None] * disks for _ in range(3)]
        for i in range(disks):
            peg[0][i] = disks - i
        top, bad = [disks, 0, 0], [0]
        moves = hanoi(disks, 0, 2, 1, peg, top, bad)
        ok = ok and moves == (1 << disks) - 1 and bad[0] == 0 and top[2] == disks
        for i in range(disks):
            ok = ok and peg[2][i] == disks - i
    return ok


t = start()
report("towers_of_hanoi", t, run())
