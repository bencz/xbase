# bench/linked_list.prg in Python: a singly linked list of two-element lists [value, next], 1,000,000 nodes pushed at
# the head, walked, reversed in place, walked again, and every third node unlinked; 3 rounds.
from bench import report, start


def run():
    total = count = first = 0
    for _ in range(3):
        head = None
        for i in range(1, 1000001):
            head = [i, head]
        node = head
        while node is not None:
            total += node[0]
            node = node[1]
        prev = None
        node = head
        while node is not None:
            nxt = node[1]
            node[1] = prev
            prev = node
            node = nxt
        head = prev
        first += head[0]
        node = head
        while node is not None and node[1] is not None:
            node[1] = node[1][1]
            node = node[1]
        node = head
        while node is not None:
            count += 1
            node = node[1]
    return total == 1500001500000 and first == 3 and count == 1500000


t = start()
report("linked_list", t, run())
