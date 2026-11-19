# bench/jaro_winkler.prg in Python: string similarity, the Jaro-Winkler similarity of 300,000 pairs of 6 to 14 letters
# (the second one the first with random changes), the sum of the floats checked to 6 decimals.
from bench import report, start

seed = 271


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def jaro_winkler(s1, s2):
    l1, l2 = len(s1), len(s2)
    m = trans = prefix = 0
    if l1 == 0 or l2 == 0:
        return 1 if l1 == l2 else 0
    rng = max(max(l1, l2) // 2 - 1, 0)
    a1, a2 = [False] * l1, [False] * l2
    for i in range(l1):
        for j in range(max(0, i - rng), min(l2, i + rng + 1)):
            if not a2[j] and s1[i] == s2[j]:
                a1[i] = True
                a2[j] = True
                m += 1
                break
    if m == 0:
        return 0
    k = 0
    for i in range(l1):
        if a1[i]:
            while not a2[k]:
                k += 1
            if s1[i] != s2[k]:
                trans += 1
            k += 1
    jaro = (m / l1 + m / l2 + (m - trans / 2) / m) / 3
    while prefix < 4 and prefix < min(l1, l2) and s1[prefix] == s2[prefix]:
        prefix += 1
    return jaro + prefix * 0.1 * (1 - jaro)


def pair(n):
    s = ""
    for _ in range(n):
        s += chr(65 + rand(6))
    u = s
    for _ in range(3):
        k = rand(len(u)) + 1
        d = rand(2)
        u = u[:k - 1] + chr(65 + rand(6)) + u[k - 1 + d:]
    return s, u


def run(pairs):
    total = 0
    for p in pairs:
        total += jaro_winkler(p[0], p[1])
    return abs(total - 260943.0356007) < 0.000001 and abs(jaro_winkler("MARTHA", "MARHTA") - 0.961111111) < 0.000000001


pairs = [pair(6 + rand(9)) for _ in range(300000)]
t = start()
report("jaro_winkler", t, run(pairs))
