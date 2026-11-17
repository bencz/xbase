# bench/crc32.prg in Python: CRC-32 of 20,000 bytes (the table made each time, the data built in chunks), 100 times.
from bench import report, start


def crc32_of_20k():
    table = [0] * 256
    for i in range(256):
        c = i
        for _ in range(8):
            c = (c >> 1) ^ (0xEDB88320 & -(c & 1))
        table[i] = c
    data = ""
    for i in range(0, 20000, 100):
        chunk = ""
        for k in range(i, i + 100):
            chunk += chr((k * 31 + 7) % 251)
        data += chunk
    crc = 0xFFFFFFFF
    for ch in data:
        crc = table[(crc ^ ord(ch)) & 0xFF] ^ (crc >> 8)
    return crc ^ 0xFFFFFFFF


t = start()
report("crc32", t, all([crc32_of_20k() == 2954121016 for _ in range(100)]))
