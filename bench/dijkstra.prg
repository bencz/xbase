// Dijkstra's shortest paths on a directed graph of 20,000 vertices and 120,000 edges (each vertex linked to the
// next and to 5 others, weights 1 .. 100, from a linear congruential generator) kept as arrays of adjacency, with a
// binary heap of (distance, vertex) pairs written in xBase and stale entries skipped; from 12 sources, the sum of the
// distances checked.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), nV := 20000, nOut := 6, nE, first, aTo, aW, seed, i, k, e, src, total := 0
   nE := nV * nOut
   first := Array( nV + 1 )
   aTo := Array( nE )
   aW := Array( nE )
   seed := 7
   e := 0
   FOR i := 1 TO nV
      first[ i ] := e + 1
      e++
      aTo[ e ] := IIf( i == nV, 1, i + 1 )
      seed := Int( seed * 16807 % 2147483647 )
      aW[ e ] := Int( Int( seed / 256 ) % 100 ) + 1
      FOR k := 2 TO nOut
         e++
         seed := Int( seed * 16807 % 2147483647 )
         aTo[ e ] := Int( Int( seed / 16 ) % nV ) + 1
         seed := Int( seed * 16807 % 2147483647 )
         aW[ e ] := Int( Int( seed / 256 ) % 100 ) + 1
      NEXT
   NEXT
   first[ nV + 1 ] := e + 1
   FOR src := 1 TO 12
      total += Dijkstra( ( src - 1 ) * 1600 + 1, nV, first, aTo, aW )
   NEXT
   ? "dijkstra", ElapsedMs() - t, IIf( total == 41762116, "ok", "WRONG" )
   RETURN

// The sum of the shortest distances from vertex src to every vertex.
STATIC FUNCTION Dijkstra( src, nV, first, aTo, aW )
   LOCAL dist := AFill( Array( nV ), -1 ), done := AFill( Array( nV ), .F. ), hD := Array( Len( aTo ) + 1 )
   LOCAL hV := Array( Len( aTo ) + 1 ), nH := 0, d, u, v, e, nd, i, c, x, y, sum := 0
   dist[ src ] := 0
   nH := 1
   hD[ 1 ] := 0
   hV[ 1 ] := src
   DO WHILE nH > 0
      d := hD[ 1 ]
      u := hV[ 1 ]
      // pop: the last entry sifted down from the root
      x := hD[ nH ]
      y := hV[ nH ]
      nH--
      i := 1
      DO WHILE ( c := i * 2 ) <= nH
         IF c < nH .AND. hD[ c + 1 ] < hD[ c ]
            c++
         ENDIF
         IF hD[ c ] >= x
            EXIT
         ENDIF
         hD[ i ] := hD[ c ]
         hV[ i ] := hV[ c ]
         i := c
      ENDDO
      hD[ i ] := x
      hV[ i ] := y
      IF done[ u ]
         LOOP
      ENDIF
      done[ u ] := .T.
      sum += d
      FOR e := first[ u ] TO first[ u + 1 ] - 1
         v := aTo[ e ]
         nd := d + aW[ e ]
         IF ! done[ v ] .AND. ( dist[ v ] < 0 .OR. nd < dist[ v ] )
            dist[ v ] := nd
            // push: sifted up from a new leaf
            nH++
            i := nH
            DO WHILE i > 1 .AND. hD[ c := Int( i / 2 ) ] > nd
               hD[ i ] := hD[ c ]
               hV[ i ] := hV[ c ]
               i := c
            ENDDO
            hD[ i ] := nd
            hV[ i ] := v
         ENDIF
      NEXT
   ENDDO
   RETURN sum
