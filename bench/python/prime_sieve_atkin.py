# bench/prime_sieve_atkin.prg in Python: the sieve of Atkin, the primes up to 2,000,000 from the three quadratic forms
# 4x^2+y^2, 3x^2+y^2 and 3x^2-y^2 taken modulo 12 (toggling a list of booleans), then the multiples of the squares of
# primes struck out; 12 times. The list keeps the .prg's 1-based indexes (element 0 unused).
from bench import report, start


def run():
    limit, ok = 2000000, True
    a = [False] * (limit + 1)
    for _ in range(12):
        for i in range(limit + 1):
            a[i] = False
        x = 1
        while x * x <= limit:
            y = 1
            while y * y <= limit:
                n = 4 * x * x + y * y
                if n <= limit:
                    r = n % 12
                    if r == 1 or r == 5:
                        a[n] = not a[n]
                n = 3 * x * x + y * y
                if n <= limit and n % 12 == 7:
                    a[n] = not a[n]
                if x > y:
                    n = 3 * x * x - y * y
                    if n <= limit and n % 12 == 11:
                        a[n] = not a[n]
                y += 1
            x += 1
        i = 5
        while i * i <= limit:
            if a[i]:
                k = i * i
                for j in range(k, limit + 1, k):
                    a[j] = False
            i += 1
        count = 2
        for i in range(5, limit + 1):
            if a[i]:
                count += 1
        ok = ok and count == 148933
    return ok


t = start()
report("prime_sieve_atkin", t, run())
