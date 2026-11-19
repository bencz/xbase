// Recursion and arrays of logicals: five hard sudoku puzzles solved by backtracking (the first empty cell tries each
// digit its row, column and box allow), checked by a hash of the solved grids, 3 times.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), aPuzzles, aGrid, aRow, aCol, aBox, cPuzzle, nSum, lSolved, i, rep, ok := .T.
   aPuzzles := { ;
      "8..........36......7..9.2...5...7.......457.....1...3...1....68..85...1..9....4..", ;
      "6.....8.3.4.7.................5.4.7.3..2.....1.6.......2.....5.....8.6......1....", ;
      "48.3............71.2.......7.5....6....2..8.............1.76...3.....4......5....", ;
      "..53.....8......2..7..1.5..4....53...1..7...6..32...8..6.5....9..4....3......97..", ;
      "1....7.9..3..2...8..96..5....53..9...1..8...26....4...3......1..4......7..7...3.." }
   FOR rep := 1 TO 3
      nSum := 0
      FOR EACH cPuzzle IN aPuzzles
         aGrid := Array( 81 )
         aRow := AFill( Array( 81 ), .F. )
         aCol := AFill( Array( 81 ), .F. )
         aBox := AFill( Array( 81 ), .F. )
         FOR i := 1 TO 81
            aGrid[ i ] := IIf( SubStr( cPuzzle, i, 1 ) == ".", 0, Asc( SubStr( cPuzzle, i, 1 ) ) - Asc( "0" ) )
            IF aGrid[ i ] != 0
               Mark( i - 1, aGrid[ i ], .T., aRow, aCol, aBox )
            ENDIF
         NEXT
         lSolved := Solve( 0, aGrid, aRow, aCol, aBox )
         ok := ok .AND. lSolved
         FOR i := 1 TO 81
            nSum := ( nSum * 31 + aGrid[ i ] ) & 0xFFFFFFF
         NEXT
      NEXT
      ok := ok .AND. nSum == 252679381
   NEXT
   ? "sudoku_solver", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

// Marks digit v as used, or free, in the row, column and box of cell p (0 to 80).
STATIC PROCEDURE Mark( p, v, lOn, aRow, aCol, aBox )
   LOCAL r := Int( p / 9 ), c := p - Int( p / 9 ) * 9
   aRow[ r * 9 + v ] := lOn
   aCol[ c * 9 + v ] := lOn
   aBox[ ( Int( r / 3 ) * 3 + Int( c / 3 ) ) * 9 + v ] := lOn
   RETURN

// Fills the empty cells from cell p (0 to 80) on; .T. when the grid is solved.
STATIC FUNCTION Solve( p, aGrid, aRow, aCol, aBox )
   LOCAL r, c, b, v
   DO WHILE p < 81 .AND. aGrid[ p + 1 ] != 0
      p++
   ENDDO
   IF p == 81
      RETURN .T.
   ENDIF
   r := Int( p / 9 )
   c := p - r * 9
   b := Int( r / 3 ) * 3 + Int( c / 3 )
   FOR v := 1 TO 9
      IF aRow[ r * 9 + v ] .OR. aCol[ c * 9 + v ] .OR. aBox[ b * 9 + v ]
         LOOP
      ENDIF
      aGrid[ p + 1 ] := v
      Mark( p, v, .T., aRow, aCol, aBox )
      IF Solve( p + 1, aGrid, aRow, aCol, aBox )
         RETURN .T.
      ENDIF
      Mark( p, v, .F., aRow, aCol, aBox )
      aGrid[ p + 1 ] := 0
   NEXT
   RETURN .F.
