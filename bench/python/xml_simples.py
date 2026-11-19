# bench/xml_simples.prg in Python: simple XML, a catalog of 50,000 books [title, author, price, year] (titles and
# authors with & < > and ' in them) made from a fixed seed, then timed: written as one document (the text escaped; a
# list of pieces joined, where the .prg appends to a string: PyPy copies a string on every +=), split into elements
# with split(), and read back (each child's text found with find() between its tags, the id attribute, the entities
# decoded with replace(), the price with float()). The values read back add up as made.
from bench import report, start

seed = 5150
WORDS = ["Rivers", "Salt & Pepper", "<Night>", "Stone", "Tales", "Tom & Jerry", "Glass", "Winter", "Red", "Old"]
AUTHORS = ["Ana Lima", "Bruno Costa", "Carla Dias", "Diego Reis", "Eva O'Neil"]


def rand(n):
    global seed
    seed = seed * 16807 % 2147483647
    return seed % n


def inner(c, tag):
    begin = c.find("<" + tag + ">") + len(tag) + 2
    return c[begin:c.find("</" + tag + ">")]


def escape(c):
    return c.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("'", "&apos;")


def unescape(c):
    return c.replace("&apos;", "'").replace("&gt;", ">").replace("&lt;", "<").replace("&amp;", "&")


def run(books):
    cents = amp = years = eva = ids = 0
    c12345 = None
    out = ['<?xml version="1.0"?>\n<catalog>\n']
    for i in range(1, len(books) + 1):
        b = books[i - 1]
        out.append('<book id="' + str(i) + '"><title>' + escape(b[0]) + "</title><author>" + escape(b[1])
                   + "</author><price>" + "%.2f" % (b[2] / 100) + "</price><year>" + "%4d" % b[3] + "</year></book>\n")
    out.append("</catalog>\n")
    c = "".join(out)
    ok = len(c) == 6276273
    parts = c.split("<book ")
    for i in range(1, len(parts)):
        book = parts[i]
        ids += int(book[4:book.find('"', 4)])
        title = unescape(inner(book, "title"))
        if "&" in title:
            amp += 1
        if i == 12345:
            c12345 = title
        if unescape(inner(book, "author")) == "Eva O'Neil":
            eva += 1
        cents += round(float(inner(book, "price")) * 100)
        years += int(inner(book, "year"))
    return (ok and len(parts) == 50001 and ids == 50000 * 50001 // 2 and cents == 255248286 and amp == 21225
            and years == 99351507 and eva == 10152 and c12345 == "Winter Glass Tom & Jerry")


def make():
    books = [None] * 50000
    for i in range(len(books)):
        n = rand(2) + 2
        title = ""
        for k in range(1, n + 1):
            title += (" " if k > 1 else "") + WORDS[rand(10)]
        books[i] = [title, AUTHORS[rand(5)], rand(10000) + 100, 1950 + rand(75)]
    return books


catalog = make()
t = start()
report("xml_simples", t, run(catalog))
