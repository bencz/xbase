# bench/pidigits.prg in Python: arbitrary precision in lists, the first 1,500 digits of pi from Gibbons' unbounded
# spigot, its numbers kept as lists of 30-bit limbs (the least significant first; the accumulator's sign apart) with
# the .prg's operations, not Python's own big integers: multiplication by a small number, addition and subtraction of
# a multiple, comparison, and a quotient below 2^30 estimated from the leading limbs with floats, then corrected. The
# sign the .prg's AddSigned() updates by reference is returned instead. Checked: the first and the last 50 digits and
# their sum.
from bench import report, start

BITS = 30
LIMB_MASK = 0x3FFFFFFF
RADIX = 1073741824.0


def normalize(a):
    n = len(a)
    while n > 1 and a[n - 1] == 0:
        n -= 1
    if n < len(a):
        del a[n:]


def mul_small(a, m):
    carry = 0
    for i in range(len(a)):
        x = a[i] * m + carry
        a[i] = x & LIMB_MASK
        carry = x >> BITS
    while carry > 0:
        a.append(carry & LIMB_MASK)
        carry >>= BITS
    normalize(a)


def add_mul(a, b, m):
    carry, nb = 0, len(b)
    if len(a) < nb:
        a.extend([0] * (nb - len(a)))
    for i in range(nb):
        x = a[i] + b[i] * m + carry
        a[i] = x & LIMB_MASK
        carry = x >> BITS
    i = nb
    while carry > 0 and i < len(a):
        x = a[i] + carry
        a[i] = x & LIMB_MASK
        carry = x >> BITS
        i += 1
    while carry > 0:
        a.append(carry & LIMB_MASK)
        carry >>= BITS


def sub_mul(a, b, m):
    borrow = 0
    for i in range(len(b)):
        x = a[i] - b[i] * m - borrow
        a[i] = x & LIMB_MASK
        borrow = -(x >> BITS)
    i = len(b)
    while borrow > 0:
        x = a[i] - borrow
        a[i] = x & LIMB_MASK
        borrow = -(x >> BITS)
        i += 1
    normalize(a)


def add_signed(a, neg, b, m, term_neg):
    if neg == term_neg:
        add_mul(a, b, m)
    else:
        tmp = b[:]
        mul_small(tmp, m)
        if cmp(a, tmp) >= 0:
            sub_mul(a, tmp, 1)
        else:
            sub_mul(tmp, a, 1)
            a[:] = tmp
            neg = term_neg
    if len(a) == 1 and a[0] == 0:
        neg = False
    return neg


def cmp(a, b):
    if len(a) != len(b):
        return -1 if len(a) < len(b) else 1
    for i in range(len(a) - 1, -1, -1):
        if a[i] != b[i]:
            return -1 if a[i] < b[i] else 1
    return 0


def top(a, i):
    return a[i - 1] * 1.0 if 1 <= i <= len(a) else 0.0


def quotient(x, d):
    h = max(len(x), len(d))
    fx = (top(x, h) * RADIX + top(x, h - 1)) * RADIX + top(x, h - 2)
    fd = (top(d, h) * RADIX + top(d, h - 1)) * RADIX + top(d, h - 2)
    q = int(fx / fd)
    tmp = d[:]
    mul_small(tmp, q)
    while cmp(tmp, x) > 0:
        sub_mul(tmp, d, 1)
        q -= 1
    add_mul(tmp, d, 1)
    while cmp(tmp, x) <= 0:
        add_mul(tmp, d, 1)
        q += 1
    return q


def run():
    n, digits, neg, k = 1500, [], False, 0
    numer, accum, denom = [1], [0], [1]
    while len(digits) < n:
        k += 1
        neg = add_signed(accum, neg, numer, 2, False)
        mul_small(accum, 2 * k + 1)
        mul_small(denom, 2 * k + 1)
        mul_small(numer, k)
        if neg or cmp(numer, accum) > 0:
            continue
        x = accum[:]
        x_neg = add_signed(x, neg, numer, 3, False)
        d = quotient(x, denom)
        add_signed(x, x_neg, numer, 1, False)
        if d != quotient(x, denom):
            continue
        digits.append(chr(48 + d))
        neg = add_signed(accum, neg, denom, d, True)
        mul_small(accum, 10)
        mul_small(numer, 10)
    s = "".join(digits)
    total = sum(ord(c) - 48 for c in s)
    return (s[:50] == "31415926535897932384626433832795028841971693993751"
            and s[-50:] == "55570674983850549458858692699569092721079750930295" and total == 6775)


t = start()
report("pidigits", t, run())
