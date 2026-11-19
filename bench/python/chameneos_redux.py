# bench/chameneos_redux.prg in Python: threads and queues, creatures on threads of their own ask for meetings on one
# queue; a broker pairs them two by two and answers each on its own queue with the other's name and color, after which
# both change color; 30,000 meetings with 3 creatures, then 30,000 with 10. Checked: the color table, and per run 2N
# meetings counted, none of a creature with itself. A thread's result (the .prg's Join() value) is kept in a list.
import queue
import threading

from bench import report, start

BLUE, RED, YELLOW = 0, 1, 2


def complement(a, b):
    return a if a == b else 3 - a - b


def live(meet, n_id, color, results):
    reply, met, same = queue.Queue(), 0, 0
    while True:
        meet.put((n_id, color, reply))
        other = reply.get()
        if other is None:
            break
        color = complement(color, other[1])
        met += 1
        if other[0] == n_id:
            same += 1
    results[n_id - 1] = (met, same)


def run(n, colors):
    meet, results = queue.Queue(), [None] * len(colors)
    left = n
    done = 0
    first = None
    threads = [threading.Thread(target=live, args=(meet, i + 1, c, results)) for i, c in enumerate(colors)]
    for th in threads:
        th.start()
    while done < len(colors):
        req = meet.get()
        if left == 0:
            req[2].put(None)
            done += 1
        elif first is None:
            first = req
        else:
            first[2].put((req[0], req[1]))
            req[2].put((first[0], first[1]))
            first = None
            left -= 1
    for th in threads:
        th.join()
    return sum(r[0] for r in results) == 2 * n and sum(r[1] for r in results) == 0


t = start()
table = "".join(str(complement(a, b)) for a in range(3) for b in range(3))
ok = table == "021210102"
ok = run(30000, [BLUE, RED, YELLOW]) and ok
ok = run(30000, [BLUE, RED, YELLOW, RED, YELLOW, BLUE, RED, YELLOW, RED, BLUE]) and ok
report("chameneos_redux", t, ok)
