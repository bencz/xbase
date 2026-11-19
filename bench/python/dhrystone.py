# bench/dhrystone.prg in Python: Dhrystone 2.1, its procedures and functions as the classic C program has them,
# 1,000,000 runs: records as lists (a reference to the next record, the discriminant, an enumeration, an integer and a
# string), character and string assignments and comparisons, a two-dimensional list, calls with parameters by value;
# the parameter Proc2 takes by reference is returned instead, as the .prg already returns the result the original
# stores through a pointer into a record's field. The values the original prints at the end are checked.
from bench import report, start

PTR_COMP, DISCR, ENUM_COMP, INT_COMP, STR_COMP = 0, 1, 2, 3, 4
IDENT_1, IDENT_2, IDENT_3, IDENT_4, IDENT_5 = 0, 1, 2, 3, 4
RUNS = 1000000

int_glob, bool_glob, ch1_glob, ch2_glob = 0, False, " ", " "
arr1_glob = arr2_glob = ptr_glob = next_ptr_glob = None


def proc1(ptr_val_par):
    next_record = ptr_val_par[PTR_COMP]
    next_record[:] = ptr_glob
    ptr_val_par[INT_COMP] = 5
    next_record[INT_COMP] = ptr_val_par[INT_COMP]
    next_record[PTR_COMP] = ptr_val_par[PTR_COMP]
    next_record[PTR_COMP] = proc3(next_record[PTR_COMP])
    if next_record[DISCR] == IDENT_1:
        next_record[INT_COMP] = 6
        next_record[ENUM_COMP] = proc6(ptr_val_par[ENUM_COMP])
        next_record[PTR_COMP] = ptr_glob[PTR_COMP]
        next_record[INT_COMP] = proc7(next_record[INT_COMP], 10)
    else:
        ptr_val_par[:] = ptr_val_par[PTR_COMP]


def proc2(int_par_ref):
    int_loc = int_par_ref + 10
    enum_loc = None
    while True:
        if ch1_glob == "A":
            int_loc -= 1
            int_par_ref = int_loc - int_glob
            enum_loc = IDENT_1
        if enum_loc == IDENT_1:
            break
    return int_par_ref


def proc3(ptr_ref_par):
    if ptr_glob is not None:
        ptr_ref_par = ptr_glob[PTR_COMP]
    ptr_glob[INT_COMP] = proc7(10, int_glob)
    return ptr_ref_par


def proc4():
    global bool_glob, ch2_glob
    bool_loc = ch1_glob == "A"
    bool_glob = bool_loc or bool_glob
    ch2_glob = "B"


def proc5():
    global ch1_glob, bool_glob
    ch1_glob = "A"
    bool_glob = False


def proc6(enum_val_par):
    enum_ref_par = enum_val_par
    if not func3(enum_val_par):
        enum_ref_par = IDENT_4
    if enum_val_par == IDENT_1:
        enum_ref_par = IDENT_1
    elif enum_val_par == IDENT_2:
        if int_glob > 100:
            enum_ref_par = IDENT_1
        else:
            enum_ref_par = IDENT_4
    elif enum_val_par == IDENT_3:
        enum_ref_par = IDENT_2
    elif enum_val_par == IDENT_5:
        enum_ref_par = IDENT_3
    return enum_ref_par


def proc7(int1_par_val, int2_par_val):
    int_loc = int1_par_val + 2
    return int2_par_val + int_loc


def proc8(arr1_par_ref, arr2_par_ref, int1_par_val, int2_par_val):
    global int_glob
    int_loc = int1_par_val + 5
    arr1_par_ref[int_loc] = int2_par_val
    arr1_par_ref[int_loc + 1] = arr1_par_ref[int_loc]
    arr1_par_ref[int_loc + 30] = int_loc
    for int_index in range(int_loc, int_loc + 2):
        arr2_par_ref[int_loc][int_index] = int_loc
    arr2_par_ref[int_loc][int_loc - 1] += 1
    arr2_par_ref[int_loc + 20][int_loc] = arr1_par_ref[int_loc]
    int_glob = 5


def func1(ch1_par_val, ch2_par_val):
    global ch1_glob
    ch1_loc = ch1_par_val
    ch2_loc = ch1_loc
    if ch2_loc != ch2_par_val:
        return IDENT_1
    ch1_glob = ch1_loc
    return IDENT_2


def func2(str1_par_ref, str2_par_ref):
    global int_glob
    int_loc = 2
    ch_loc = None
    while int_loc <= 2:
        if func1(str1_par_ref[int_loc], str2_par_ref[int_loc + 1]) == IDENT_1:
            ch_loc = "A"
            int_loc += 1
    if "W" <= ch_loc < "Z":
        int_loc = 7
    if ch_loc == "R":
        return True
    if str1_par_ref > str2_par_ref:
        int_loc += 7
        int_glob = int_loc
        return True
    return False


def func3(enum_par_val):
    enum_loc = enum_par_val
    return enum_loc == IDENT_3


def run():
    global bool_glob, int_glob, arr1_glob, arr2_glob, ptr_glob, next_ptr_glob
    int3_loc = 0
    arr1_glob = [0] * 50
    arr2_glob = [[0] * 50 for _ in range(50)]
    next_ptr_glob = [None] * 5
    ptr_glob = [next_ptr_glob, IDENT_1, IDENT_3, 40, "DHRYSTONE PROGRAM, SOME STRING"]
    str1_loc = "DHRYSTONE PROGRAM, 1'ST STRING"
    arr2_glob[8][7] = 10
    for run_no in range(1, RUNS + 1):
        proc5()
        proc4()
        int1_loc = 2
        int2_loc = 3
        str2_loc = "DHRYSTONE PROGRAM, 2'ND STRING"
        enum_loc = IDENT_2
        bool_glob = not func2(str1_loc, str2_loc)
        while int1_loc < int2_loc:
            int3_loc = 5 * int1_loc - int2_loc
            int3_loc = proc7(int1_loc, int2_loc)
            int1_loc += 1
        proc8(arr1_glob, arr2_glob, int1_loc, int3_loc)
        proc1(ptr_glob)
        for ch in range(ord("A"), ord(ch2_glob) + 1):
            if enum_loc == func1(chr(ch), "C"):
                enum_loc = proc6(IDENT_1)
                str2_loc = "DHRYSTONE PROGRAM, 3'RD STRING"
                int2_loc = run_no
                int_glob = run_no
        int2_loc = int2_loc * int1_loc
        int1_loc = int(int2_loc / int3_loc)
        int2_loc = 7 * (int2_loc - int3_loc) - int1_loc
        int1_loc = proc2(int1_loc)
    return (int_glob == 5 and bool_glob and ch1_glob == "A" and ch2_glob == "B" and arr1_glob[8] == 7
            and arr2_glob[8][7] == RUNS + 10 and ptr_glob[PTR_COMP] is next_ptr_glob and ptr_glob[DISCR] == IDENT_1
            and ptr_glob[ENUM_COMP] == IDENT_3 and ptr_glob[INT_COMP] == 17
            and ptr_glob[STR_COMP] == "DHRYSTONE PROGRAM, SOME STRING" and next_ptr_glob[PTR_COMP] is next_ptr_glob
            and next_ptr_glob[DISCR] == IDENT_1 and next_ptr_glob[ENUM_COMP] == IDENT_2
            and next_ptr_glob[INT_COMP] == 18 and next_ptr_glob[STR_COMP] == "DHRYSTONE PROGRAM, SOME STRING"
            and int1_loc == 5 and int2_loc == 13 and int3_loc == 7 and enum_loc == IDENT_2
            and str1_loc == "DHRYSTONE PROGRAM, 1'ST STRING" and str2_loc == "DHRYSTONE PROGRAM, 2'ND STRING")


t = start()
report("dhrystone", t, run())
