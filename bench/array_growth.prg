// Many arrays growing and shrinking: 300,000 arrays grown with AAdd() to 1 to 64 elements, enlarged with ASize()
// to twice that, cut to half, an element inserted in front with AIns() and removed with ADel(); one in 100 kept to
// the end.
STATIC s_nSeed := 3

PROCEDURE Main()
   LOCAL t := ElapsedMs(), aKept := {}, a, i, j, nSize, n := 0, ok
   FOR i := 1 TO 300000
      a := {}
      nSize := Rand( 64 ) + 1
      FOR j := 1 TO nSize
         AAdd( a, j * 2 )
      NEXT
      ASize( a, nSize * 2 )
      a[ nSize * 2 ] := i % 1000
      n += a[ nSize * 2 ]
      ASize( a, Int( ( nSize + 1 ) / 2 ) )
      AIns( a, 1 )
      a[ 1 ] := nSize
      ADel( a, 1 )
      a[ Len( a ) ] := i % 7
      n += Len( a ) + a[ Len( a ) ] + IIf( Len( a ) > 1, a[ 1 ], 0 )
      IF i % 100 == 0
         AAdd( aKept, a )
      ENDIF
   NEXT
   FOR EACH a IN aKept
      n += a[ 1 ]
   NEXT
   ok := n == 156283982 .AND. Len( aKept ) == 3000
   ? "array_growth", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
