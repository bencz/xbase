# bench/sequential_scan.prg in Python: sequential scans of a table, 200,000 sales records (region, amount, quantity,
# customer, date) made from a fixed seed, then timed, 4 rounds of six passes over every record: a loop with a skip to
# the next record, a sum, a count with a condition, a walk with a function and a condition (the .prg's dbEval), a
# search with its continuations, and a sum over a date range. Python has no DBF tables: the program writes the records
# as the DBF's are laid out (a deletion flag, then the fields as fixed-width text: numbers right-aligned, the date as
# YYYYMMDD), without the DBF's header, to "bench_seqscan.dat", which every pass reads again record by record through a
# small table class (go_top, skip, eof, recno and the fields decoded); the file is deleted at the end.
import datetime
import os

from bench import report, start

seed = 31337
PATH = "bench_seqscan.dat"
REC = 1 + 1 + 10 + 3 + 10 + 8


def rand(n):
    global seed
    seed = seed * 16807 % 2147483647
    return seed % n


class Table:
    def __init__(self, path):
        self.f = open(path, "rb")
        self.rec = b""
        self.recno = 0
        self.eof = True

    def close(self):
        self.f.close()

    def go_top(self):
        self.f.seek(0)
        self.recno = 0
        self.skip()

    def skip(self):
        self.rec = self.f.read(REC)
        self.recno += 1
        self.eof = len(self.rec) < REC

    def region(self):
        return int(self.rec[1:2])

    def amount(self):
        return int(self.rec[2:12])

    def qty(self):
        return int(self.rec[12:15])

    def cust(self):
        return self.rec[15:25].decode("latin-1")

    def saledate(self):
        r = self.rec
        return datetime.date(int(r[25:29]), int(r[29:31]), int(r[31:33]))


def create(path):
    base = datetime.date(2026, 1, 1)
    with open(path, "wb") as f:
        for _ in range(200000):
            region = rand(9) + 1
            amount = rand(1000000)
            qty = rand(50) + 1
            date = base + datetime.timedelta(days=rand(365))
            cust = "CUST" + "%06d" % rand(50000)
            f.write((" %1d%10d%3d%-10s%s" % (region, amount, qty, cust, date.strftime("%Y%m%d"))).encode("latin-1"))


def scans(tb):
    sum_r3 = cnt_r3 = total = big = qty = loc = loc_ids = jan = 0
    tb.go_top()
    while not tb.eof:
        if tb.region() == 3:
            sum_r3 += tb.amount()
            cnt_r3 += 1
        tb.skip()
    tb.go_top()
    while not tb.eof:
        total += tb.amount()
        tb.skip()
    tb.go_top()
    while not tb.eof:
        if tb.amount() > 900000:
            big += 1
        tb.skip()
    tb.go_top()
    while not tb.eof:
        if tb.region() <= 2:
            qty += tb.qty()
        tb.skip()
    tb.go_top()
    while not tb.eof:
        if tb.cust()[:8] == "CUST0001":
            loc += 1
            loc_ids += tb.recno
        tb.skip()
    limit = datetime.date(2026, 2, 1)
    tb.go_top()
    while not tb.eof:
        if tb.saledate() < limit:
            jan += tb.amount()
        tb.skip()
    return (sum_r3 == 11099768125 and cnt_r3 == 22094 and total == 99898922935 and big == 19973 and qty == 1135702
            and loc == 396 and loc_ids == 40072280 and jan == 8435099403)


create(PATH)
try:
    table = Table(PATH)
    t = start()
    ok = all([scans(table) for _ in range(4)])
    report("sequential_scan", t, ok)
    table.close()
finally:
    os.remove(PATH)
