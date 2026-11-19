// Map, filter and reduce written in xBase over arrays with codeblocks: an array of 1,000,000 numbers mapped, the
// result filtered and reduced to a sum; 8 rounds with other blocks.
PROCEDURE Main()
   LOCAL t, a := Array( 1000000 ), i, r, n := 0, ok
   FOR i := 1 TO Len( a )
      a[ i ] := i % 1000
   NEXT
   t := ElapsedMs()
   FOR r := 1 TO 8
      n += Reduce( Filter( Map( a, {| x | x * r + 1 } ), {| x | x % 3 == 0 } ), {| nAcc, x | nAcc + x }, 0 )
   NEXT
   ok := n == 4492503000
   ? "map_filter_reduce", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Map( a, b )
   LOCAL aOut := Array( Len( a ) ), i
   FOR i := 1 TO Len( a )
      aOut[ i ] := Eval( b, a[ i ] )
   NEXT
   RETURN aOut

STATIC FUNCTION Filter( a, b )
   LOCAL aOut := {}, x
   FOR EACH x IN a
      IF Eval( b, x )
         AAdd( aOut, x )
      ENDIF
   NEXT
   RETURN aOut

STATIC FUNCTION Reduce( a, b, xAcc )
   LOCAL x
   FOR EACH x IN a
      xAcc := Eval( b, xAcc, x )
   NEXT
   RETURN xAcc
