// Dhrystone 2.1, its procedures and functions as the classic C program has them, 1,000,000 runs: records as arrays
// (a reference to the next record, the discriminant, an enumeration, an integer and a string), character and string
// assignments and comparisons, a two-dimensional array, calls with parameters by value and by reference (a result the
// original stores through a pointer into a record's field is returned instead). The values the original prints at the
// end are checked.
#define PTR_COMP   1
#define DISCR      2
#define ENUM_COMP  3
#define INT_COMP   4
#define STR_COMP   5
#define IDENT_1    0
#define IDENT_2    1
#define IDENT_3    2
#define IDENT_4    3
#define IDENT_5    4
#define RUNS       1000000

STATIC s_nIntGlob := 0, s_lBoolGlob := .F., s_cCh1Glob := " ", s_cCh2Glob := " ", s_aArr1Glob, s_aArr2Glob
STATIC s_aPtrGlob, s_aNextPtrGlob

PROCEDURE Main()
   LOCAL t := ElapsedMs(), nInt1Loc, nInt2Loc, nInt3Loc := 0, nEnumLoc, cStr1Loc, cStr2Loc, nRun, nCh, ok
   s_aArr1Glob := AFill( Array( 50 ), 0 )
   s_aArr2Glob := Array( 50, 50 )
   AEval( s_aArr2Glob, {| a | AFill( a, 0 ) } )
   s_aNextPtrGlob := Array( 5 )
   s_aPtrGlob := { s_aNextPtrGlob, IDENT_1, IDENT_3, 40, "DHRYSTONE PROGRAM, SOME STRING" }
   cStr1Loc := "DHRYSTONE PROGRAM, 1'ST STRING"
   s_aArr2Glob[ 9, 8 ] := 10
   FOR nRun := 1 TO RUNS
      Proc5()
      Proc4()
      nInt1Loc := 2
      nInt2Loc := 3
      cStr2Loc := "DHRYSTONE PROGRAM, 2'ND STRING"
      nEnumLoc := IDENT_2
      s_lBoolGlob := ! Func2( cStr1Loc, cStr2Loc )
      DO WHILE nInt1Loc < nInt2Loc
         nInt3Loc := 5 * nInt1Loc - nInt2Loc
         nInt3Loc := Proc7( nInt1Loc, nInt2Loc )
         nInt1Loc++
      ENDDO
      Proc8( s_aArr1Glob, s_aArr2Glob, nInt1Loc, nInt3Loc )
      Proc1( s_aPtrGlob )
      FOR nCh := Asc( "A" ) TO Asc( s_cCh2Glob )
         IF nEnumLoc == Func1( Chr( nCh ), "C" )
            nEnumLoc := Proc6( IDENT_1 )
            cStr2Loc := "DHRYSTONE PROGRAM, 3'RD STRING"
            nInt2Loc := nRun
            s_nIntGlob := nRun
         ENDIF
      NEXT
      nInt2Loc := nInt2Loc * nInt1Loc
      nInt1Loc := Int( nInt2Loc / nInt3Loc )
      nInt2Loc := 7 * ( nInt2Loc - nInt3Loc ) - nInt1Loc
      Proc2( @nInt1Loc )
   NEXT
   ok := s_nIntGlob == 5 .AND. s_lBoolGlob .AND. s_cCh1Glob == "A" .AND. s_cCh2Glob == "B" .AND. ;
         s_aArr1Glob[ 9 ] == 7 .AND. s_aArr2Glob[ 9, 8 ] == RUNS + 10 .AND. ;
         s_aPtrGlob[ PTR_COMP ] == s_aNextPtrGlob .AND. s_aPtrGlob[ DISCR ] == IDENT_1 .AND. ;
         s_aPtrGlob[ ENUM_COMP ] == IDENT_3 .AND. s_aPtrGlob[ INT_COMP ] == 17 .AND. ;
         s_aPtrGlob[ STR_COMP ] == "DHRYSTONE PROGRAM, SOME STRING" .AND. ;
         s_aNextPtrGlob[ PTR_COMP ] == s_aNextPtrGlob .AND. s_aNextPtrGlob[ DISCR ] == IDENT_1 .AND. ;
         s_aNextPtrGlob[ ENUM_COMP ] == IDENT_2 .AND. s_aNextPtrGlob[ INT_COMP ] == 18 .AND. ;
         s_aNextPtrGlob[ STR_COMP ] == "DHRYSTONE PROGRAM, SOME STRING" .AND. ;
         nInt1Loc == 5 .AND. nInt2Loc == 13 .AND. nInt3Loc == 7 .AND. nEnumLoc == IDENT_2 .AND. ;
         cStr1Loc == "DHRYSTONE PROGRAM, 1'ST STRING" .AND. cStr2Loc == "DHRYSTONE PROGRAM, 2'ND STRING"
   ? "dhrystone", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC PROCEDURE Proc1( aPtrValPar )
   LOCAL aNextRecord := aPtrValPar[ PTR_COMP ]
   ACopy( s_aPtrGlob, aPtrValPar[ PTR_COMP ] )
   aPtrValPar[ INT_COMP ] := 5
   aNextRecord[ INT_COMP ] := aPtrValPar[ INT_COMP ]
   aNextRecord[ PTR_COMP ] := aPtrValPar[ PTR_COMP ]
   aNextRecord[ PTR_COMP ] := Proc3( aNextRecord[ PTR_COMP ] )
   IF aNextRecord[ DISCR ] == IDENT_1
      aNextRecord[ INT_COMP ] := 6
      aNextRecord[ ENUM_COMP ] := Proc6( aPtrValPar[ ENUM_COMP ] )
      aNextRecord[ PTR_COMP ] := s_aPtrGlob[ PTR_COMP ]
      aNextRecord[ INT_COMP ] := Proc7( aNextRecord[ INT_COMP ], 10 )
   ELSE
      ACopy( aPtrValPar[ PTR_COMP ], aPtrValPar )
   ENDIF
   RETURN

STATIC PROCEDURE Proc2( nIntParRef )
   LOCAL nIntLoc := nIntParRef + 10, nEnumLoc
   DO WHILE .T.
      IF s_cCh1Glob == "A"
         nIntLoc--
         nIntParRef := nIntLoc - s_nIntGlob
         nEnumLoc := IDENT_1
      ENDIF
      IF nEnumLoc == IDENT_1
         EXIT
      ENDIF
   ENDDO
   RETURN

STATIC FUNCTION Proc3( aPtrRefPar )
   IF s_aPtrGlob != NIL
      aPtrRefPar := s_aPtrGlob[ PTR_COMP ]
   ENDIF
   s_aPtrGlob[ INT_COMP ] := Proc7( 10, s_nIntGlob )
   RETURN aPtrRefPar

STATIC PROCEDURE Proc4()
   LOCAL lBoolLoc := s_cCh1Glob == "A"
   s_lBoolGlob := lBoolLoc .OR. s_lBoolGlob
   s_cCh2Glob := "B"
   RETURN

STATIC PROCEDURE Proc5()
   s_cCh1Glob := "A"
   s_lBoolGlob := .F.
   RETURN

STATIC FUNCTION Proc6( nEnumValPar )
   LOCAL nEnumRefPar := nEnumValPar
   IF ! Func3( nEnumValPar )
      nEnumRefPar := IDENT_4
   ENDIF
   DO CASE
   CASE nEnumValPar == IDENT_1
      nEnumRefPar := IDENT_1
   CASE nEnumValPar == IDENT_2
      IF s_nIntGlob > 100
         nEnumRefPar := IDENT_1
      ELSE
         nEnumRefPar := IDENT_4
      ENDIF
   CASE nEnumValPar == IDENT_3
      nEnumRefPar := IDENT_2
   CASE nEnumValPar == IDENT_5
      nEnumRefPar := IDENT_3
   ENDCASE
   RETURN nEnumRefPar

STATIC FUNCTION Proc7( nInt1ParVal, nInt2ParVal )
   LOCAL nIntLoc := nInt1ParVal + 2
   RETURN nInt2ParVal + nIntLoc

// The arrays are indexed from 1 where the original's are from 0.
STATIC PROCEDURE Proc8( aArr1ParRef, aArr2ParRef, nInt1ParVal, nInt2ParVal )
   LOCAL nIntIndex, nIntLoc := nInt1ParVal + 5
   aArr1ParRef[ nIntLoc + 1 ] := nInt2ParVal
   aArr1ParRef[ nIntLoc + 2 ] := aArr1ParRef[ nIntLoc + 1 ]
   aArr1ParRef[ nIntLoc + 31 ] := nIntLoc
   FOR nIntIndex := nIntLoc TO nIntLoc + 1
      aArr2ParRef[ nIntLoc + 1, nIntIndex + 1 ] := nIntLoc
   NEXT
   aArr2ParRef[ nIntLoc + 1, nIntLoc ]++
   aArr2ParRef[ nIntLoc + 21, nIntLoc + 1 ] := aArr1ParRef[ nIntLoc + 1 ]
   s_nIntGlob := 5
   RETURN

STATIC FUNCTION Func1( cCh1ParVal, cCh2ParVal )
   LOCAL cCh1Loc := cCh1ParVal, cCh2Loc := cCh1Loc
   IF !( cCh2Loc == cCh2ParVal )
      RETURN IDENT_1
   ENDIF
   s_cCh1Glob := cCh1Loc
   RETURN IDENT_2

// The strings are indexed from 1 where the original's are from 0.
STATIC FUNCTION Func2( cStr1ParRef, cStr2ParRef )
   LOCAL nIntLoc := 2, cChLoc
   DO WHILE nIntLoc <= 2
      IF Func1( SubStr( cStr1ParRef, nIntLoc + 1, 1 ), SubStr( cStr2ParRef, nIntLoc + 2, 1 ) ) == IDENT_1
         cChLoc := "A"
         nIntLoc++
      ENDIF
   ENDDO
   IF cChLoc >= "W" .AND. cChLoc < "Z"
      nIntLoc := 7
   ENDIF
   IF cChLoc == "R"
      RETURN .T.
   ENDIF
   IF cStr1ParRef > cStr2ParRef
      nIntLoc += 7
      s_nIntGlob := nIntLoc
      RETURN .T.
   ENDIF
   RETURN .F.

STATIC FUNCTION Func3( nEnumParVal )
   LOCAL nEnumLoc := nEnumParVal
   RETURN nEnumLoc == IDENT_3
