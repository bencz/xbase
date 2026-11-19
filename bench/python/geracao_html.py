# bench/geracao_html.prg in Python: HTML generation, a table of 100,000 items [name of two words, quantity, price in
# cents] made from a fixed seed, then timed: one page built by appending to a string buffer (io.StringIO: PyPy copies a
# string on every +=), each row with its id, a zebra class, the name escaped (replace() of & < > "), and the price and
# total formatted with "%.2f" (the .prg's Str()). The page's length, a row and its end are checked.
import io

from bench import report, start

seed = 4711
WORDS = ["Alpha", "Beta & Sons", "<Gamma>", "Delta", 'Echo "Q"', "Foxtrot", "Golf", "H&M"]


def rand(n):
    global seed
    seed = seed * 16807 % 2147483647
    return seed % n


def escape(c):
    return c.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")


def run(items):
    buf = io.StringIO()
    buf.write("<html><head><title>Items</title></head><body>\n<table>\n"
              "<tr><th>Id</th><th>Name</th><th>Qty</th><th>Price</th><th>Total</th></tr>\n")
    pos77 = 0
    for i in range(1, len(items) + 1):
        r = items[i - 1]
        c_id = str(i)
        if i == 77777:
            pos77 = buf.tell() + 1
        buf.write('<tr id="r' + c_id + '" class="' + ("odd" if i % 2 == 1 else "even") + '"><td>' + c_id + "</td><td>"
                  + escape(r[0]) + '</td><td class="n">' + "%d" % r[1] + '</td><td class="n">' + "%.2f" % (r[2] / 100)
                  + '</td><td class="n">' + "%.2f" % (r[1] * r[2] / 100) + "</td></tr>\n")
    buf.write("</table>\n</body></html>\n")
    c = buf.getvalue()
    row77 = ('<tr id="r77777" class="odd"><td>77777</td><td>&lt;Gamma&gt; Delta</td><td class="n">56</td>'
             '<td class="n">3992.87</td><td class="n">223600.72</td></tr>\n')
    tail = ('<tr id="r100000" class="even"><td>100000</td><td>Foxtrot Alpha</td><td class="n">57</td>'
            '<td class="n">4506.67</td><td class="n">256880.19</td></tr>\n</table>\n</body></html>\n')
    return (len(c) == 15121698 and pos77 == 11756338 and c[pos77 - 1:pos77 - 1 + len(row77)] == row77
            and c[-len(tail):] == tail)


items = [[WORDS[rand(8)] + " " + WORDS[rand(8)], rand(100) + 1, rand(1000000) + 1] for _ in range(100000)]
t = start()
report("geracao_html", t, run(items))
