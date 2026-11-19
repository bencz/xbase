# bench/inheritance_depth.prg in Python: a line of 8 classes, each inheriting the one before, 4,000,000 rounds, on an
# object of the last (made by name through eval(), as the .prg's macro makes it), of a method each class overrides and
# that calls super()'s down to the first (8 calls), a method only the first class has, and attributes set by the first,
# a middle and the last class (read, and the first one assigned).
from bench import report, start


class TL0:
    def __init__(self):
        self.v0 = 0

    def depth(self):
        return 1

    def base(self):
        return self.v0 + 100


class TL1(TL0):
    def __init__(self):
        super().__init__()
        self.v1 = 1

    def depth(self):
        return super().depth() + 2


class TL2(TL1):
    def __init__(self):
        super().__init__()
        self.v2 = 2

    def depth(self):
        return super().depth() + 3


class TL3(TL2):
    def __init__(self):
        super().__init__()
        self.v3 = 3

    def depth(self):
        return super().depth() + 4


class TL4(TL3):
    def __init__(self):
        super().__init__()
        self.v4 = 4

    def depth(self):
        return super().depth() + 5


class TL5(TL4):
    def __init__(self):
        super().__init__()
        self.v5 = 5

    def depth(self):
        return super().depth() + 6


class TL6(TL5):
    def __init__(self):
        super().__init__()
        self.v6 = 6

    def depth(self):
        return super().depth() + 7


class TL7(TL6):
    def __init__(self):
        super().__init__()
        self.v7 = 7

    def depth(self):
        return super().depth() + 8


def run(o):
    n = 0
    for i in range(1, 4000001):
        n += o.depth() + o.base() + o.v0 + o.v4 + o.v7
        o.v0 = i % 3
    return n


t = start()
report("inheritance_depth", t, run(eval("TL7()")) == 4000000 * (36 + 100 + 11) + 2 * 3999999)
