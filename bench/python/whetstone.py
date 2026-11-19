# bench/whetstone.prg in Python: Whetstone (Curnow and Wichmann's synthetic benchmark, its modules scaled by a loop
# count of 5,000): list elements, a list as a parameter, conditional jumps, integer arithmetic, procedure calls,
# list references, and sqrt(exp(log(x) / t)); modules 1, 7 and 10 left out as the .prg does. The list e1 keeps the
# .prg's 1-based indexes (element 0 unused); P3's parameter by reference is returned instead. The check is the sum of
# the values each module leaves.
import math

from bench import report, start

t, t1, t2 = 0.499975, 0.50025, 2.0
e1 = [None, 1.0, -1.0, -1.0, -1.0]
j = k = l = 0


def pa(e):
    n = 0
    while n < 6:
        e[1] = (e[1] + e[2] + e[3] - e[4]) * t
        e[2] = (e[1] + e[2] - e[3] + e[4]) * t
        e[3] = (e[1] - e[2] + e[3] + e[4]) * t
        e[4] = (-e[1] + e[2] + e[3] + e[4]) / t2
        n += 1


def p0():
    e1[j] = e1[k]
    e1[k] = e1[l]
    e1[l] = e1[j]


def p3(x, y):
    x1, y1 = x, y
    x1 = t * (x1 + y1)
    y1 = t * (x1 + y1)
    return (x1 + y1) / t2


def run():
    global j, k, l
    n_loop = 5000
    for _ in range(12 * n_loop):
        e1[1] = (e1[1] + e1[2] + e1[3] - e1[4]) * t
        e1[2] = (e1[1] + e1[2] - e1[3] + e1[4]) * t
        e1[3] = (e1[1] - e1[2] + e1[3] + e1[4]) * t
        e1[4] = (-e1[1] + e1[2] + e1[3] + e1[4]) * t
    check = e1[1] + e1[2] + e1[3] + e1[4]
    for _ in range(14 * n_loop):
        pa(e1)
    check += e1[1] + e1[2] + e1[3] + e1[4]
    j = 1
    for _ in range(345 * n_loop):
        if j == 1:
            j = 2
        else:
            j = 3
        if j > 2:
            j = 0
        else:
            j = 1
        if j < 1:
            j = 1
        else:
            j = 0
    check += j
    j, k, l = 1, 2, 3
    for _ in range(210 * n_loop):
        j = j * (k - j) * (l - k)
        k = l * k - (l - j) * k
        l = (l - k) * (k + j)
        e1[l - 1] = j + k + l
        e1[k - 1] = j * k * l
    check += j + k + l + e1[1] + e1[2] + e1[3] + e1[4]
    x = y = z = 1.0
    for _ in range(899 * n_loop):
        z = p3(x, y)
    check += x + y + z
    j, k, l = 1, 2, 3
    e1[1], e1[2], e1[3] = 1.0, 2.0, 3.0
    for _ in range(616 * n_loop):
        p0()
    check += e1[1] + e1[2] + e1[3] + e1[4]
    x = 0.75
    for _ in range(93 * n_loop):
        x = math.sqrt(math.exp(math.log(x) / t1))
    check += x
    return abs(check - 30.95582672351981) < 0.000000001


tm = start()
report("whetstone", tm, run())
