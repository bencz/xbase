// Small integer arrays permuted and reversed in place: the pancake flips of every permutation of 9 elements (the
// checksum and the most flips), 4 times.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 9, aPerm, aPerm1, aCount, nChecksum, nMaxFlips, nPermCount, nFlips, r, i, j, k, x
   LOCAL nPerm0, rep, ok := .T., lDone
   FOR rep := 1 TO 4
      aPerm := Array( n )
      aPerm1 := Array( n )
      aCount := Array( n )
      FOR i := 1 TO n
         aPerm1[ i ] := i - 1
      NEXT
      nChecksum := 0
      nMaxFlips := 0
      nPermCount := 0
      r := n
      lDone := .F.
      DO WHILE ! lDone
         DO WHILE r != 1
            aCount[ r ] := r
            r--
         ENDDO
         FOR i := 1 TO n
            aPerm[ i ] := aPerm1[ i ]
         NEXT
         nFlips := 0
         k := aPerm[ 1 ]
         DO WHILE k != 0
            i := 1
            j := k + 1
            DO WHILE i < j
               x := aPerm[ i ]
               aPerm[ i ] := aPerm[ j ]
               aPerm[ j ] := x
               i++
               j--
            ENDDO
            nFlips++
            k := aPerm[ 1 ]
         ENDDO
         nMaxFlips := Max( nMaxFlips, nFlips )
         nChecksum += IIf( ( nPermCount & 1 ) == 0, nFlips, -nFlips )
         DO WHILE .T.
            IF r == n
               lDone := .T.
               EXIT
            ENDIF
            nPerm0 := aPerm1[ 1 ]
            FOR i := 1 TO r
               aPerm1[ i ] := aPerm1[ i + 1 ]
            NEXT
            aPerm1[ r + 1 ] := nPerm0
            aCount[ r + 1 ]--
            IF aCount[ r + 1 ] > 0
               EXIT
            ENDIF
            r++
         ENDDO
         nPermCount++
      ENDDO
      ok := ok .AND. nChecksum == 8629 .AND. nMaxFlips == 30
   NEXT
   ? "fannkuch_redux", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN
