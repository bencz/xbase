# bench/join_arrays.prg in Python: joins of two lists of records, 20,000 customers [id, name, segment, city] and
# 500,000 orders [id, customer, amount] made from a fixed seed (a third of the orders name a customer that exists).
# Timed: a hash join (the customers in a dict by id, each order looked up; amounts summed per segment, orders counted
# per city in a dict), then a sort-merge join (the orders' customer and position packed in one integer key, sorted with
# the list's sort, merged with the customers in id order; the amounts weighted by segment).
from bench import report, start

seed = 2718


def rand(n):
    global seed
    seed = seed * 16807 % 2147483647
    return seed % n


def run(cust, orders):
    h_cust, seg, h_city = {}, [0, 0, 0, 0, 0], {}
    matched = weighted = merged = best = 0
    c_best = None
    for c in cust:
        h_cust[c[0]] = c
    for o in orders:
        if o[1] in h_cust:
            c = h_cust[o[1]]
            seg[c[2] - 1] += o[2]
            h_city[c[3]] = h_city.get(c[3], 0) + 1
            matched += 1
    for k, n in h_city.items():
        if n > best or (n == best and k < c_best):
            c_best = k
            best = n
    keys = [None] * len(orders)
    for i in range(len(orders)):
        keys[i] = orders[i][1] * 1000000 + i + 1
    keys.sort()
    pos = 0
    for k in keys:
        o = orders[k % 1000000 - 1]
        while pos < len(cust) and cust[pos][0] < o[1]:
            pos += 1
        if pos < len(cust) and cust[pos][0] == o[1]:
            weighted += o[2] * cust[pos][2]
            merged += 1
    return (matched == 166641 and merged == 166641 and weighted == 25122891088 and seg[0] == 1626943956
            and seg[1] == 1648544230 and seg[2] == 1673211091 and seg[3] == 1694374411 and seg[4] == 1680345551
            and c_best == "CITY080" and best == 1980 and len(h_city) == 100)


customers = [[i * 3, "Customer " + str(i), rand(5) + 1, "CITY" + "%03d" % (rand(100) + 1)] for i in range(1, 20001)]
order_list = [[i, rand(60000) + 1, rand(100000) + 1] for i in range(1, 500001)]
t = start()
report("join_arrays", t, run(customers, order_list))
