// Fork-join over an unbalanced tree (as the unbalanced tree search benchmark): the root and the next four levels have
// 8 children each, every node below has 5 children or none as its hash says (subtrees of very different sizes,
// 2,859,034 nodes in all), each node hashed 40 times. The four levels below the root are tasks (TaskRun), 4,680 of
// them; a task waits for its children's results in turn (one no worker has taken yet runs inline, as a fork-join's
// join), the levels below run inside the task, and idle workers steal the queued tasks. The node count and the sum of
// the hashes must be the tree's, whatever the scheduling.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), a := Branch( 19937, 0 )
   ? "work_stealing", ElapsedMs() - t, IIf( a[ 1 ] == 2859034 .AND. a[ 2 ] == 1428166988, "ok", "WRONG" )
RETURN

// { nodes, sum } of the subtree of seed s at depth d: its children as tasks down to depth 4, then walked in place.
STATIC FUNCTION Branch( s, d )
   LOCAL aTasks := {}, k, a, nCount := 0, nSum := 0, h
   IF d >= 4
      Visit( s, d, @nCount, @nSum )
      RETURN { nCount, nSum }
   ENDIF
   h := Hash( s )
   FOR k := 1 TO 8
      AAdd( aTasks, TaskRun( {| c, e | Branch( c, e ) }, ( s * 16807 + k * 48271 ) % 2147483647, d + 1 ) )
   NEXT
   FOR k := 8 TO 1 STEP -1
      a := aTasks[ k ]:Result
      nCount += a[ 1 ]
      nSum += a[ 2 ]
   NEXT
RETURN { nCount + 1, nSum + h % 1000 }

STATIC PROCEDURE Visit( s, d, nCount, nSum )
   LOCAL h := Hash( s ), k
   nCount++
   nSum += h % 1000
   IF d < 5 .OR. h % 1000 < 198
      FOR k := 1 TO IIf( d < 5, 8, 5 )
         Visit( ( s * 16807 + k * 48271 ) % 2147483647, d + 1, @nCount, @nSum )
      NEXT
   ENDIF
RETURN

STATIC FUNCTION Hash( s )
   LOCAL i
   FOR i := 1 TO 40
      s := s * 16807 % 2147483647
   NEXT
RETURN s
