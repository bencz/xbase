// AAdd() done by compiled code in place when the array is local and has room, in the form it has: integers into an
// array of integers (NIL too; the integer that marks NIL in that form, a double and a number with its own width turn
// it into items), logicals into one of logicals, anything into one of items (strings, arrays, objects); growth past
// its room through the runtime; the value back; an array that is not an array variable (an element); new strings
// appended to an old array that alone holds them, then collections and allocations of strings of that size; an array
// other threads append to; anything but an array, an argument error.
PROCEDURE Main()
   LOCAL a, b, i, n, c, o, x := 5, aTasks, nMin := 1 << 63, bOld

   a := {}
   FOR i := 1 TO 40
      AAdd( a, i * 3 )
   NEXT
   ? "integers:", Len( a ), a[ 1 ], a[ 40 ], AAdd( a, NIL ), Len( a ), a[ 41 ]
   AAdd( a, nMin )
   ? "the mark of NIL:", Len( a ), a[ 42 ] == nMin, a[ 41 ], a[ 40 ]
   b := { 1, 2 }
   AAdd( b, 2.5 )
   AAdd( b, 7 )
   ? "a double:", b[ 2 ], b[ 3 ], b[ 4 ], Len( b )
   b := { 1, 2 }
   AAdd( b, 10.50 )
   ? "own width:", b[ 3 ], Str( b[ 3 ] ), Len( b )

   a := {}
   FOR i := 1 TO 20
      AAdd( a, i % 3 == 0 )
   NEXT
   AAdd( a, NIL )
   ? "logicals:", Len( a ), a[ 1 ], a[ 3 ], a[ 21 ], AAdd( a, .T. ), AAdd( a, 1 ), a[ 22 ], a[ 23 ]

   a := { "s" }
   FOR i := 1 TO 30
      AAdd( a, Replicate( "x", i ) )
   NEXT
   AAdd( a, { i } )
   AAdd( a, TBox():New( 9 ) )
   ? "items:", Len( a ), a[ 1 ], Len( a[ 31 ] ), a[ 32 ][ 1 ], a[ 33 ]:n, AAdd( a, @x ), a[ 34 ]
   x := 6
   ? "by reference:", a[ 34 ], x

   a := { { 1 }, {} }
   AAdd( a[ 2 ], "e" )
   AAdd( a[ 1 ], 2 )
   ? "an element:", Len( a[ 1 ] ), a[ 1 ][ 2 ], a[ 2 ][ 1 ]

   a := {}
   FOR i := 1 TO 3000
      AAdd( a, "old" )
   NEXT
   __GcCollect() // the array is old
   AppendNew( a )
   __GcCollect( .T. )
   FOR i := 1 TO 20000 // what a young collection freed is taken again: strings of the same size
      c := Replicate( Chr( 65 + i % 26 ), 40 ) + LTrim( Str( i % 90 + 10 ) )
   NEXT
   __GcCollect( .T. )
   __GcCollect()
   n := 0
   FOR i := 1 TO 100
      IF a[ 3000 + i ] == Replicate( "v", 40 ) + LTrim( Str( i + 9 ) )
         n++
      ENDIF
   NEXT
   ? "old array, new strings:", Len( a ), n, a[ 3000 ], Right( a[ 3001 ], 2 ), Right( a[ 3100 ], 3 )

   a := {}
   aTasks := {}
   FOR i := 1 TO 4
      AAdd( aTasks, TaskRun( {|| Appended( a, 500 ) } ) )
   NEXT
   n := 0
   FOR EACH o IN aTasks
      n += o:Result
   NEXT
   ? "shared:", n, Len( a )

   bOld := ErrorBlock( {| e | Break( e ) } )
   BEGIN SEQUENCE
      AAdd( "s", 1 )
      ? "no error"
   RECOVER USING o
      ? "not an array:", o:subCode, o:operation
   END SEQUENCE
   ErrorBlock( bOld )
RETURN

STATIC PROCEDURE AppendNew( a )
   LOCAL k
   FOR k := 1 TO 100
      AAdd( a, Replicate( "v", 40 ) + LTrim( Str( k + 9 ) ) )
   NEXT
RETURN

STATIC FUNCTION Appended( a, n )
   LOCAL i
   FOR i := 1 TO n
      AAdd( a, i )
   NEXT
RETURN n

CLASS TBox
   VAR n
   METHOD New( n ) INLINE ::n := n, Self
ENDCLASS
