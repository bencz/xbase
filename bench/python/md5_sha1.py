# bench/md5_sha1.prg in Python: MD5 and SHA-1 written out, 32-bit words in integers, masked after each sum, rotations
# from the shift operators, bytes read with ord() from a string. A 200,000-byte buffer from an LCG (a list of
# characters joined: PyPy copies a string on every +=) is hashed 40 times with each, padded as the standards say; both
# are first checked against the published digests of "" and "abc", and the buffer's against md5sum/sha1sum.
from bench import report, start

MASK = 0xFFFFFFFF
MD5_K = [
    0xD76AA478, 0xE8C7B756, 0x242070DB, 0xC1BDCEEE, 0xF57C0FAF, 0x4787C62A, 0xA8304613, 0xFD469501,
    0x698098D8, 0x8B44F7AF, 0xFFFF5BB1, 0x895CD7BE, 0x6B901122, 0xFD987193, 0xA679438E, 0x49B40821,
    0xF61E2562, 0xC040B340, 0x265E5A51, 0xE9B6C7AA, 0xD62F105D, 0x02441453, 0xD8A1E681, 0xE7D3FBC8,
    0x21E1CDE6, 0xC33707D6, 0xF4D50D87, 0x455A14ED, 0xA9E3E905, 0xFCEFA3F8, 0x676F02D9, 0x8D2A4C8A,
    0xFFFA3942, 0x8771F681, 0x6D9D6122, 0xFDE5380C, 0xA4BEEA44, 0x4BDECFA9, 0xF6BB4B60, 0xBEBFBC70,
    0x289B7EC6, 0xEAA127FA, 0xD4EF3085, 0x04881D05, 0xD9D4D039, 0xE6DB99E5, 0x1FA27CF8, 0xC4AC5665,
    0xF4292244, 0x432AFF97, 0xAB9423A7, 0xFC93A039, 0x655B59C3, 0x8F0CCC92, 0xFFEFF47D, 0x85845DD1,
    0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1, 0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391]
MD5_R = [7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21]


def rotl(x, s):
    return ((x << s) | (x >> (32 - s))) & MASK


def pad(s, little):
    n = len(s)
    n_len = (n + 8) // 64 * 64 + 64
    bits = ""
    for i in range(8):
        b = chr(((n * 8) >> (8 * i)) & 0xFF)
        bits = bits + b if little else b + bits
    return s + chr(0x80) + chr(0) * (n_len - n - 9) + bits


def hex_of(words, little):
    out = ""
    for w in words:
        for b in range(4):
            byte = (w >> (8 * b if little else 24 - 8 * b)) & 0xFF
            out += "0123456789abcdef"[byte >> 4]
            out += "0123456789abcdef"[byte & 15]
    return out


def md5_of(s):
    p, h, m = pad(s, True), [0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476], [0] * 16
    for blk in range(0, len(p), 64):
        for j in range(16):
            q = blk + 4 * j
            m[j] = ord(p[q]) | (ord(p[q + 1]) << 8) | (ord(p[q + 2]) << 16) | (ord(p[q + 3]) << 24)
        a, b, c, d = h
        for i in range(64):
            if i < 16:
                f = (b & c) | ((b ^ MASK) & d)
                g = i
            elif i < 32:
                f = (d & b) | ((d ^ MASK) & c)
                g = (5 * i + 1) & 15
            elif i < 48:
                f = b ^ c ^ d
                g = (3 * i + 5) & 15
            else:
                f = c ^ (b | (d ^ MASK))
                g = (7 * i) & 15
            tmp = d
            d = c
            c = b
            b = (b + rotl((a + f + MD5_K[i] + m[g]) & MASK, MD5_R[(i >> 4) * 4 + (i & 3)])) & MASK
            a = tmp
        h[0] = (h[0] + a) & MASK
        h[1] = (h[1] + b) & MASK
        h[2] = (h[2] + c) & MASK
        h[3] = (h[3] + d) & MASK
    return hex_of(h, True)


def sha1_of(s):
    p, h, w = pad(s, False), [0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0], [0] * 80
    for blk in range(0, len(p), 64):
        for j in range(16):
            q = blk + 4 * j
            w[j] = (ord(p[q]) << 24) | (ord(p[q + 1]) << 16) | (ord(p[q + 2]) << 8) | ord(p[q + 3])
        for j in range(16, 80):
            w[j] = rotl(w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16], 1)
        a, b, c, d, e = h
        for i in range(80):
            if i < 20:
                f = (b & c) | ((b ^ MASK) & d)
                k = 0x5A827999
            elif i < 40:
                f = b ^ c ^ d
                k = 0x6ED9EBA1
            elif i < 60:
                f = (b & c) | (b & d) | (c & d)
                k = 0x8F1BBCDC
            else:
                f = b ^ c ^ d
                k = 0xCA62C1D6
            tmp = (rotl(a, 5) + f + e + k + w[i]) & MASK
            e = d
            d = c
            c = rotl(b, 30)
            b = a
            a = tmp
        h[0] = (h[0] + a) & MASK
        h[1] = (h[1] + b) & MASK
        h[2] = (h[2] + c) & MASK
        h[3] = (h[3] + d) & MASK
        h[4] = (h[4] + e) & MASK
    return hex_of(h, False)


def run():
    seed, parts = 7, []
    ok = (md5_of("") == "d41d8cd98f00b204e9800998ecf8427e" and md5_of("abc") == "900150983cd24fb0d6963f7d28e17f72"
          and sha1_of("") == "da39a3ee5e6b4b0d3255bfef95601890afd80709"
          and sha1_of("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d")
    for _ in range(200000):
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        parts.append(chr((seed >> 16) & 0xFF))
    data = "".join(parts)
    for _ in range(40):
        ok = (ok and md5_of(data) == "9caadd7dbbe1174da261d335b92f85fb"
              and sha1_of(data) == "cec3d79455d923d234a1a40741be135a4ba980e2")
    return ok


t = start()
report("md5_sha1", t, run())
