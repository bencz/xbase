# bench/date_time_ops.prg in Python: dates and times, 200,000 dates between 1900 and 2099 and times of day from a fixed
# seed. For each: year, month, day against strftime("%Y%m%d") (the .prg's DToS); isoweekday() and strftime("%A") (its
# DoW and CDoW: Sundays, Fridays the 13th); "dd/mm/yyyy" and back with strptime() (its DToC and CToD under SET DATE
# BRITISH and SET CENTURY ON); 13 months added (the day kept within the month, through date() and date arithmetic,
# where the .prg reads SToD() strings); the days to the year's end; a time of day through "hh:mm:ss" and back (Python
# has no TString() or Secs(): written out).
import datetime

from bench import report, start

seed = 1999


def rand(n):
    global seed
    seed = seed * 16807 % 2147483647
    return seed % n


def add_months(y, m, day, n):
    nm = m + n
    ny = y + (nm - 1) // 12
    nm = (nm - 1) % 12 + 1
    first = datetime.date(ny, nm, 1)
    nxt = first + datetime.timedelta(days=31)
    return first + datetime.timedelta(days=min(day, (nxt - datetime.timedelta(days=nxt.day)).day) - 1)


def tstring(s):
    return "%02d:%02d:%02d" % (s // 3600, s // 60 % 60, s % 60)


def secs(c):
    return int(c[0:2]) * 3600 + int(c[3:5]) * 60 + int(c[6:8])


def run():
    base = datetime.date(1900, 1, 1)
    ymd = sun = fri13 = added = to_end = sec = bad = 0
    for _ in range(200000):
        d = base + datetime.timedelta(days=rand(73000))
        y, m, day = d.year, d.month, d.day
        n = y * 10000 + m * 100 + day
        ymd += n
        if int(d.strftime("%Y%m%d")) != n or \
                datetime.datetime.strptime(d.strftime("%d/%m/%Y"), "%d/%m/%Y").date() != d:
            bad += 1
        if d.isoweekday() == 7:
            sun += 1
        if day == 13 and d.strftime("%A") == "Friday":
            fri13 += 1
        added += int(add_months(y, m, day, 13).strftime("%Y%m%d"))
        to_end += (datetime.date(y, 12, 31) - d).days
        s = rand(86400)
        if secs(tstring(s)) != s:
            bad += 1
        sec += s
    return (bad == 0 and ymd == 3998853334023 and sun == 28567 and fri13 == 951 and added == 4001021793716
            and to_end == 36531947 and sec == 8632784536)


t = start()
report("date_time_ops", t, run())
