# bench/parallel_map.prg in Python: a parallel map, 1,000,000 numbers (1 to 1,000,000, from a fixed seed) mapped to
# their Collatz step counts by 64 tasks on a pool of processes (threads would share one interpreter lock), each mapping
# its slice into a list of its own; the slices are joined in order, and the result checked: its sum, its maximum and
# where it is first, a running hash of the whole. Each task gets its slice of the input as an argument (a process
# cannot read the caller's list) and the halving is x // 2 where the .prg's x / 2 gives an integral number.
from concurrent.futures import ProcessPoolExecutor

from bench import report, start


def steps(x):
    s = 0
    while x != 1:
        x = x // 2 if x % 2 == 0 else 3 * x + 1
        s += 1
    return s


def map_slice(a):
    out = [0] * len(a)
    for i in range(len(a)):
        out[i] = steps(a[i])
    return out


if __name__ == "__main__":
    n, x = 1000000, 777
    a_in = [0] * n
    for i in range(n):
        x = x * 16807 % 2147483647
        a_in[i] = x % 1000000 + 1
    t = start()
    chunk = n // 64
    a_out = []
    with ProcessPoolExecutor() as pool:
        for part in pool.map(map_slice, [a_in[i:i + chunk] for i in range(0, n, chunk)]):
            a_out.extend(part)
    total = high = at = chk = 0
    for i in range(n):
        x = a_out[i]
        total += x
        if x > high:
            high = x
            at = i + 1
        chk = (chk * 31 + x) % 1000000007
    report("parallel_map", t, len(a_out) == n and total == 131359344 and high == 524 and at == 237731
           and chk == 68222219)
