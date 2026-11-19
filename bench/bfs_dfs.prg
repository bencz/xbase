// Breadth-first and depth-first search on an undirected graph of 50,000 vertices and 150,000 random edges (from a
// linear congruential generator) kept as arrays of adjacency: BFS from 6 sources (the sum of the levels and of the
// vertices reached), then an iterative DFS with an explicit stack over every component (the number of components and
// a checksum of the visiting order); 2 times.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), nV := 50000, nE := 150000, eu := Array( nE ), ev := Array( nE ), first, fill, adj, seed
   LOCAL i, d, rep, src, total := 0
   seed := 3
   first := AFill( Array( nV + 1 ), 0 )
   FOR i := 1 TO nE
      seed := Int( seed * 16807 % 2147483647 )
      eu[ i ] := Int( Int( seed / 16 ) % nV ) + 1
      seed := Int( seed * 16807 % 2147483647 )
      ev[ i ] := Int( Int( seed / 16 ) % nV ) + 1
      first[ eu[ i ] ]++
      first[ ev[ i ] ]++
   NEXT
   // degrees to the first slot of each vertex; first[ nV + 1 ] ends the last one
   fill := 1
   FOR i := 1 TO nV + 1
      d := first[ i ]
      first[ i ] := fill
      fill += d
   NEXT
   fill := AClone( first )
   adj := Array( 2 * nE )
   FOR i := 1 TO nE
      adj[ fill[ eu[ i ] ]++ ] := ev[ i ]
      adj[ fill[ ev[ i ] ]++ ] := eu[ i ]
   NEXT
   FOR rep := 1 TO 2
      FOR src := 1 TO 6
         total += Bfs( ( src - 1 ) * 8000 + 1, nV, first, adj )
      NEXT
      total += Dfs( nV, first, adj )
   NEXT
   ? "bfs_dfs", ElapsedMs() - t, IIf( total == 62828511998050, "ok", "WRONG" )
   RETURN

// The sum of the levels of the vertices reached from src, plus their number.
STATIC FUNCTION Bfs( src, nV, first, adj )
   LOCAL level := AFill( Array( nV ), -1 ), queue := Array( nV ), head := 1, tail := 1, u, v, e, d, sum := 0
   level[ src ] := 0
   queue[ 1 ] := src
   DO WHILE head <= tail
      u := queue[ head++ ]
      d := level[ u ]
      sum += d + 1
      FOR e := first[ u ] TO first[ u + 1 ] - 1
         v := adj[ e ]
         IF level[ v ] < 0
            level[ v ] := d + 1
            queue[ ++tail ] := v
         ENDIF
      NEXT
   ENDDO
   RETURN sum

// The number of components times 1,000,000,000 plus the sum of each vertex times its place in the visiting order.
STATIC FUNCTION Dfs( nV, first, adj )
   LOCAL seen := AFill( Array( nV ), .F. ), stack := Array( 2 * Len( adj ) + nV ), top, s, u, e, order := 0
   LOCAL comps := 0, sum := 0
   FOR s := 1 TO nV
      IF seen[ s ]
         LOOP
      ENDIF
      comps++
      top := 1
      stack[ 1 ] := s
      DO WHILE top > 0
         u := stack[ top-- ]
         IF seen[ u ]
            LOOP
         ENDIF
         seen[ u ] := .T.
         order++
         sum += u * order
         FOR e := first[ u + 1 ] - 1 TO first[ u ] STEP -1
            IF ! seen[ adj[ e ] ]
               stack[ ++top ] := adj[ e ]
            ENDIF
         NEXT
      ENDDO
   NEXT
   RETURN comps * 1000000000 + sum
