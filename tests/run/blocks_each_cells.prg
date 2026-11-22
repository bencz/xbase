// Codeblocks that capture FOR EACH variables, kept and evaluated later: each block gets cells of its own holding the
// elements of its time, made while the block is a root (a collection while one cell is made must not free another the
// block holds already). Two variables per block, many blocks, enough garbage between them for several collections.
PROCEDURE Main()
   LOCAL a := {}, c := {}, i, x, y, b, bad := 0, aB := {}, aV := {}
   FOR i := 1 TO 2000
      AAdd( a, Replicate( "x", 20 ) + Str( i ) )
      AAdd( c, i )
   NEXT
   FOR i := 1 TO 60
      FOR EACH x, y IN a, c
         b := {|| x + Str( y ) }
         AAdd( aB, b )
         AAdd( aV, x + Str( y ) )
      NEXT
   NEXT
   FOR i := 1 TO Len( aB )
      IF !( Eval( aB[ i ] ) == aV[ i ] )
         bad++
      ENDIF
   NEXT
   ? Len( aB ), bad
   RETURN
