// Binary search written in xBase: 1,000,000 keys from a fixed-seed generator looked for in a sorted array of
// 1,000,000 numbers (every third number), about a third of them found.
STATIC s_nSeed := 4711

PROCEDURE Main()
   LOCAL t, a := Array( 1000000 ), i, k, nLo, nHi, nMid, nFound := 0, nPosSum := 0, ok
   FOR i := 1 TO Len( a )
      a[ i ] := i * 3
   NEXT
   t := ElapsedMs()
   FOR i := 1 TO 1000000
      k := Rand( 3000010 )
      nLo := 1
      nHi := Len( a )
      DO WHILE nLo <= nHi
         nMid := Int( ( nLo + nHi ) / 2 )
         IF a[ nMid ] < k
            nLo := nMid + 1
         ELSEIF a[ nMid ] > k
            nHi := nMid - 1
         ELSE
            nFound++
            nPosSum += nMid
            EXIT
         ENDIF
      ENDDO
   NEXT
   ok := nFound == 333181 .AND. nPosSum == 166512579113
   ? "binary_search", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
