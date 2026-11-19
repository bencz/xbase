# bench/dijkstra.prg in Python: Dijkstra's shortest paths on a directed graph of 20,000 vertices and 120,000 edges
# (each vertex linked to the next and to 5 others, weights 1 .. 100, from a linear congruential generator) kept as
# lists of adjacency, with a binary heap of (distance, vertex) pairs written out as the .prg's and stale entries
# skipped; from 12 sources, the sum of the distances checked. The lists keep the .prg's 1-based indexes (element 0
# unused).
from bench import report, start


def dijkstra(src, nv, first, to, wt):
    dist, done = [-1] * (nv + 1), [False] * (nv + 1)
    hd, hv = [0] * len(to), [0] * len(to)
    total = 0
    dist[src] = 0
    nh = 1
    hd[1] = 0
    hv[1] = src
    while nh > 0:
        d = hd[1]
        u = hv[1]
        x = hd[nh]
        y = hv[nh]
        nh -= 1
        i = 1
        while (c := i * 2) <= nh:
            if c < nh and hd[c + 1] < hd[c]:
                c += 1
            if hd[c] >= x:
                break
            hd[i] = hd[c]
            hv[i] = hv[c]
            i = c
        hd[i] = x
        hv[i] = y
        if done[u]:
            continue
        done[u] = True
        total += d
        for e in range(first[u], first[u + 1]):
            v = to[e]
            nd = d + wt[e]
            if not done[v] and (dist[v] < 0 or nd < dist[v]):
                dist[v] = nd
                nh += 1
                i = nh
                while i > 1 and hd[c := i // 2] > nd:
                    hd[i] = hd[c]
                    hv[i] = hv[c]
                    i = c
                hd[i] = nd
                hv[i] = v
    return total


def run():
    nv, n_out, seed, e, total = 20000, 6, 7, 0, 0
    ne = nv * n_out
    first, to, wt = [0] * (nv + 2), [0] * (ne + 1), [0] * (ne + 1)
    for i in range(1, nv + 1):
        first[i] = e + 1
        e += 1
        to[e] = 1 if i == nv else i + 1
        seed = seed * 16807 % 2147483647
        wt[e] = seed // 256 % 100 + 1
        for _ in range(2, n_out + 1):
            e += 1
            seed = seed * 16807 % 2147483647
            to[e] = seed // 16 % nv + 1
            seed = seed * 16807 % 2147483647
            wt[e] = seed // 256 % 100 + 1
    first[nv + 1] = e + 1
    for src in range(1, 13):
        total += dijkstra((src - 1) * 1600 + 1, nv, first, to, wt)
    return total


t = start()
report("dijkstra", t, run() == 41762116)
