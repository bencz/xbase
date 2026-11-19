// bench/towers_of_hanoi.prg in Harbour (its bit functions where xbase has bit operators).
// Recursion and nested arrays: 20 disks moved between three pegs (arrays with a height each), every move checked to
// put a smaller disk on a larger one, 4 times.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), nDisks := 20, aPeg, aTop, aBad, nMoves, i, rep, ok := .T.
   FOR rep := 1 TO 4
      aPeg := Array( 3, nDisks )
      FOR i := 1 TO nDisks
         aPeg[ 1, i ] := nDisks - i + 1
      NEXT
      aTop := { nDisks, 0, 0 }
      aBad := { 0 }
      nMoves := Hanoi( nDisks, 1, 3, 2, aPeg, aTop, aBad )
      ok := ok .AND. nMoves == hb_bitShift( 1, nDisks ) - 1 .AND. aBad[ 1 ] == 0 .AND. aTop[ 3 ] == nDisks
      FOR i := 1 TO nDisks
         ok := ok .AND. aPeg[ 3, i ] == nDisks - i + 1
      NEXT
   NEXT
   ? "towers_of_hanoi", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

// Moves n disks from peg nFrom to peg nTo through nVia; returns the number of moves.
STATIC FUNCTION Hanoi( n, nFrom, nTo, nVia, aPeg, aTop, aBad )
   LOCAL nMoves := 0, d
   IF n > 0
      nMoves := Hanoi( n - 1, nFrom, nVia, nTo, aPeg, aTop, aBad )
      d := aPeg[ nFrom, aTop[ nFrom ] ]
      aTop[ nFrom ]--
      IF aTop[ nTo ] > 0 .AND. aPeg[ nTo, aTop[ nTo ] ] < d
         aBad[ 1 ]++
      ENDIF
      aTop[ nTo ]++
      aPeg[ nTo, aTop[ nTo ] ] := d
      nMoves += 1 + Hanoi( n - 1, nVia, nTo, nFrom, aPeg, aTop, aBad )
   ENDIF
   RETURN nMoves
