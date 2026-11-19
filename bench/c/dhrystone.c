/* bench/dhrystone.prg in C: Dhrystone 2.1 as the classic program has it, 1,000,000 runs. The procedures may not be
 * inlined (noinline, noipa), as when the classic is built from its two files, and as xbase calls its functions. */
#include "bench.h"
#include <stdbool.h>
#include <string.h>

#define RUNS 1000000
#define PROC __attribute__((noinline, noipa)) static

typedef enum { Ident_1, Ident_2, Ident_3, Ident_4, Ident_5 } Enumeration;

typedef struct Record {
  struct Record *Ptr_Comp;
  Enumeration Discr;
  Enumeration Enum_Comp;
  int64_t Int_Comp;
  char Str_Comp[31];
} Record;

static Record Glob_Rec, Next_Glob_Rec, *Ptr_Glob, *Next_Ptr_Glob;
static int64_t Int_Glob, Arr_1_Glob[50], Arr_2_Glob[50][50];
static bool Bool_Glob;
static char Ch_1_Glob, Ch_2_Glob;

PROC void Proc_7(int64_t Int_1_Par_Val, int64_t Int_2_Par_Val, int64_t *Int_Par_Ref) {
  int64_t Int_Loc = Int_1_Par_Val + 2;
  *Int_Par_Ref = Int_2_Par_Val + Int_Loc;
}

PROC Enumeration Func_1(char Ch_1_Par_Val, char Ch_2_Par_Val) {
  char Ch_1_Loc = Ch_1_Par_Val, Ch_2_Loc = Ch_1_Loc;
  if (Ch_2_Loc != Ch_2_Par_Val) return Ident_1;
  Ch_1_Glob = Ch_1_Loc;
  return Ident_2;
}

PROC bool Func_3(Enumeration Enum_Par_Val) {
  Enumeration Enum_Loc = Enum_Par_Val;
  return Enum_Loc == Ident_3;
}

PROC void Proc_6(Enumeration Enum_Val_Par, Enumeration *Enum_Ref_Par) {
  *Enum_Ref_Par = Enum_Val_Par;
  if (!Func_3(Enum_Val_Par)) *Enum_Ref_Par = Ident_4;
  switch (Enum_Val_Par) {
  case Ident_1: *Enum_Ref_Par = Ident_1; break;
  case Ident_2: *Enum_Ref_Par = Int_Glob > 100 ? Ident_1 : Ident_4; break;
  case Ident_3: *Enum_Ref_Par = Ident_2; break;
  case Ident_4: break;
  case Ident_5: *Enum_Ref_Par = Ident_3; break;
  }
}

PROC void Proc_3(Record **Ptr_Ref_Par) {
  if (Ptr_Glob != NULL) *Ptr_Ref_Par = Ptr_Glob->Ptr_Comp;
  Proc_7(10, Int_Glob, &Ptr_Glob->Int_Comp);
}

PROC void Proc_1(Record *Ptr_Val_Par) {
  Record *Next_Record = Ptr_Val_Par->Ptr_Comp;
  *Ptr_Val_Par->Ptr_Comp = *Ptr_Glob;
  Ptr_Val_Par->Int_Comp = 5;
  Next_Record->Int_Comp = Ptr_Val_Par->Int_Comp;
  Next_Record->Ptr_Comp = Ptr_Val_Par->Ptr_Comp;
  Proc_3(&Next_Record->Ptr_Comp);
  if (Next_Record->Discr == Ident_1) {
    Next_Record->Int_Comp = 6;
    Proc_6(Ptr_Val_Par->Enum_Comp, &Next_Record->Enum_Comp);
    Next_Record->Ptr_Comp = Ptr_Glob->Ptr_Comp;
    Proc_7(Next_Record->Int_Comp, 10, &Next_Record->Int_Comp);
  } else
    *Ptr_Val_Par = *Ptr_Val_Par->Ptr_Comp;
}

PROC void Proc_2(int64_t *Int_Par_Ref) {
  int64_t Int_Loc = *Int_Par_Ref + 10;
  Enumeration Enum_Loc = Ident_2;
  do
    if (Ch_1_Glob == 'A') {
      Int_Loc -= 1;
      *Int_Par_Ref = Int_Loc - Int_Glob;
      Enum_Loc = Ident_1;
    }
  while (Enum_Loc != Ident_1);
}

PROC void Proc_4(void) {
  bool Bool_Loc = Ch_1_Glob == 'A';
  Bool_Glob = Bool_Loc | Bool_Glob;
  Ch_2_Glob = 'B';
}

PROC void Proc_5(void) {
  Ch_1_Glob = 'A';
  Bool_Glob = false;
}

PROC void Proc_8(int64_t Arr_1_Par_Ref[50], int64_t Arr_2_Par_Ref[50][50], int64_t Int_1_Par_Val,
                 int64_t Int_2_Par_Val) {
  int64_t Int_Loc = Int_1_Par_Val + 5;
  Arr_1_Par_Ref[Int_Loc] = Int_2_Par_Val;
  Arr_1_Par_Ref[Int_Loc + 1] = Arr_1_Par_Ref[Int_Loc];
  Arr_1_Par_Ref[Int_Loc + 30] = Int_Loc;
  for (int64_t Int_Index = Int_Loc; Int_Index <= Int_Loc + 1; ++Int_Index) Arr_2_Par_Ref[Int_Loc][Int_Index] = Int_Loc;
  Arr_2_Par_Ref[Int_Loc][Int_Loc - 1] += 1;
  Arr_2_Par_Ref[Int_Loc + 20][Int_Loc] = Arr_1_Par_Ref[Int_Loc];
  Int_Glob = 5;
}

PROC bool Func_2(const char *Str_1_Par_Ref, const char *Str_2_Par_Ref) {
  int64_t Int_Loc = 2;
  char Ch_Loc = 0;
  while (Int_Loc <= 2)
    if (Func_1(Str_1_Par_Ref[Int_Loc], Str_2_Par_Ref[Int_Loc + 1]) == Ident_1) {
      Ch_Loc = 'A';
      Int_Loc += 1;
    }
  if (Ch_Loc >= 'W' && Ch_Loc < 'Z') Int_Loc = 7;
  if (Ch_Loc == 'R') return true;
  if (strcmp(Str_1_Par_Ref, Str_2_Par_Ref) > 0) {
    Int_Loc += 7;
    Int_Glob = Int_Loc;
    return true;
  }
  return false;
}

int main(void) {
  long long t = bench_ms();
  int64_t Int_1_Loc = 0, Int_2_Loc = 0, Int_3_Loc = 0;
  Enumeration Enum_Loc = Ident_1;
  char Str_1_Loc[31], Str_2_Loc[31];
  Next_Ptr_Glob = &Next_Glob_Rec;
  Ptr_Glob = &Glob_Rec;
  Ptr_Glob->Ptr_Comp = Next_Ptr_Glob;
  Ptr_Glob->Discr = Ident_1;
  Ptr_Glob->Enum_Comp = Ident_3;
  Ptr_Glob->Int_Comp = 40;
  strcpy(Ptr_Glob->Str_Comp, "DHRYSTONE PROGRAM, SOME STRING");
  strcpy(Str_1_Loc, "DHRYSTONE PROGRAM, 1'ST STRING");
  Arr_2_Glob[8][7] = 10;
  for (int64_t Run_Index = 1; Run_Index <= RUNS; ++Run_Index) {
    Proc_5();
    Proc_4();
    Int_1_Loc = 2;
    Int_2_Loc = 3;
    strcpy(Str_2_Loc, "DHRYSTONE PROGRAM, 2'ND STRING");
    Enum_Loc = Ident_2;
    Bool_Glob = !Func_2(Str_1_Loc, Str_2_Loc);
    while (Int_1_Loc < Int_2_Loc) {
      Int_3_Loc = 5 * Int_1_Loc - Int_2_Loc;
      Proc_7(Int_1_Loc, Int_2_Loc, &Int_3_Loc);
      Int_1_Loc += 1;
    }
    Proc_8(Arr_1_Glob, Arr_2_Glob, Int_1_Loc, Int_3_Loc);
    Proc_1(Ptr_Glob);
    for (char Ch_Index = 'A'; Ch_Index <= Ch_2_Glob; ++Ch_Index) {
      if (Enum_Loc == Func_1(Ch_Index, 'C')) {
        Proc_6(Ident_1, &Enum_Loc);
        strcpy(Str_2_Loc, "DHRYSTONE PROGRAM, 3'RD STRING");
        Int_2_Loc = Run_Index;
        Int_Glob = Run_Index;
      }
    }
    Int_2_Loc = Int_2_Loc * Int_1_Loc;
    Int_1_Loc = Int_2_Loc / Int_3_Loc;
    Int_2_Loc = 7 * (Int_2_Loc - Int_3_Loc) - Int_1_Loc;
    Proc_2(&Int_1_Loc);
  }
  const char *some = "DHRYSTONE PROGRAM, SOME STRING";
  int ok = Int_Glob == 5 && Bool_Glob && Ch_1_Glob == 'A' && Ch_2_Glob == 'B' && Arr_1_Glob[8] == 7 &&
           Arr_2_Glob[8][7] == RUNS + 10 && Ptr_Glob->Ptr_Comp == Next_Ptr_Glob && Ptr_Glob->Discr == Ident_1 &&
           Ptr_Glob->Enum_Comp == Ident_3 && Ptr_Glob->Int_Comp == 17 && strcmp(Ptr_Glob->Str_Comp, some) == 0 &&
           Next_Ptr_Glob->Ptr_Comp == Next_Ptr_Glob && Next_Ptr_Glob->Discr == Ident_1 &&
           Next_Ptr_Glob->Enum_Comp == Ident_2 && Next_Ptr_Glob->Int_Comp == 18 &&
           strcmp(Next_Ptr_Glob->Str_Comp, some) == 0 && Int_1_Loc == 5 && Int_2_Loc == 13 && Int_3_Loc == 7 &&
           Enum_Loc == Ident_2 && strcmp(Str_1_Loc, "DHRYSTONE PROGRAM, 1'ST STRING") == 0 &&
           strcmp(Str_2_Loc, "DHRYSTONE PROGRAM, 2'ND STRING") == 0;
  bench_report("dhrystone", t, ok);
  return 0;
}
