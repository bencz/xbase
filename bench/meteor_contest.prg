// Bit masks and a recursive search: every way to fill the meteor contest's board (5 x 10 hexagonal cells, odd rows
// shifted half a cell right) with its 10 pieces of 5 cells. The pieces are read from a solution; their placements (12
// orientations, every position, duplicates dropped) are 64-bit masks listed by piece and lowest cell. The search fills
// the lowest empty cell first and drops a placement that leaves an empty cell walled in among the next 10. The board
// turned half a turn is the same board, with cell i at 49 - i: piece 3, which no half turn leaves the same, takes only
// 6 of its orientations, one of each such pair, and every solution found counts for itself and its turned image (the
// reversed string). Checked: 2098 solutions, and the smallest and largest as strings of 50 digits.
#define COLS 5
#define ROWS 10
#define CELLS 50
#define HALVED 3
#define WINDOW 10

STATIC s_aMask, s_aCells, s_aBy, s_aNeighbors, s_aPlaced, s_nSolutions := 0, s_cBest, s_cWorst

PROCEDURE Main()
   LOCAL t := ElapsedMs(), ok
   MakePlacements( "99998966856688568255777257472014220144031400311333" )
   s_aPlaced := AFill( Array( 10 ), 0 )
   Solve( 0, 0, 0 )
   ok := s_nSolutions == 2098 .AND. s_cBest == "00001222012661126155865558633348893448934747977799" .AND. ;
      s_cWorst == "99998966856688568255777257472014220144031400311333"
   ? "meteor_contest", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

// Offset coordinates (odd rows shifted right) of a cell (from 0) to axial ones { q, r }, and back; -1 off the board.
STATIC FUNCTION ToAxial( nCell )
   LOCAL nRow := Int( nCell / COLS )
   RETURN { nCell - nRow * COLS - ( nRow - ( nRow & 1 ) ) / 2, nRow }

STATIC FUNCTION ToCell( q, r )
   LOCAL nCol := q + ( r - ( r & 1 ) ) / 2
   IF r < 0 .OR. r >= ROWS .OR. nCol < 0 .OR. nCol >= COLS
      RETURN -1
   ENDIF
   RETURN r * COLS + nCol

STATIC PROCEDURE MakePlacements( cShape )
   LOCAL aDq := { 1, -1, 0, 0, 1, -1 }, aDr := { 0, 0, 1, -1, -1, 1 }
   LOCAL p, c, o, nAt, aPts, aAt, aPlace, nMask, nLow, i, x, nQ, lOk, hSeen
   s_aNeighbors := AFill( Array( CELLS ), 0 )
   FOR c := 0 TO CELLS - 1
      aAt := ToAxial( c )
      FOR i := 1 TO 6
         x := ToCell( aAt[ 1 ] + aDq[ i ], aAt[ 2 ] + aDr[ i ] )
         IF x >= 0
            s_aNeighbors[ c + 1 ] := s_aNeighbors[ c + 1 ] | ( 1 << x )
         ENDIF
      NEXT
   NEXT
   s_aMask := Array( 10 )
   s_aCells := Array( 10 )
   s_aBy := Array( 10 )
   FOR p := 1 TO 10
      s_aMask[ p ] := {}
      s_aCells[ p ] := {}
      s_aBy[ p ] := Array( CELLS )
      FOR c := 1 TO CELLS
         s_aBy[ p ][ c ] := {}
      NEXT
      aPts := {}
      FOR c := 0 TO CELLS - 1
         IF Asc( SubStr( cShape, c + 1, 1 ) ) - 48 == p - 1
            AAdd( aPts, ToAxial( c ) )
         ENDIF
      NEXT
      hSeen := { => }
      FOR o := 0 TO 11
         FOR nAt := 0 TO CELLS - 1
            IF p - 1 == HALVED .AND. o % 6 >= 3
               EXIT
            ENDIF
            aAt := ToAxial( nAt )
            aPlace := Array( 5 )
            lOk := .T.
            FOR i := 1 TO 5
               x := ToCell( aPts[ i ][ 1 ] - aPts[ 1 ][ 1 ] + aAt[ 1 ], aPts[ i ][ 2 ] - aPts[ 1 ][ 2 ] + aAt[ 2 ] )
               aPlace[ i ] := x
               IF x < 0
                  lOk := .F.
                  EXIT
               ENDIF
            NEXT
            IF lOk
               nMask := 0
               nLow := CELLS
               FOR i := 1 TO 5
                  nMask := nMask | ( 1 << aPlace[ i ] )
                  nLow := Min( nLow, aPlace[ i ] )
               NEXT
               IF ! nMask $ hSeen
                  hSeen[ nMask ] := .T.
                  AAdd( s_aMask[ p ], nMask )
                  AAdd( s_aCells[ p ], aPlace )
                  AAdd( s_aBy[ p ][ nLow + 1 ], Len( s_aMask[ p ] ) )
               ENDIF
            ENDIF
         NEXT
         // The next orientation: a turn of 60 degrees, and after the sixth a mirror image.
         FOR i := 1 TO 5
            nQ := aPts[ i ][ 1 ]
            IF o == 5
               aPts[ i ][ 1 ] := aPts[ i ][ 2 ]
               aPts[ i ][ 2 ] := nQ
            ELSE
               aPts[ i ][ 1 ] := -aPts[ i ][ 2 ]
               aPts[ i ][ 2 ] := nQ + aPts[ i ][ 2 ]
            ENDIF
         NEXT
      NEXT
   NEXT
   RETURN

// Whether an empty cell among the WINDOW from nCell has no empty neighbor.
STATIC FUNCTION WalledIn( nBoard, nCell )
   LOCAL c
   FOR c := nCell TO Min( nCell + WINDOW, CELLS ) - 1
      IF ( ( nBoard >> c ) & 1 ) == 0 .AND. ( nBoard & s_aNeighbors[ c + 1 ] ) == s_aNeighbors[ c + 1 ]
         RETURN .T.
      ENDIF
   NEXT
   RETURN .F.

STATIC PROCEDURE Record( cSol )
   IF s_nSolutions == 0 .OR. cSol < s_cBest
      s_cBest := cSol
   ENDIF
   IF s_nSolutions == 0 .OR. cSol > s_cWorst
      s_cWorst := cSol
   ENDIF
   s_nSolutions++
   RETURN

STATIC PROCEDURE Solve( nBoard, nCell, nDepth )
   LOCAL p, k, nNext, cSol, cTurned, i
   DO WHILE ( ( nBoard >> nCell ) & 1 ) == 1
      nCell++
   ENDDO
   IF nDepth == 10
      cSol := Replicate( " ", CELLS )
      FOR p := 1 TO 10
         FOR EACH i IN s_aCells[ p ][ s_aPlaced[ p ] ]
            cSol := Stuff( cSol, i + 1, 1, Chr( 47 + p ) )
         NEXT
      NEXT
      cTurned := ""
      FOR i := CELLS TO 1 STEP -1
         cTurned += SubStr( cSol, i, 1 )
      NEXT
      Record( cSol )
      Record( cTurned )
      RETURN
   ENDIF
   FOR p := 1 TO 10
      IF s_aPlaced[ p ] == 0
         FOR EACH k IN s_aBy[ p ][ nCell + 1 ]
            nNext := nBoard | s_aMask[ p ][ k ]
            IF ( nBoard & s_aMask[ p ][ k ] ) == 0 .AND. ! WalledIn( nNext, nCell )
               s_aPlaced[ p ] := k
               Solve( nNext, nCell, nDepth + 1 )
            ENDIF
         NEXT
         s_aPlaced[ p ] := 0
      ENDIF
   NEXT
   RETURN
