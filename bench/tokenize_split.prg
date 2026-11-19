// Splitting lines into fields: 200,000 comma-separated lines of 6 fields cut with At() and SubStr() into arrays,
// the numeric fields read with Val(); 2 rounds.
STATIC s_nSeed := 64

PROCEDURE Main()
   LOCAL t, aLines := Array( 200000 ), i, r, a, c, n := 0, ok
   FOR i := 1 TO Len( aLines )
      aLines[ i ] := LTrim( Str( Rand( 100000 ) ) ) + ",name" + LTrim( Str( i ) ) + "," + ;
         LTrim( Str( Rand( 1000 ) ) ) + ",," + Replicate( "z", Rand( 12 ) ) + ",end"
   NEXT
   t := ElapsedMs()
   FOR r := 1 TO 2
      FOR EACH c IN aLines
         a := Split( c, "," )
         n += Val( a[ 1 ] ) + Val( a[ 3 ] ) + Len( a ) + Len( a[ 5 ] )
      NEXT
   NEXT
   ok := n == 20190509958
   ? "tokenize_split", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Split( c, cSep )
   LOCAL a := {}, n
   DO WHILE ( n := At( cSep, c ) ) > 0
      AAdd( a, Left( c, n - 1 ) )
      c := SubStr( c, n + 1 )
   ENDDO
   AAdd( a, c )
   RETURN a

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
