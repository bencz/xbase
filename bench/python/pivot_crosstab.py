# bench/pivot_crosstab.prg in Python: a pivot (crosstab) of sales, 2,000,000 records [seller, date, amount] made from a
# fixed seed (60 sellers, the days of 2025), then timed: the amounts summed per seller and month (the date's month) in a
# dict of rows, and per weekday (Sunday first, as the .prg's DoW()) and month; row and column totals, the sellers
# sorted, and the table rendered as text (ljust(), "%11d" for the .prg's PadR() and Str()) with a totals line.
import datetime

from bench import report, start

seed = 16180


def rand(n):
    global seed
    seed = seed * 16807 % 2147483647
    return seed % n


def run(sales):
    rows, col, dow = {}, [0] * 12, [[0] * 12 for _ in range(7)]
    tab, sat, grand, best, c_best, c_s07 = "", 0, 0, 0, None, None
    for r in sales:
        a = rows.get(r[0])
        if a is None:
            a = [0] * 12
            rows[r[0]] = a
        month = r[1].month
        a[month - 1] += r[2]
        dow[r[1].isoweekday() % 7][month - 1] += r[2]
    rows = dict(sorted(rows.items()))
    for k, a in rows.items():
        line = k.ljust(10)
        row = 0
        for i in range(12):
            line += "%11d" % a[i]
            row += a[i]
            col[i] += a[i]
        line += "%13d" % row
        if row > best:
            c_best = k
            best = row
        if k == "SELLER007":
            c_s07 = line
        tab += line + "\n"
    line = "TOTAL".ljust(10)
    for i in range(12):
        line += "%11d" % col[i]
        grand += col[i]
    tab += line + "%13d" % grand + "\n"
    for x in dow[6]:
        sat += x
    return (grand == 50028051852 and c_best == "SELLER015" and best == 844241139 and col[0] == 4275964399
            and col[1] == 3837132322 and col[11] == 4246237533 and dow[0][0] == 547728957 and sat == 7111132032
            and len(tab) == 61 * (10 + 12 * 11 + 13 + 1) and c_s07 == "SELLER007 "
            "   71382096   62515370   68284183   68152167   70863968   69201960"
            "   71447433   71672784   67917922   68878748   65718083   68477906    824512620")


base = datetime.date(2025, 1, 1)
records = [None] * 2000000
for _i in range(len(records)):
    records[_i] = ["SELLER" + "%03d" % (rand(60) + 1), base + datetime.timedelta(days=rand(365)), rand(50000) + 1]
t = start()
report("pivot_crosstab", t, run(records))
