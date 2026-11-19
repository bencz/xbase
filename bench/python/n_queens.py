# bench/n_queens.prg in Python: backtracking over lists of booleans, the solutions of the 12 queens problem (taken
# columns and diagonals), 3 times.
from bench import report, start


def place(row, n, col, d1, d2):
    if row >= n:
        return 1
    count = 0
    for c in range(n):
        if not col[c] and not d1[row + c] and not d2[row - c + n - 1]:
            col[c] = True
            d1[row + c] = True
            d2[row - c + n - 1] = True
            count += place(row + 1, n, col, d1, d2)
            col[c] = False
            d1[row + c] = False
            d2[row - c + n - 1] = False
    return count


t = start()
report("n_queens", t, all([place(0, 12, [False] * 12, [False] * 23, [False] * 23) == 14200 for _ in range(3)]))
