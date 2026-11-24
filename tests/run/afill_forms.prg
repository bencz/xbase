// AFill writes its range at once, the array taking first the form that holds the value: logicals, integers, strings and
// a mix into arrays of each form (an Array( n ) of NILs, a literal, one filled before), partial ranges and counts, then
// stores of other values (the array turns into items, the elements filled kept); an old array of items filled with
// new strings that it alone holds, then collections and allocations of strings of that size (the elements must survive
// them); an array other threads read while it is filled.
PROCEDURE Main()
   LOCAL a, b, i, n, c, aTasks, o

   a := AFill( Array( 6 ), .T. )
   ? "logicals:", a[ 1 ], a[ 6 ], ValType( a[ 3 ] )
   AFill( a, .F., 2, 3 )
   ? "partial:", a[ 1 ], a[ 2 ], a[ 4 ], a[ 5 ], a[ 6 ]
   a[ 6 ] := 7
   ? "then a number:", a[ 1 ], a[ 3 ], a[ 6 ]

   a := AFill( Array( 5 ), 3 )
   AFill( a, NIL, 4 )
   ? "integers:", a[ 1 ], a[ 3 ], a[ 4 ], a[ 5 ]
   AFill( a, 2.5, 2, 1 )
   ? "a double:", a[ 1 ], a[ 2 ], a[ 3 ]

   a := { 1, "x", .T. }
   AFill( a, "s", 2 )
   ? "items:", a[ 1 ], a[ 2 ], a[ 3 ]
   AFill( a, .T., -1 )
   ? "negative start:", a[ 1 ], a[ 2 ]

   a := Array( 20000 )
   AFill( a, "old" ) // items already: no change of form records the array
   __GcCollect() // the array is old
   FillNew( a ) // the strings are the old array's alone, whole lines of the heap
   b := AFill( Array( 3 ), { 9 } )
   __GcCollect( .T. )
   FOR i := 1 TO 20000 // what a young collection freed is taken again: strings of the same size
      c := Replicate( Chr( 65 + i % 26 ), 40 ) + LTrim( Str( i % 90 + 10 ) )
   NEXT
   __GcCollect( .T. )
   __GcCollect()
   n := 0
   FOR i := 100 TO 5099
      IF a[ i ] == Replicate( "v", 40 ) + LTrim( Str( Int( ( i - 100 ) / 50 ) + 10 ) )
         n++
      ENDIF
   NEXT
   ? "old array, new strings:", n, a[ 99 ], Right( a[ 100 ], 3 ), Right( a[ 5099 ], 3 ), a[ 5100 ], b[ 1 ] == b[ 3 ], ;
     b[ 2 ][ 1 ]

   a := AFill( Array( 1000 ), 1 )
   aTasks := {}
   FOR i := 1 TO 4
      AAdd( aTasks, TaskRun( {|| Summed( a ) } ) )
   NEXT
   AFill( a, 2 )
   n := 0
   FOR EACH o IN aTasks
      n += IIf( o:Result == 1000 .OR. o:Result == 2000 .OR. ( o:Result > 1000 .AND. o:Result < 2000 ), 1, 0 )
   NEXT
   ? "shared:", n, a[ 1 ], a[ 1000 ]
RETURN

STATIC PROCEDURE FillNew( a )
   LOCAL k
   FOR k := 0 TO 99
      AFill( a, Replicate( "v", 40 ) + LTrim( Str( k + 10 ) ), 100 + k * 50, 50 )
   NEXT
RETURN

STATIC FUNCTION Summed( a )
   LOCAL i, n := 0
   FOR i := 1 TO Len( a )
      n += a[ i ]
   NEXT
RETURN n
