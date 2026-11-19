// bench/dhrystone.prg in Java: Dhrystone 2.1, its procedures and functions as the classic C program has them,
// 1,000,000 runs: records as objects, characters as char, a two-dimensional array, results the original stores through
// a pointer returned instead (Proc2's parameter by reference too); the values the original prints are checked.
public class Dhrystone {
  static final int IDENT_1 = 0, IDENT_2 = 1, IDENT_3 = 2, IDENT_4 = 3, IDENT_5 = 4;
  static final int RUNS = 1000000;

  static class Record {
    Record ptrComp;
    int discr, enumComp;
    long intComp;
    String strComp;

    void copyFrom(Record r) {
      ptrComp = r.ptrComp;
      discr = r.discr;
      enumComp = r.enumComp;
      intComp = r.intComp;
      strComp = r.strComp;
    }
  }

  static long intGlob;
  static boolean boolGlob;
  static char ch1Glob = ' ', ch2Glob = ' ';
  static final long[] ARR1_GLOB = new long[50];
  static final long[][] ARR2_GLOB = new long[50][50];
  static Record ptrGlob, nextPtrGlob;

  static void proc1(Record ptrValPar) {
    Record nextRecord = ptrValPar.ptrComp;
    ptrValPar.ptrComp.copyFrom(ptrGlob);
    ptrValPar.intComp = 5;
    nextRecord.intComp = ptrValPar.intComp;
    nextRecord.ptrComp = ptrValPar.ptrComp;
    nextRecord.ptrComp = proc3(nextRecord.ptrComp);
    if (nextRecord.discr == IDENT_1) {
      nextRecord.intComp = 6;
      nextRecord.enumComp = proc6(ptrValPar.enumComp);
      nextRecord.ptrComp = ptrGlob.ptrComp;
      nextRecord.intComp = proc7(nextRecord.intComp, 10);
    } else {
      ptrValPar.copyFrom(ptrValPar.ptrComp);
    }
  }

  static long proc2(long intParRef) {
    long intLoc = intParRef + 10;
    int enumLoc = IDENT_2;
    while (true) {
      if (ch1Glob == 'A') {
        intLoc--;
        intParRef = intLoc - intGlob;
        enumLoc = IDENT_1;
      }
      if (enumLoc == IDENT_1) return intParRef;
    }
  }

  static Record proc3(Record ptrRefPar) {
    if (ptrGlob != null) ptrRefPar = ptrGlob.ptrComp;
    ptrGlob.intComp = proc7(10, intGlob);
    return ptrRefPar;
  }

  static void proc4() {
    boolean boolLoc = ch1Glob == 'A';
    boolGlob = boolLoc || boolGlob;
    ch2Glob = 'B';
  }

  static void proc5() {
    ch1Glob = 'A';
    boolGlob = false;
  }

  static int proc6(int enumValPar) {
    int enumRefPar = enumValPar;
    if (!func3(enumValPar)) enumRefPar = IDENT_4;
    switch (enumValPar) {
      case IDENT_1 -> enumRefPar = IDENT_1;
      case IDENT_2 -> enumRefPar = intGlob > 100 ? IDENT_1 : IDENT_4;
      case IDENT_3 -> enumRefPar = IDENT_2;
      case IDENT_5 -> enumRefPar = IDENT_3;
      default -> { }
    }
    return enumRefPar;
  }

  static long proc7(long int1ParVal, long int2ParVal) {
    long intLoc = int1ParVal + 2;
    return int2ParVal + intLoc;
  }

  static void proc8(long[] arr1ParRef, long[][] arr2ParRef, long int1ParVal, long int2ParVal) {
    int intLoc = (int) int1ParVal + 5;
    arr1ParRef[intLoc] = int2ParVal;
    arr1ParRef[intLoc + 1] = arr1ParRef[intLoc];
    arr1ParRef[intLoc + 30] = intLoc;
    for (int intIndex = intLoc; intIndex <= intLoc + 1; intIndex++) arr2ParRef[intLoc][intIndex] = intLoc;
    arr2ParRef[intLoc][intLoc - 1]++;
    arr2ParRef[intLoc + 20][intLoc] = arr1ParRef[intLoc];
    intGlob = 5;
  }

  static int func1(char ch1ParVal, char ch2ParVal) {
    char ch1Loc = ch1ParVal, ch2Loc = ch1Loc;
    if (ch2Loc != ch2ParVal) return IDENT_1;
    ch1Glob = ch1Loc;
    return IDENT_2;
  }

  static boolean func2(String str1ParRef, String str2ParRef) {
    int intLoc = 2;
    char chLoc = 0;
    while (intLoc <= 2)
      if (func1(str1ParRef.charAt(intLoc), str2ParRef.charAt(intLoc + 1)) == IDENT_1) {
        chLoc = 'A';
        intLoc++;
      }
    if (chLoc >= 'W' && chLoc < 'Z') intLoc = 7;
    if (chLoc == 'R') return true;
    if (str1ParRef.compareTo(str2ParRef) > 0) {
      intLoc += 7;
      intGlob = intLoc;
      return true;
    }
    return false;
  }

  static boolean func3(int enumParVal) {
    int enumLoc = enumParVal;
    return enumLoc == IDENT_3;
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), int1Loc = 0, int2Loc = 0, int3Loc = 0;
    int enumLoc = IDENT_1;
    nextPtrGlob = new Record();
    ptrGlob = new Record();
    ptrGlob.ptrComp = nextPtrGlob;
    ptrGlob.discr = IDENT_1;
    ptrGlob.enumComp = IDENT_3;
    ptrGlob.intComp = 40;
    ptrGlob.strComp = "DHRYSTONE PROGRAM, SOME STRING";
    String str1Loc = "DHRYSTONE PROGRAM, 1'ST STRING", str2Loc = null;
    ARR2_GLOB[8][7] = 10;
    for (long run = 1; run <= RUNS; run++) {
      proc5();
      proc4();
      int1Loc = 2;
      int2Loc = 3;
      str2Loc = "DHRYSTONE PROGRAM, 2'ND STRING";
      enumLoc = IDENT_2;
      boolGlob = !func2(str1Loc, str2Loc);
      while (int1Loc < int2Loc) {
        int3Loc = 5 * int1Loc - int2Loc;
        int3Loc = proc7(int1Loc, int2Loc);
        int1Loc++;
      }
      proc8(ARR1_GLOB, ARR2_GLOB, int1Loc, int3Loc);
      proc1(ptrGlob);
      for (char ch = 'A'; ch <= ch2Glob; ch++) {
        if (enumLoc == func1(ch, 'C')) {
          enumLoc = proc6(IDENT_1);
          str2Loc = "DHRYSTONE PROGRAM, 3'RD STRING";
          int2Loc = run;
          intGlob = run;
        }
      }
      int2Loc = int2Loc * int1Loc;
      int1Loc = int2Loc / int3Loc;
      int2Loc = 7 * (int2Loc - int3Loc) - int1Loc;
      int1Loc = proc2(int1Loc);
    }
    String some = "DHRYSTONE PROGRAM, SOME STRING";
    boolean ok = intGlob == 5 && boolGlob && ch1Glob == 'A' && ch2Glob == 'B' && ARR1_GLOB[8] == 7
        && ARR2_GLOB[8][7] == RUNS + 10 && ptrGlob.ptrComp == nextPtrGlob && ptrGlob.discr == IDENT_1
        && ptrGlob.enumComp == IDENT_3 && ptrGlob.intComp == 17 && some.equals(ptrGlob.strComp)
        && nextPtrGlob.ptrComp == nextPtrGlob && nextPtrGlob.discr == IDENT_1 && nextPtrGlob.enumComp == IDENT_2
        && nextPtrGlob.intComp == 18 && some.equals(nextPtrGlob.strComp) && int1Loc == 5 && int2Loc == 13
        && int3Loc == 7 && enumLoc == IDENT_2 && str1Loc.equals("DHRYSTONE PROGRAM, 1'ST STRING")
        && str2Loc.equals("DHRYSTONE PROGRAM, 2'ND STRING");
    Bench.report("dhrystone", t, ok);
  }
}
