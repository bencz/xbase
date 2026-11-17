# bench/gc.prg in Python: 40,000 slots holding strings of 16 to 2,000 characters and small records of them, replaced at
# random 3,000,000 times, while 20,000 records that live to the end are kept beside them.
from bench import report, start

seed = 7


def rand(n):
    global seed
    seed = (seed * 1103515245 + 12345) % 2147483648
    return (seed // 65536) % n


def run():
    slots, kept = [None] * 40000, []
    for i in range(1, 3000001):
        n = rand(len(slots))
        if rand(4) == 0:
            x = [chr(65 + i % 26) * (16 + rand(200)), i, " " * rand(100)]
        else:
            x = chr(97 + i % 26) * (16 + rand(2000))
        slots[n] = x
        if i % 150 == 0:
            kept.append([i, "%10d" % i, "k" * 64])
    check = 0
    for x in slots:
        if isinstance(x, str):
            check += len(x)
        elif isinstance(x, list):
            check += len(x[0]) + len(x[2])
    return len(kept) == 20000 and check > 0


t = start()
report("gc", t, run())
