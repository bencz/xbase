# bench/object_churn.prg in Python: objects replaced in a graph, 50,000 slots holding objects of three classes that
# point to other slots' objects (cycles included); 700,000 times a slot's object is replaced by a new one, then the
# graph is walked. The .prg's Str( i % 100 ) has 13 characters (its % gives SET DECIMALS decimals): "%13.2f".
from bench import report, start

seed = 9


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


class TLeaf:
    def __init__(self, n):
        self.value = n

    def weight(self):
        return self.value


class TPair(TLeaf):
    def __init__(self, n, left, right):
        super().__init__(n)
        self.left = left
        self.right = right

    def weight(self):
        return self.value + self.left.value + self.right.value


class TList(TLeaf):
    def __init__(self, n, o):
        super().__init__(n)
        self.items = [o, n, "%13.2f" % n]

    def weight(self):
        return self.value + self.items[0].value + len(self.items[2])


def run():
    slots = [TLeaf(i) for i in range(1, 50001)]
    for i in range(1, 700001):
        k = rand(len(slots))
        kind = rand(3)
        if kind == 0:
            o = TLeaf(i % 100)
        elif kind == 1:
            o = TPair(i % 100, slots[rand(len(slots))], slots[rand(len(slots))])
        else:
            o = TList(i % 100, slots[rand(len(slots))])
        slots[k] = o
    return sum(o.weight() for o in slots)


t = start()
report("object_churn", t, run() == 5162688)
