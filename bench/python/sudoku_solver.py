# bench/sudoku_solver.prg in Python: recursion and lists of booleans, five hard sudoku puzzles solved by backtracking
# (the first empty cell tries each digit its row, column and box allow), checked by a hash of the solved grids,
# 3 times. The used-digit lists keep the .prg's indexes (digit v of unit u at u * 9 + v, element 0 unused).
from bench import report, start

PUZZLES = [
    "8..........36......7..9.2...5...7.......457.....1...3...1....68..85...1..9....4..",
    "6.....8.3.4.7.................5.4.7.3..2.....1.6.......2.....5.....8.6......1....",
    "48.3............71.2.......7.5....6....2..8.............1.76...3.....4......5....",
    "..53.....8......2..7..1.5..4....53...1..7...6..32...8..6.5....9..4....3......97..",
    "1....7.9..3..2...8..96..5....53..9...1..8...26....4...3......1..4......7..7...3..",
]


def mark(p, v, on, rows, cols, boxes):
    r = p // 9
    c = p - p // 9 * 9
    rows[r * 9 + v] = on
    cols[c * 9 + v] = on
    boxes[(r // 3 * 3 + c // 3) * 9 + v] = on


def solve(p, grid, rows, cols, boxes):
    while p < 81 and grid[p] != 0:
        p += 1
    if p == 81:
        return True
    r = p // 9
    c = p - r * 9
    b = r // 3 * 3 + c // 3
    for v in range(1, 10):
        if rows[r * 9 + v] or cols[c * 9 + v] or boxes[b * 9 + v]:
            continue
        grid[p] = v
        mark(p, v, True, rows, cols, boxes)
        if solve(p + 1, grid, rows, cols, boxes):
            return True
        mark(p, v, False, rows, cols, boxes)
        grid[p] = 0
    return False


def run():
    ok = True
    for _ in range(3):
        total = 0
        for puzzle in PUZZLES:
            grid = [0] * 81
            rows, cols, boxes = [False] * 82, [False] * 82, [False] * 82
            for i in range(81):
                grid[i] = 0 if puzzle[i] == "." else ord(puzzle[i]) - ord("0")
                if grid[i] != 0:
                    mark(i, grid[i], True, rows, cols, boxes)
            ok = ok and solve(0, grid, rows, cols, boxes)
            for i in range(81):
                total = (total * 31 + grid[i]) & 0xFFFFFFF
        ok = ok and total == 252679381
    return ok


t = start()
report("sudoku_solver", t, run())
