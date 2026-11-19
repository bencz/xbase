# bench/random_massivo.prg in Python: massive random-number generation, 30,000,000 rounds of four generators in integer
# arithmetic (a 31-bit linear congruential generator, a 64-bit xorshift, Marsaglia's two 16-bit multiply-with-carry
# streams and a 32-bit xorshift), their sums, a histogram of 256 buckets of their top bytes and the sum of the
# congruential numbers scaled to [0, 1). The 64-bit xorshift is kept as an unsigned 64-bit value: its left shifts are
# masked to 64 bits where the .prg's wrap.
from bench import report, start

M64 = 0xFFFFFFFFFFFFFFFF


def run():
    hist = [0] * 256
    lcg, xs64, z, w, xs32 = 12345, 88172645463325252, 362436069, 521288629, 2463534242
    s1 = s2 = s3 = s4 = check = 0
    fsum = 0.0
    for _ in range(30000000):
        lcg = (lcg * 1103515245 + 12345) & 0x7FFFFFFF
        s1 += lcg
        hist[lcg >> 23] += 1
        fsum += lcg / 2147483648.0

        xs64 ^= (xs64 << 13) & M64
        xs64 ^= (xs64 >> 7) & 0x01FFFFFFFFFFFFFF
        xs64 ^= (xs64 << 17) & M64
        r = (xs64 >> 32) & 0xFFFFFFFF
        s2 += r
        hist[r >> 24] += 1

        z = 36969 * (z & 0xFFFF) + (z >> 16)
        w = 18000 * (w & 0xFFFF) + (w >> 16)
        r = ((z << 16) + w) & 0xFFFFFFFF
        s3 += r
        hist[r >> 24] += 1

        xs32 ^= (xs32 << 13) & 0xFFFFFFFF
        xs32 ^= xs32 >> 17
        xs32 ^= (xs32 << 5) & 0xFFFFFFFF
        s4 += xs32
        hist[xs32 >> 24] += 1
    for i in range(256):
        check += hist[i] * (i + 1)
    return (s1 == 32217797789519680 and s2 == 64428429046975605 and s3 == 64423232821249815
            and s4 == 64428119329011698 and check == 15421035017 and abs(fsum - 15002581.187856009) < 0.000001)


t = start()
report("random_massivo", t, run())
