// SubStr() over a large text: 6,000,000 pieces of 1 to 20 bytes cut at spread positions of a 1,000,000-byte text,
// their first and last bytes read.
STATIC s_nSeed := 5

PROCEDURE Main()
   LOCAL t, c := "", i, s, n := 0, ok
   FOR i := 1 TO 1000000
      s := Chr( 33 + Rand( 90 ) )
      c += s
   NEXT
   t := ElapsedMs()
   FOR i := 1 TO 6000000
      s := SubStr( c, ( i * 7919 ) % 999970 + 1, i % 20 + 1 )
      n += Asc( s ) + Asc( Right( s, 1 ) ) + Len( s )
   NEXT
   ok := n == 994529563
   ? "substr_massivo", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
