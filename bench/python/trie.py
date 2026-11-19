# bench/trie.prg in Python: a trie of dicts, one per node, from a letter to the node [children, words]: 100,000 words of
# 3 to 10 letters from an 8-letter alphabet inserted, then 100,000 other words of the same kind looked up, as words and
# as prefixes of the words stored.
from bench import report, start

seed = 2718


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def new_word():
    n, w = rand(8) + 3, ""
    for _ in range(n):
        w += "etaoinsr"[rand(rand(8) + 1)]
    return w


def run(words, probes):
    root, nodes, found, prefix = [{}, 0], 1, 0, 0
    for w in words:
        node = root
        for c in w:
            if c in node[0]:
                node = node[0][c]
            else:
                node[0][c] = node = [{}, 0]
                nodes += 1
        node[1] += 1
    for w in probes:
        node = root
        for c in w:
            if c not in node[0]:
                node = None
                break
            node = node[0][c]
        if node is not None:
            prefix += 1
            if node[1] > 0:
                found += 1
    return nodes == 159557 and found == 40767 and prefix == 50841


words, probes = [None] * 100000, [None] * 100000
for i in range(100000):
    words[i] = new_word()
    probes[i] = new_word()
t = start()
report("trie", t, run(words, probes))
