# bench/sieve.prg in Python: the primes up to 2,000,000, 10 times.
from bench import report, start


def run():
    n, ok = 2000000, True
    a = [False] * (n + 1)
    for _ in range(10):
        count = 0
        for i in range(2, n + 1):
            a[i] = True
        a[1] = False
        i = 2
        while i * i <= n:
            if a[i]:
                for j in range(i * i, n + 1, i):
                    a[j] = False
            i += 1
        for i in range(1, n + 1):
            if a[i]:
                count += 1
        ok = ok and count == 148933
    return ok


t = start()
report("sieve", t, run())
