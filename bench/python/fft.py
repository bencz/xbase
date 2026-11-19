# bench/fft.prg in Python: the fast Fourier transform (iterative radix 2, in place, on lists of real and imaginary
# parts), 65,536 complex points from a linear congruential generator, transformed forward and back 10 times. The
# twiddle factors come from cos and sin of 2 pi / 2^k and complex multiplication; the check is the spectrum's sum of
# magnitudes and the round trip's largest error.
import math

from bench import report, start


def fft(re, im, n, cos_t, sin_t, sign):
    j = 0
    for i in range(n - 1):
        if i < j:
            re[i], re[j] = re[j], re[i]
            im[i], im[j] = im[j], im[i]
        k = n >> 1
        while k <= j:
            j -= k
            k >>= 1
        j += k
    length, st = 2, 0
    while length <= n:
        half = length >> 1
        wr0 = cos_t[st]
        wi0 = sign * sin_t[st]
        for i in range(0, n, length):
            wr, wi = 1.0, 0.0
            for j in range(i, i + half):
                k = j + half
                xr = re[k] * wr - im[k] * wi
                xi = re[k] * wi + im[k] * wr
                re[k] = re[j] - xr
                im[k] = im[j] - xi
                re[j] += xr
                im[j] += xi
                tmp = wr * wr0 - wi * wi0
                wi = wr * wi0 + wi * wr0
                wr = tmp
        length <<= 1
        st += 1


def run():
    log_n, seed, spec, max_err = 16, 2024, 0.0, 0.0
    n = 1 << log_n
    re, im = [0.0] * n, [0.0] * n
    for i in range(n):
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        re[i] = seed / 1073741824.0 - 1.0
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        im[i] = seed / 1073741824.0 - 1.0
    re0, im0 = re[:], im[:]
    # cos and sin of 2 pi / 2^k, for k from 1
    cos_t, sin_t = [0.0] * log_n, [0.0] * log_n
    for i in range(log_n):
        cos_t[i] = math.cos(2.0 * math.pi / (1 << (i + 1)))
        sin_t[i] = math.sin(2.0 * math.pi / (1 << (i + 1)))
    for rep in range(1, 11):
        fft(re, im, n, cos_t, sin_t, -1.0)
        if rep == 1:
            for i in range(n):
                spec += abs(re[i]) + abs(im[i])
        fft(re, im, n, cos_t, sin_t, 1.0)
        for i in range(n):
            re[i] = re[i] / n
            im[i] = im[i] / n
    for i in range(n):
        max_err = max(max_err, max(abs(re[i] - re0[i]), abs(im[i] - im0[i])))
    return abs(spec / 15471706.35354992 - 1) < 0.000000001 and max_err < 0.000000001


t = start()
report("fft", t, run())
