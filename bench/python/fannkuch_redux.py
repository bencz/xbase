# bench/fannkuch_redux.prg in Python: small integer lists permuted and reversed in place, the pancake flips of every
# permutation of 9 elements (the checksum and the most flips), 4 times. The lists keep the .prg's 1-based indexes
# (element 0 unused).
from bench import report, start


def fannkuch(n):
    perm, perm1, count = [0] * (n + 1), [0] * (n + 2), [0] * (n + 1)
    for i in range(1, n + 1):
        perm1[i] = i - 1
    checksum = max_flips = perm_count = 0
    r = n
    while True:
        while r != 1:
            count[r] = r
            r -= 1
        for i in range(1, n + 1):
            perm[i] = perm1[i]
        flips = 0
        k = perm[1]
        while k != 0:
            i, j = 1, k + 1
            while i < j:
                perm[i], perm[j] = perm[j], perm[i]
                i += 1
                j -= 1
            flips += 1
            k = perm[1]
        max_flips = max(max_flips, flips)
        checksum += flips if (perm_count & 1) == 0 else -flips
        while True:
            if r == n:
                return checksum, max_flips
            perm0 = perm1[1]
            for i in range(1, r + 1):
                perm1[i] = perm1[i + 1]
            perm1[r + 1] = perm0
            count[r + 1] -= 1
            if count[r + 1] > 0:
                break
            r += 1
        perm_count += 1


t = start()
report("fannkuch_redux", t, all([fannkuch(9) == (8629, 30) for _ in range(4)]))
