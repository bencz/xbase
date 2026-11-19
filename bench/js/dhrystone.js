// bench/dhrystone.prg in JavaScript: Dhrystone 2.1, its procedures and functions as the classic C program has them,
// 1,000,000 runs: records as arrays (a reference to the next record, the discriminant, an enumeration, an integer and
// a string), character and string assignments and comparisons, a two-dimensional array, calls with parameters by value
// and by reference (Proc2's parameter by reference, and a result the original stores through a pointer into a
// record's field, are returned instead). The values the original prints at the end are checked. Records and arrays are
// numbered from 1, as in the .prg (element 0 unused).
const { report } = require("./bench.js");
const PTR_COMP = 1, DISCR = 2, ENUM_COMP = 3, INT_COMP = 4, STR_COMP = 5;
const IDENT_1 = 0, IDENT_2 = 1, IDENT_3 = 2, IDENT_4 = 3, IDENT_5 = 4;
const RUNS = 1000000;

let intGlob = 0, boolGlob = false, ch1Glob = " ", ch2Glob = " ";
let arr1Glob, arr2Glob, nextPtrGlob, ptrGlob;

function proc1(ptrValPar) {
  const nextRecord = ptrValPar[PTR_COMP];
  for (let i = 1; i <= 5; i++) ptrValPar[PTR_COMP][i] = ptrGlob[i];
  ptrValPar[INT_COMP] = 5;
  nextRecord[INT_COMP] = ptrValPar[INT_COMP];
  nextRecord[PTR_COMP] = ptrValPar[PTR_COMP];
  nextRecord[PTR_COMP] = proc3(nextRecord[PTR_COMP]);
  if (nextRecord[DISCR] === IDENT_1) {
    nextRecord[INT_COMP] = 6;
    nextRecord[ENUM_COMP] = proc6(ptrValPar[ENUM_COMP]);
    nextRecord[PTR_COMP] = ptrGlob[PTR_COMP];
    nextRecord[INT_COMP] = proc7(nextRecord[INT_COMP], 10);
  } else {
    for (let i = 1; i <= 5; i++) ptrValPar[i] = ptrValPar[PTR_COMP][i];
  }
}

// The parameter by reference: its new value is returned.
function proc2(intParRef) {
  let intLoc = intParRef + 10, enumLoc;
  for (;;) {
    if (ch1Glob === "A") {
      intLoc--;
      intParRef = intLoc - intGlob;
      enumLoc = IDENT_1;
    }
    if (enumLoc === IDENT_1) break;
  }
  return intParRef;
}

function proc3(ptrRefPar) {
  if (ptrGlob !== undefined) ptrRefPar = ptrGlob[PTR_COMP];
  ptrGlob[INT_COMP] = proc7(10, intGlob);
  return ptrRefPar;
}

function proc4() {
  const boolLoc = ch1Glob === "A";
  boolGlob = boolLoc || boolGlob;
  ch2Glob = "B";
}

function proc5() {
  ch1Glob = "A";
  boolGlob = false;
}

function proc6(enumValPar) {
  let enumRefPar = enumValPar;
  if (!func3(enumValPar)) enumRefPar = IDENT_4;
  if (enumValPar === IDENT_1) enumRefPar = IDENT_1;
  else if (enumValPar === IDENT_2) enumRefPar = intGlob > 100 ? IDENT_1 : IDENT_4;
  else if (enumValPar === IDENT_3) enumRefPar = IDENT_2;
  else if (enumValPar === IDENT_5) enumRefPar = IDENT_3;
  return enumRefPar;
}

function proc7(int1ParVal, int2ParVal) {
  const intLoc = int1ParVal + 2;
  return int2ParVal + intLoc;
}

// The arrays are indexed from 1 where the original's are from 0.
function proc8(arr1ParRef, arr2ParRef, int1ParVal, int2ParVal) {
  const intLoc = int1ParVal + 5;
  arr1ParRef[intLoc + 1] = int2ParVal;
  arr1ParRef[intLoc + 2] = arr1ParRef[intLoc + 1];
  arr1ParRef[intLoc + 31] = intLoc;
  for (let intIndex = intLoc; intIndex <= intLoc + 1; intIndex++) arr2ParRef[intLoc + 1][intIndex + 1] = intLoc;
  arr2ParRef[intLoc + 1][intLoc]++;
  arr2ParRef[intLoc + 21][intLoc + 1] = arr1ParRef[intLoc + 1];
  intGlob = 5;
}

function func1(ch1ParVal, ch2ParVal) {
  const ch1Loc = ch1ParVal, ch2Loc = ch1Loc;
  if (ch2Loc !== ch2ParVal) return IDENT_1;
  ch1Glob = ch1Loc;
  return IDENT_2;
}

// The strings are indexed from 1 where the original's are from 0 (charAt from 0 here: the same characters).
function func2(str1ParRef, str2ParRef) {
  let intLoc = 2, chLoc;
  while (intLoc <= 2) {
    if (func1(str1ParRef.charAt(intLoc), str2ParRef.charAt(intLoc + 1)) === IDENT_1) {
      chLoc = "A";
      intLoc++;
    }
  }
  if (chLoc >= "W" && chLoc < "Z") intLoc = 7;
  if (chLoc === "R") return true;
  if (str1ParRef > str2ParRef) {
    intLoc += 7;
    intGlob = intLoc;
    return true;
  }
  return false;
}

function func3(enumParVal) {
  const enumLoc = enumParVal;
  return enumLoc === IDENT_3;
}

const t = performance.now();
arr1Glob = new Array(51).fill(0);
arr2Glob = Array.from({ length: 51 }, () => new Array(51).fill(0));
nextPtrGlob = new Array(6);
ptrGlob = [undefined, nextPtrGlob, IDENT_1, IDENT_3, 40, "DHRYSTONE PROGRAM, SOME STRING"];
let int1Loc, int2Loc, int3Loc = 0, enumLoc, str2Loc;
const str1Loc = "DHRYSTONE PROGRAM, 1'ST STRING";
arr2Glob[9][8] = 10;
for (let run = 1; run <= RUNS; run++) {
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
  proc8(arr1Glob, arr2Glob, int1Loc, int3Loc);
  proc1(ptrGlob);
  for (let ch = "A".charCodeAt(0); ch <= ch2Glob.charCodeAt(0); ch++) {
    if (enumLoc === func1(String.fromCharCode(ch), "C")) {
      enumLoc = proc6(IDENT_1);
      str2Loc = "DHRYSTONE PROGRAM, 3'RD STRING";
      int2Loc = run;
      intGlob = run;
    }
  }
  int2Loc = int2Loc * int1Loc;
  int1Loc = Math.trunc(int2Loc / int3Loc);
  int2Loc = 7 * (int2Loc - int3Loc) - int1Loc;
  int1Loc = proc2(int1Loc);
}
const ok = intGlob === 5 && boolGlob && ch1Glob === "A" && ch2Glob === "B" && arr1Glob[9] === 7 &&
  arr2Glob[9][8] === RUNS + 10 && ptrGlob[PTR_COMP] === nextPtrGlob && ptrGlob[DISCR] === IDENT_1 &&
  ptrGlob[ENUM_COMP] === IDENT_3 && ptrGlob[INT_COMP] === 17 &&
  ptrGlob[STR_COMP] === "DHRYSTONE PROGRAM, SOME STRING" &&
  nextPtrGlob[PTR_COMP] === nextPtrGlob && nextPtrGlob[DISCR] === IDENT_1 && nextPtrGlob[ENUM_COMP] === IDENT_2 &&
  nextPtrGlob[INT_COMP] === 18 && nextPtrGlob[STR_COMP] === "DHRYSTONE PROGRAM, SOME STRING" && int1Loc === 5 &&
  int2Loc === 13 && int3Loc === 7 && enumLoc === IDENT_2 && str1Loc === "DHRYSTONE PROGRAM, 1'ST STRING" &&
  str2Loc === "DHRYSTONE PROGRAM, 2'ND STRING";
report("dhrystone", t, ok);
