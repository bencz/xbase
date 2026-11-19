# bench/work_stealing.prg in Python: fork-join over an unbalanced tree (as the unbalanced tree search benchmark): the
# root and the next four levels have 8 children each, every node below has 5 children or none as its hash says
# (2,859,034 nodes in all), each node hashed 40 times. The four levels below the root are tasks on a pool of threads,
# 4,680 of them; a task waits for its children's results in turn (one no worker has taken yet is cancelled and runs
# inline, as a fork-join's join), the levels below run inside the task, and idle workers take the queued tasks. The
# threads share one interpreter lock, so they do not run the hashing in parallel. The counters the .prg's Visit() takes
# by reference are a two-element list. The node count and the sum of the hashes must be the tree's.
from concurrent.futures import ThreadPoolExecutor
import os
import sys

from bench import report, start

sys.setrecursionlimit(100000)


def hash_of(s):
    for _ in range(40):
        s = s * 16807 % 2147483647
    return s


def visit(s, d, acc):
    h = hash_of(s)
    acc[0] += 1
    acc[1] += h % 1000
    if d < 5 or h % 1000 < 198:
        for k in range(1, (8 if d < 5 else 5) + 1):
            visit((s * 16807 + k * 48271) % 2147483647, d + 1, acc)


def branch(pool, s, d):
    if d >= 4:
        acc = [0, 0]
        visit(s, d, acc)
        return acc
    h = hash_of(s)
    args = [((s * 16807 + k * 48271) % 2147483647, d + 1) for k in range(1, 9)]
    tasks = [pool.submit(branch, pool, c, e) for c, e in args]
    count = total = 0
    for k in range(7, -1, -1):
        a = branch(pool, *args[k]) if tasks[k].cancel() else tasks[k].result()
        count += a[0]
        total += a[1]
    return [count + 1, total + h % 1000]


t = start()
with ThreadPoolExecutor(max_workers=os.cpu_count()) as executor:
    result = branch(executor, 19937, 0)
report("work_stealing", t, result == [2859034, 1428166988])
