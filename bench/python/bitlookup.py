# bench/bitlookup.prg in Python: the bitwise AND from a table of the ANDs of two nibbles, IOR and XOR written out as
# the .prg's macros expand them (7 calls of and_() a pair), for every pair a, b in 0 .. 999.
from bench import report, start

TABLE = [a & b for a in range(16) for b in range(16)]


def and_(a, b):
    r = 0
    for _ in range(4):
        r = r // 16 + TABLE[(a % 16) * 16 + (b % 16)] * 4096
        a //= 16
        b //= 16
    return r


def run():
    for a in range(1000):
        for b in range(1000):
            if (and_(a, b) != (a & b)
                    or a + b - 2 * and_(a, b) + and_(a, b) - 2 * and_(a + b - 2 * and_(a, b), and_(a, b)) != (a | b)
                    or a + b - 2 * and_(a, b) != (a ^ b)):
                return False
    return True


t = start()
report("bitlookup", t, run())
