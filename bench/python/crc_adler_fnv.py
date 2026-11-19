# bench/crc_adler_fnv.prg in Python: checksums of bytes, twenty buffers of 1,000,000 bytes from an LCG, built a
# character at a time (a list of characters joined: PyPy copies a string on every +=), each hashed with CRC-32 (from a
# table), Adler-32 and FNV-1a (32 bits); the functions are first checked against their published values ("123456789",
# "Wikipedia", "a").
from bench import report, start


def crc32_of(table, s):
    crc = 0xFFFFFFFF
    for ch in s:
        crc = table[(crc ^ ord(ch)) & 0xFF] ^ (crc >> 8)
    return crc ^ 0xFFFFFFFF


def adler32_of(s):
    a, b = 1, 0
    for ch in s:
        a += ord(ch)
        if a >= 65521:
            a -= 65521
        b += a
        if b >= 65521:
            b -= 65521
    return (b << 16) | a


def fnv1a_of(s):
    h = 2166136261
    for ch in s:
        h = ((h ^ ord(ch)) * 16777619) & 0xFFFFFFFF
    return h


def run():
    n, seed, total, table = 1000000, 42, 0, [0] * 256
    for i in range(256):
        c = i
        for _ in range(8):
            c = (c >> 1) ^ (0xEDB88320 & -(c & 1))
        table[i] = c
    ok = crc32_of(table, "123456789") == 0xCBF43926 and adler32_of("Wikipedia") == 0x11E60398 and \
        fnv1a_of("a") == 0xE40C292C
    for _ in range(20):
        parts = []
        for _ in range(n):
            seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
            parts.append(chr((seed >> 16) & 0xFF))
        data = "".join(parts)
        total += crc32_of(table, data) + adler32_of(data) + fnv1a_of(data)
    return ok and total == 142519858956


t = start()
report("crc_adler_fnv", t, run())
