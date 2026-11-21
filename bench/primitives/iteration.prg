// Going through collections, element by element: FOR EACH over a string, an array, a hash, a generator, a lazy
// Enumerable sequence and an object with GetEnumerator(), the same done with an index, nested loops, and Enumerable's
// chains next to the loop that does their work; each sums its elements, in nanoseconds per element (N of them per
// measure, over collections of 1000). iteration.c does the same work in C (a generator as a state machine, a sequence
// as a function applied in a loop), and the `primitives` target prints the two side by side.
#define N 4000000
#define M 1000
#xcommand MEASURE <name> DO <stmt> => ;
   t := ElapsedMs() ;; FOR k := 1 TO N / M ;; <stmt> ;; NEXT ;; Report( <name>, ElapsedMs() - t )

STATIC s_nSink := 0

PROCEDURE Main()
   LOCAL t, k, i, n := 0, s := "", a := Array( M ), h := { => }, aRows := Array( M / 10 ), o := Items():New()
   FOR i := 1 TO M
      s += Chr( 48 + i % 64 )
      a[ i ] := i % 97
      h[ i ] := i % 89
   NEXT
   FOR i := 1 TO M / 10
      aRows[ i ] := Left( s, 10 )
   NEXT
   MEASURE "string_foreach" DO n += Bytes( s )
   MEASURE "string_index" DO n += BytesByIndex( s )
   MEASURE "array_foreach" DO n += Sum( a )
   MEASURE "array_index" DO n += SumByIndex( a )
   MEASURE "hash_foreach" DO n += Sum( h )
   MEASURE "generator" DO n += Sum( Numbers( M ) )
   MEASURE "object_enum" DO n += Sum( o )
   MEASURE "nested_string" DO n += Rows( aRows )
   MEASURE "select_sum" DO n += a:Select( {| v | v * 2 } ):Sum()
   MEASURE "select_loop" DO n += Doubled( a )
   MEASURE "where_count" DO n += a:Count( {| v | v % 3 == 0 } )
   MEASURE "where_loop" DO n += Thirds( a )
   MEASURE "string_where" DO n += s:Where( {| c | c > "Z" } ):Count()
   MEASURE "string_select" DO n += Sum( s:Select( {| c | Asc( c ) } ) )
   MEASURE "chain_3" DO n += a:Where( {| v | v % 2 == 0 } ):Select( {| v | v + 1 } ):Take( 400 ):Sum()
   Keep( n )
   ? "sink", s_nSink
RETURN

STATIC FUNCTION Bytes( s )
   LOCAL n := 0, c
   FOR EACH c IN s
      n += Asc( c )
   NEXT
RETURN n

STATIC FUNCTION BytesByIndex( s )
   LOCAL n := 0, i
   FOR i := 1 TO Len( s )
      n += Asc( SubStr( s, i, 1 ) )
   NEXT
RETURN n

STATIC FUNCTION Sum( x )
   LOCAL n := 0, v
   FOR EACH v IN x
      n += v
   NEXT
RETURN n

STATIC FUNCTION SumByIndex( a )
   LOCAL n := 0, i
   FOR i := 1 TO Len( a )
      n += a[ i ]
   NEXT
RETURN n

STATIC FUNCTION Rows( aRows )
   LOCAL n := 0, r, c
   FOR EACH r IN aRows
      FOR EACH c IN r
         n += Asc( c )
      NEXT
   NEXT
RETURN n

STATIC FUNCTION Doubled( a )
   LOCAL n := 0, v
   FOR EACH v IN a
      n += v * 2
   NEXT
RETURN n

STATIC FUNCTION Thirds( a )
   LOCAL n := 0, v
   FOR EACH v IN a
      IF v % 3 == 0
         ++n
      ENDIF
   NEXT
RETURN n

STATIC FUNCTION Numbers( nCount )
   LOCAL i
   FOR i := 1 TO nCount
      YIELD i % 97
   NEXT
RETURN

STATIC PROCEDURE Keep( x )
   s_nSink += x
RETURN

STATIC PROCEDURE Report( cName, nMs )
   ? PadR( cName, 18 ), Str( nMs * 1000000 / N, 8, 1 )
RETURN

CLASS Items
   METHOD New() INLINE Self
   METHOD GetEnumerator()
ENDCLASS

METHOD GetEnumerator() CLASS Items
   LOCAL i
   FOR i := 1 TO M
      YIELD i % 97
   NEXT
RETURN
