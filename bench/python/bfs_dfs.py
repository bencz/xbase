# bench/bfs_dfs.prg in Python: breadth-first and depth-first search on an undirected graph of 50,000 vertices and
# 150,000 random edges (from a linear congruential generator) kept as lists of adjacency: BFS from 6 sources (the sum
# of the levels and of the vertices reached), then an iterative DFS with an explicit stack over every component (the
# number of components and a checksum of the visiting order); 2 times. The lists keep the .prg's 1-based indexes
# (element 0 unused).
from bench import report, start


def bfs(src, nv, first, adj):
    level, queue = [-1] * (nv + 1), [0] * (nv + 1)
    head = tail = 1
    total = 0
    level[src] = 0
    queue[1] = src
    while head <= tail:
        u = queue[head]
        head += 1
        d = level[u]
        total += d + 1
        for e in range(first[u], first[u + 1]):
            v = adj[e]
            if level[v] < 0:
                level[v] = d + 1
                tail += 1
                queue[tail] = v
    return total


def dfs(nv, first, adj):
    seen, stack = [False] * (nv + 1), [0] * (2 * (len(adj) - 1) + nv + 1)
    order = comps = total = 0
    for s in range(1, nv + 1):
        if seen[s]:
            continue
        comps += 1
        top = 1
        stack[1] = s
        while top > 0:
            u = stack[top]
            top -= 1
            if seen[u]:
                continue
            seen[u] = True
            order += 1
            total += u * order
            for e in range(first[u + 1] - 1, first[u] - 1, -1):
                if not seen[adj[e]]:
                    top += 1
                    stack[top] = adj[e]
    return comps * 1000000000 + total


def run():
    nv, ne, seed, total = 50000, 150000, 3, 0
    eu, ev, first = [0] * (ne + 1), [0] * (ne + 1), [0] * (nv + 2)
    for i in range(1, ne + 1):
        seed = seed * 16807 % 2147483647
        eu[i] = seed // 16 % nv + 1
        seed = seed * 16807 % 2147483647
        ev[i] = seed // 16 % nv + 1
        first[eu[i]] += 1
        first[ev[i]] += 1
    fill = 1
    for i in range(1, nv + 2):
        d = first[i]
        first[i] = fill
        fill += d
    fill = first[:]
    adj = [0] * (2 * ne + 1)
    for i in range(1, ne + 1):
        adj[fill[eu[i]]] = ev[i]
        fill[eu[i]] += 1
        adj[fill[ev[i]]] = eu[i]
        fill[ev[i]] += 1
    for _ in range(2):
        for src in range(1, 7):
            total += bfs((src - 1) * 8000 + 1, nv, first, adj)
        total += dfs(nv, first, adj)
    return total


t = start()
report("bfs_dfs", t, run() == 62828511998050)
