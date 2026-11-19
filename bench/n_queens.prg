// Backtracking over arrays of logicals: the solutions of the 12 queens problem (taken columns and diagonals), 3 times.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 12, rep, ok := .T.
   FOR rep := 1 TO 3
      ok := ok .AND. Place( 1, n, AFill( Array( n ), .F. ), AFill( Array( 2 * n - 1 ), .F. ), ;
         AFill( Array( 2 * n - 1 ), .F. ) ) == 14200
   NEXT
   ? "n_queens", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

// The solutions with queens on rows 1 to nRow - 1 as aCol and the diagonals aD1 and aD2 say.
STATIC FUNCTION Place( nRow, n, aCol, aD1, aD2 )
   LOCAL c, nCount := 0
   IF nRow > n
      RETURN 1
   ENDIF
   FOR c := 1 TO n
      IF ! aCol[ c ] .AND. ! aD1[ nRow + c - 1 ] .AND. ! aD2[ nRow - c + n ]
         aCol[ c ] := .T.
         aD1[ nRow + c - 1 ] := .T.
         aD2[ nRow - c + n ] := .T.
         nCount += Place( nRow + 1, n, aCol, aD1, aD2 )
         aCol[ c ] := .F.
         aD1[ nRow + c - 1 ] := .F.
         aD2[ nRow - c + n ] := .F.
      ENDIF
   NEXT
   RETURN nCount
