# bench/lru_cache.prg in Python: a least-recently-used cache of 2,000 entries, a dict from key to node and a doubly
# linked list of nodes [key, value, prev, next] in order of use; 2,000,000 reads of keys skewed toward small numbers, a
# miss computing the value and evicting the oldest entry when the cache is full.
from bench import report, start

N_KEY, N_VALUE, N_PREV, N_NEXT = 0, 1, 2, 3
seed = 1234


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def run():
    h, head, tail = {}, [None, None, None, None], [None, None, None, None]
    hits = total = 0
    cap = 2000
    head[N_NEXT] = tail
    tail[N_PREV] = head
    for _ in range(2000000):
        k = rand(rand(20000) + 1)
        if k in h:
            hits += 1
            node = h[k]
            node[N_PREV][N_NEXT] = node[N_NEXT]
            node[N_NEXT][N_PREV] = node[N_PREV]
        else:
            if len(h) >= cap:
                node = tail[N_PREV]
                node[N_PREV][N_NEXT] = tail
                tail[N_PREV] = node[N_PREV]
                del h[node[N_KEY]]
            node = [k, k * 3 + 1, None, None]
            h[k] = node
        node[N_NEXT] = head[N_NEXT]
        node[N_PREV] = head
        head[N_NEXT][N_PREV] = node
        head[N_NEXT] = node
        total += node[N_VALUE]
    return hits == 378837 and total == 30033837356 and len(h) == cap


t = start()
report("lru_cache", t, run())
