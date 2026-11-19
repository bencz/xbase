# bench/base64.prg in Python: Base64 written out, 1,200,000 bytes from a fixed-seed generator encoded (three bytes to
# four characters through a table) and decoded back; 3 rounds. The output strings, built by appending in the .prg, are
# lists of pieces joined (PyPy copies a string on every +=). Named base64_bench: a file named base64 here would be found
# before the standard library's module by the ports that import it (through asyncio or concurrent.futures).
from bench import report, start

B64_TABLE = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
VALUES = [0] * 256
for _i in range(64):
    VALUES[ord(B64_TABLE[_i])] = _i
seed = 64


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def encode(c):
    out, n_len, rest = [], len(c), len(c) % 3
    for i in range(0, n_len - rest, 3):
        n = ord(c[i]) * 65536 + ord(c[i + 1]) * 256 + ord(c[i + 2])
        out.append(B64_TABLE[n // 262144] + B64_TABLE[n // 4096 % 64] + B64_TABLE[n // 64 % 64] + B64_TABLE[n % 64])
    if rest == 1:
        n = ord(c[n_len - 1])
        out.append(B64_TABLE[n // 4] + B64_TABLE[(n % 4) * 16] + "==")
    elif rest == 2:
        n = ord(c[n_len - 2]) * 256 + ord(c[n_len - 1])
        out.append(B64_TABLE[n // 1024] + B64_TABLE[n // 16 % 64] + B64_TABLE[(n % 16) * 4] + "=")
    return "".join(out)


def decode(c):
    out, pad = [], 0
    if c[-2:] == "==":
        pad = 2
    elif c[-1:] == "=":
        pad = 1
    for i in range(0, len(c), 4):
        n = (VALUES[ord(c[i])] * 262144 + VALUES[ord(c[i + 1])] * 4096 + VALUES[ord(c[i + 2])] * 64
             + VALUES[ord(c[i + 3])])
        out.append(chr(n // 65536) + chr(n // 256 % 256) + chr(n % 256))
    s = "".join(out)
    return s[:len(s) - pad]


def run(c):
    n, ok = 0, True
    for r in range(1, 4):
        part = c[:len(c) - r + 1]
        enc = encode(part)
        ok = ok and decode(enc) == part
        n += len(enc)
        for i in range(0, len(enc), 97):
            n += ord(enc[i])
    return ok and n == 9038319 and encode("Ma") == "TWE=" and decode("TQ==") == "M"


if __name__ == "__main__":
    data = "".join([chr(rand(256)) for _ in range(1200000)])
    t = start()
    report("base64", t, run(data))
