# bench/channels.prg in Python: 4 producer threads send 250,000 numbers each through a bounded queue (capacity 64) to 2
# consumer threads, then 1,000,000 put/get pairs on an unbounded queue from one thread.
import queue
import threading
from bench import report, start

END = None


def run():
    ch, sums = queue.Queue(64), []

    def consume():
        s = 0
        while (x := ch.get()) is not END:
            s += x
        sums.append(s)

    def produce():
        for i in range(1, 250001):
            ch.put(i)

    consumers = [threading.Thread(target=consume) for _ in range(2)]
    producers = [threading.Thread(target=produce) for _ in range(4)]
    for th in consumers + producers:
        th.start()
    for th in producers:
        th.join()
    for _ in consumers:  # the channel closed: one end mark for each consumer
        ch.put(END)
    for th in consumers:
        th.join()
    s, u = sum(sums), queue.SimpleQueue()
    for k in range(1, 1000001):
        u.put(k)
        s += u.get() % 2
    return s


t = start()
report("channels", t, run() == 4 * 31250125000 + 500000)
