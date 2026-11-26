// The young budget (__GcMemory()[ 6 ]) against the live data (__GcMemory()[ 7 ]): 20,000 slots of small arrays (with a
// 600-byte string each) replaced at random 400,000 times, so that most of each young collection survives while the old
// slots it scans are few (the budget's floor for them, 128 bytes each, is far below the live data): the budget grows
// past its minimum (4 MB) and past half the live data, but never past the live data; then a string of 40,000,000 bytes
// made by appends beside small garbage: what survives is the string, a large object, which costs a young collection
// little, so the budget stays at its minimum. Every slot and the string are checked at the end.
STATIC s_nSeed := 41

PROCEDURE Main()
   LOCAL aSlots := Array( 20000 ), i, k, nBad := 0, nSum := 0, nWant := 0, aNow, aMem, nMost := 0, lWithin := .T.
   LOCAL nMin := 4 * 1024 * 1024, c, a, lHalf := .F.

   FOR i := 1 TO Len( aSlots )
      aSlots[ i ] := Made( i )
   NEXT
   __GcCollect()
   FOR i := 1 TO 400000
      k := Rand( Len( aSlots ) ) + 1
      aSlots[ k ] := Made( i )
      IF i % 2000 == 0
         aMem := __GcMemory()
         nMost := Max( nMost, aMem[ 6 ] )
         lWithin := lWithin .AND. aMem[ 6 ] <= Max( aMem[ 7 ], nMin )
         lHalf := lHalf .OR. aMem[ 6 ] > aMem[ 7 ] / 2
      ENDIF
   NEXT
   __GcCollect()
   FOR i := 1 TO Len( aSlots )
      aNow := aSlots[ i ]
      IF Len( aNow ) != 4 .OR. aNow[ 2 ] != "n" + LTrim( Str( aNow[ 1 ] ) ) .OR. aNow[ 3 ][ 1 ] != aNow[ 1 ] * 2 .OR. ;
         Len( aNow[ 4 ] ) != 600 .OR. Right( aNow[ 4 ], 1 ) != Chr( 65 + aNow[ 1 ] % 26 )
         nBad++
      ENDIF
      nSum += aNow[ 3 ][ 1 ]
      nWant += aNow[ 1 ] * 2
   NEXT
   ? "slots:", Len( aSlots ), "wrong:", nBad, "sums agree:", nSum == nWant
   ? "budget grew:", nMost > nMin, "past half the live data:", lHalf, "within it:", lWithin

   aSlots := NIL
   __GcCollect()
   c := ""
   nMost := 0
   FOR i := 1 TO 40000
      c += Replicate( Chr( 65 + i % 26 ), 1000 )
      FOR k := 1 TO 20
         a := { i, k }
      NEXT
      IF i % 200 == 0
         nMost := Max( nMost, __GcMemory()[ 6 ] )
      ENDIF
   NEXT
   ? "string:", Len( c ), SubStr( c, 1000, 2 ), Right( c, 1 ), "budget at its minimum:", nMost <= nMin
RETURN

STATIC FUNCTION Made( n )
RETURN { n, "n" + LTrim( Str( n ) ), { n * 2 }, Replicate( Chr( 65 + n % 26 ), 600 ) }

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
RETURN s_nSeed % n
