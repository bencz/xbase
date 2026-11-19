# bench/pipeline_tasks.prg in Python: a pipeline of four coroutines joined by bounded asyncio queues (capacity 256): a
# source sends 200,000 records as text ("id;value", the values from a fixed seed), a parser splits them into
# (id, value), a stage maps each value to its square modulo 1,000,003 and keeps the even ones, and a sink sums them.
# Each stage awaits its puts and gets and ends its output with an end mark (asyncio's queue has no close) when its
# input ends. The sums must be the same whatever the scheduling.
import asyncio

from bench import report, start

END = None


async def source(out, n):
    x = 4321
    for i in range(1, n + 1):
        x = x * 16807 % 2147483647
        await out.put(str(i) + ";" + str(x % 100000))
    await out.put(END)


async def parser(cin, out):
    while (c := await cin.get()) is not END:
        n = c.find(";")
        await out.put((int(c[:n]), int(c[n + 1:])))
    await out.put(END)


async def square(cin, out):
    while (a := await cin.get()) is not END:
        y = a[1] * a[1] % 1000003
        if y % 2 == 0:
            await out.put((a[0], y))
    await out.put(END)


async def sink(cin):
    total = count = ids = 0
    while (a := await cin.get()) is not END:
        total += a[1]
        count += 1
        ids += a[0]
    return total, count, ids


async def main():
    t = start()
    c1, c2, c3 = asyncio.Queue(256), asyncio.Queue(256), asyncio.Queue(256)
    tasks = [asyncio.ensure_future(source(c1, 200000)), asyncio.ensure_future(parser(c1, c2)),
             asyncio.ensure_future(square(c2, c3)), asyncio.ensure_future(sink(c3))]
    result = await tasks[3]
    await asyncio.gather(*tasks)
    report("pipeline_tasks", t, result == (49893566082, 100073, 10032092994))


asyncio.run(main())
