# bench/producer_consumer.prg in Python: producers and consumers on threads over a bounded queue of 64 (a list) guarded
# by a lock and two conditions (not full, not empty): 4 producers put 50,000 numbers each, 4 consumers take them until
# each finds the end mark the program puts once the producers are joined. The sum and count of what was taken must be
# what was put, whatever the scheduling. A thread's result (the .prg's Join() value) is kept in a list.
import threading

from bench import report, start


def put(lock, not_full, not_empty, q, x):
    with lock:
        while len(q) >= 64:
            not_full.wait()
        q.append(x)
        not_empty.notify()


def produce(lock, not_full, not_empty, q, p, n):
    for i in range(1, n + 1):
        put(lock, not_full, not_empty, q, p * 1000000 + i)


def consume(lock, not_full, not_empty, q, results, k):
    total = count = 0
    while True:
        with lock:
            while len(q) == 0:
                not_empty.wait()
            x = q.pop(0)
            not_full.notify()
        if x is None:
            break
        total += x
        count += 1
    results[k] = (total, count)


def run(n_items):
    lock = threading.Lock()
    not_full, not_empty, q, results = threading.Condition(lock), threading.Condition(lock), [], [None] * 4
    consumers = [threading.Thread(target=consume, args=(lock, not_full, not_empty, q, results, k)) for k in range(4)]
    producers = [threading.Thread(target=produce, args=(lock, not_full, not_empty, q, p, n_items)) for p in range(1, 5)]
    for th in consumers + producers:
        th.start()
    for th in producers:
        th.join()
    for _ in range(4):
        put(lock, not_full, not_empty, q, None)
    for th in consumers:
        th.join()
    total = sum(r[0] for r in results)
    count = sum(r[1] for r in results)
    return count == 4 * n_items and total == 10000000 * n_items + 2 * n_items * (n_items + 1)


t = start()
report("producer_consumer", t, run(50000))
