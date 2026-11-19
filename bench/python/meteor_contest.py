# bench/meteor_contest.prg in Python: bit masks and a recursive search, every way to fill the meteor contest's board
# (5 x 10 hexagonal cells, odd rows shifted half a cell right) with its 10 pieces of 5 cells. The pieces are read from
# a solution; their placements (12 orientations, every position, duplicates dropped) are 64-bit masks listed by piece
# and lowest cell. The search fills the lowest empty cell first and drops a placement that leaves an empty cell walled
# in among the next 10. Piece 3 takes only 6 of its orientations, and every solution found counts for itself and its
# turned image (the reversed string). The solution is a list of characters joined where the .prg uses Stuff() on a
# string. Checked: 2098 solutions, and the smallest and largest as strings of 50 digits.
from bench import report, start

COLS, ROWS, CELLS, HALVED, WINDOW = 5, 10, 50, 3, 10

masks = cells = by = neighbors = placed = None
n_solutions, best, worst = 0, None, None


def to_axial(cell):
    row = cell // COLS
    return [cell - row * COLS - (row - (row & 1)) // 2, row]


def to_cell(q, r):
    col = q + (r - (r & 1)) // 2
    if r < 0 or r >= ROWS or col < 0 or col >= COLS:
        return -1
    return r * COLS + col


def make_placements(shape):
    global masks, cells, by, neighbors
    dq, dr = [1, -1, 0, 0, 1, -1], [0, 0, 1, -1, -1, 1]
    neighbors = [0] * CELLS
    for c in range(CELLS):
        at = to_axial(c)
        for i in range(6):
            x = to_cell(at[0] + dq[i], at[1] + dr[i])
            if x >= 0:
                neighbors[c] |= 1 << x
    masks, cells, by = [], [], []
    for p in range(10):
        masks.append([])
        cells.append([])
        by.append([[] for _ in range(CELLS)])
        pts = [to_axial(c) for c in range(CELLS) if ord(shape[c]) - 48 == p]
        seen = {}
        for o in range(12):
            for n_at in range(CELLS):
                if p == HALVED and o % 6 >= 3:
                    break
                at = to_axial(n_at)
                place = [0] * 5
                ok = True
                for i in range(5):
                    x = to_cell(pts[i][0] - pts[0][0] + at[0], pts[i][1] - pts[0][1] + at[1])
                    place[i] = x
                    if x < 0:
                        ok = False
                        break
                if ok:
                    mask, low = 0, CELLS
                    for i in range(5):
                        mask |= 1 << place[i]
                        low = min(low, place[i])
                    if mask not in seen:
                        seen[mask] = True
                        masks[p].append(mask)
                        cells[p].append(place)
                        by[p][low].append(len(masks[p]) - 1)
            for i in range(5):
                q = pts[i][0]
                if o == 5:
                    pts[i][0] = pts[i][1]
                    pts[i][1] = q
                else:
                    pts[i][0] = -pts[i][1]
                    pts[i][1] = q + pts[i][1]


def walled_in(board, cell):
    for c in range(cell, min(cell + WINDOW, CELLS)):
        if ((board >> c) & 1) == 0 and (board & neighbors[c]) == neighbors[c]:
            return True
    return False


def record(sol):
    global n_solutions, best, worst
    if n_solutions == 0 or sol < best:
        best = sol
    if n_solutions == 0 or sol > worst:
        worst = sol
    n_solutions += 1


def solve(board, cell, depth):
    while ((board >> cell) & 1) == 1:
        cell += 1
    if depth == 10:
        sol = [" "] * CELLS
        for p in range(10):
            for i in cells[p][placed[p]]:
                sol[i] = chr(48 + p)
        sol = "".join(sol)
        record(sol)
        record(sol[::-1])
        return
    for p in range(10):
        if placed[p] is None:
            for k in by[p][cell]:
                nxt = board | masks[p][k]
                if (board & masks[p][k]) == 0 and not walled_in(nxt, cell):
                    placed[p] = k
                    solve(nxt, cell, depth + 1)
            placed[p] = None


t = start()
make_placements("99998966856688568255777257472014220144031400311333")
placed = [None] * 10
solve(0, 0, 0)
report("meteor_contest", t, n_solutions == 2098 and best == "00001222012661126155865558633348893448934747977799"
       and worst == "99998966856688568255777257472014220144031400311333")
