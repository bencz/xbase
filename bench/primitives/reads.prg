// Reads repeated within an expression: an object's variable and an array's element read three times where one read
// would do, next to the same work with the value read once into a LOCAL, in nanoseconds per expression (N of them per
// measure, over an array of 1000 numbers); the difference is what merging the reads would save. reads.c does the same
// work in C, where the reads are loads the compiler keeps in registers, and the `primitives` target prints the two
// side by side.
#define N 4000000
#define M 1000
#xcommand MEASURE <name> DO <stmt> => ;
   t := ElapsedMs() ;; FOR k := 1 TO N / M ;; <stmt> ;; NEXT ;; Report( <name>, ElapsedMs() - t )

STATIC s_nSink := 0

PROCEDURE Main()
   LOCAL t, k, i, n := 0, a := Array( M ), o := Point():New( 3, 4 )
   FOR i := 1 TO M
      a[ i ] := i % 97
   NEXT
   MEASURE "member_thrice" DO n += MemberThrice( o )
   MEASURE "member_once" DO n += MemberOnce( o )
   MEASURE "element_thrice" DO n += ElementThrice( a )
   MEASURE "element_once" DO n += ElementOnce( a )
   Keep( n )
   ? "sink", s_nSink
RETURN

STATIC FUNCTION MemberThrice( o )
   LOCAL i, n := 0
   FOR i := 1 TO M
      n += o:x * o:x + o:y * o:x + i
   NEXT
RETURN n

STATIC FUNCTION MemberOnce( o )
   LOCAL i, n := 0, x
   FOR i := 1 TO M
      x := o:x
      n += x * x + o:y * x + i
   NEXT
RETURN n

STATIC FUNCTION ElementThrice( a )
   LOCAL i, n := 0
   FOR i := 1 TO M
      n += a[ i ] * a[ i ] + a[ 1 ] * a[ i ]
   NEXT
RETURN n

STATIC FUNCTION ElementOnce( a )
   LOCAL i, n := 0, x
   FOR i := 1 TO M
      x := a[ i ]
      n += x * x + a[ 1 ] * x
   NEXT
RETURN n

STATIC PROCEDURE Keep( x )
   s_nSink += x
RETURN

STATIC PROCEDURE Report( cName, nMs )
   ? PadR( cName, 18 ), Str( nMs * 1000000 / N, 8, 1 )
RETURN

CLASS Point
   VAR x
   VAR y
   METHOD New( x, y ) INLINE ::x := x, ::y := y, Self
ENDCLASS
