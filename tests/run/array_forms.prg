// The forms of an array's elements (xbrt.h): a new array of NILs takes the form of the first value stored in it
// (logicals, integers, or items for anything else), and turns into items, keeping every value, when a value its form
// does not hold is stored, in place or through the runtime, inside a loop or outside one. No program sees a form: every
// value reads back as it was stored.
STATIC s_aInts := { 1, 2, 3 }

PROCEDURE Main()
   LOCAL a, b, c, i, n, r, s, x, m
   MEMVAR p

   ? "-- new arrays"
   a := Array( 4 )
   ? a[ 1 ], Len( a ), ValType( a[ 4 ] )
   a[ 2 ] := .T.
   a[ 3 ] := .F.
   ? a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ]
   a[ 1 ] := 7                     // an integer into logicals: items
   ? a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ], ValType( a[ 1 ] )

   b := Array( 3 )
   b[ 1 ] := 10
   b[ 3 ] := -5
   ? b[ 1 ], b[ 2 ], b[ 3 ]
   b[ 2 ] := .T.                   // a logical into integers: items
   ? b[ 1 ], b[ 2 ], b[ 3 ]

   ? "-- values a compact form does not hold"
   b := { 1, 2, 3 }
   b[ 2 ] := -9223372036854775807 - 1   // the integers' own NIL mark, as a value
   ? b[ 1 ], b[ 2 ] == -9223372036854775807 - 1, b[ 3 ]
   b := { 1, 2, 3 }
   b[ 1 ] := Val( "123" )               // an integer with a width of its own
   ? b[ 1 ], Len( Str( b[ 1 ] ) ), b[ 2 ]
   b := { 1, 2, 3 }
   b[ 3 ] := 2.5
   ? b[ 1 ], b[ 2 ], b[ 3 ]
   b := { 1, 2, 3 }
   b[ 2 ] := "two"
   ? b[ 1 ], b[ 2 ], b[ 3 ]
   b := { .T., NIL, .F. }
   b[ 2 ] := Date() - Date()
   ? b[ 1 ], b[ 2 ], b[ 3 ]
   b := { 1, NIL, 3 }
   ? b[ 2 ], ValType( b[ 2 ] )
   b[ 1 ] := NIL
   ? b[ 1 ], b[ 3 ]

   ? "-- loops"
   ? Sieve( 1000 ), Sieve( 100000 )
   a := Array( 100 )
   FOR i := 1 TO 100
      a[ i ] := i * i
   NEXT
   s := 0
   FOR i := 1 TO 100
      s += a[ i ]
   NEXT
   ? s
   FOR i := 1 TO 100               // the form changes in the middle of the loop
      IF i == 50
         a[ 7 ] := "seven"
      ENDIF
      a[ i ] := IIf( i == 7, a[ i ], a[ i ] + 1 )
   NEXT
   ? a[ 1 ], a[ 7 ], a[ 49 ], a[ 50 ], a[ 100 ]
   a := Array( 10 )
   n := 0
   FOR i := 1 TO 10                // NILs read in the loop that fills the array
      IF a[ i ] == NIL
         n++
      ENDIF
      a[ i ] := i % 2 == 0
   NEXT
   ? n, a[ 1 ], a[ 2 ], a[ 10 ]
   n := 0
   FOR i := 1 TO 10
      IF ! a[ i ]
         n++
      ENDIF
   NEXT
   ? n
   a[ 5 ] := NIL
   n := 0
   FOR i := 1 TO 10
      IF a[ i ] == NIL
         n++
      ENDIF
   NEXT
   ? n, a[ 5 ], a[ 6 ]
   m := Array( 3, 4 )
   FOR i := 1 TO 3
      FOR n := 1 TO 4
         m[ i, n ] := i * 10 + n
      NEXT
   NEXT
   s := 0
   FOR n := 1 TO 4                 // a row read on every pass
      FOR i := 1 TO 3
         s += m[ i, n ]
      NEXT
   NEXT
   ? s, m[ 2, 3 ], m[ 3 ][ 4 ]
   m[ 2, 2 ] := .T.
   ? m[ 2, 1 ], m[ 2, 2 ], m[ 2, 3 ]
   ? Total( { 1, 2, 3, 4 } ), Total( { 1.5, 2, 3 } ), Total( { 2, 4, 2.25 } )

   ? "-- the array functions"
   a := { 5, 3, NIL, 9, 1 }
   ? AScan( a, 9 ), AScan( a, NIL ), AScan( a, {| e | e != NIL .AND. e > 4 } )
   ASort( a )
   ? a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ], a[ 5 ]
   a := { 5, 3, 8, 1 }
   ASort( a,,, {| p, q | p > q } )
   ? a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ]
   a := { 1, 2, 3 }
   AAdd( a, 4 )
   AAdd( a, NIL )
   ? Len( a ), a[ 4 ], a[ 5 ], ATail( a )
   AAdd( a, "five" )
   ? Len( a ), a[ 1 ], a[ 6 ]
   a := {}
   AAdd( a, .T. )
   AAdd( a, .F. )
   ? Len( a ), a[ 1 ], a[ 2 ]
   a := { 1, 2, 3, 4 }
   AIns( a, 2 )
   ? a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ]
   ADel( a, 1 )
   ? a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ]
   ASize( a, 6 )
   ? Len( a ), a[ 5 ], a[ 6 ], a[ 2 ]
   ASize( a, 2 )
   ? Len( a ), a[ 1 ], a[ 2 ]
   a := Array( 5 )
   AFill( a, 3 )
   ? a[ 1 ], a[ 5 ]
   AFill( a, .T., 2, 2 )
   ? a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ]
   a := { 1, 2, 3 }
   b := AClone( a )
   b[ 1 ] := 100
   ? a[ 1 ], b[ 1 ], b[ 3 ]
   c := Array( 5 )
   ACopy( { .T., .F., .T. }, c, 1, 3, 2 )
   ? c[ 1 ], c[ 2 ], c[ 3 ], c[ 4 ], c[ 5 ]
   r := 0
   AEval( { 4, 5, 6 }, {| e, k | r += e * k } )
   ? r
   ? Empty( Array( 0 ) ), Empty( {} ), Len( Array( 0 ) )

   ? "-- references, FOR EACH, macros"
   a := { 1, 2, 3 }
   Twice( @a[ 2 ] )
   ? a[ 1 ], a[ 2 ], a[ 3 ]
   Name( @a[ 3 ] )
   ? a[ 1 ], a[ 2 ], a[ 3 ]
   a := { .T., .F. }
   FOR EACH x IN a
      x := ! x
   NEXT
   ? a[ 1 ], a[ 2 ]
   a := { 10, 20, 30 }
   n := 3
   FOR EACH x IN a DESCEND
      x := x + n
      n--
   NEXT
   ? a[ 1 ], a[ 2 ], a[ 3 ]
   PRIVATE p := { 1, 2 }
   ? &( "p[ 2 ]" ), &( "p[ 1 ] + 40" )
   &( "p[ 1 ] := .T." )
   ? p[ 1 ], p[ 2 ]

   ? "-- other threads, collections"
   ? ThreadStart( {|| Sum( s_aInts ) } ):Join(), Sum( s_aInts )   // published as the second thread attaches
   s_aInts[ 2 ] := .T.
   ? s_aInts[ 1 ], s_aInts[ 2 ], s_aInts[ 3 ]
   b := { 4, 5, 6 }
   x := TaskRun( {|| Sum( b ) } )
   x:Wait()
   ? x:Result, b[ 3 ]
   c := Channel()
   ThreadStart( {|| c:TrySend( { 7, 8, NIL } ), c:TrySend( { .T., .F. } ) } ):Join()
   r := { NIL }
   c:TryReceive( r )
   ? r[ 1 ][ 1 ], r[ 1 ][ 2 ], r[ 1 ][ 3 ]
   r[ 1 ][ 3 ] := 9
   c:TryReceive( r )
   ? r[ 1 ][ 1 ], r[ 1 ][ 2 ]
   a := Array( 1000 )
   FOR i := 1 TO 1000
      a[ i ] := i
   NEXT
   ? ThreadStart( {|| Sum( a ) } ):Join(), Sum( a )   // shared now: items
   a[ 1 ] := .T.
   ? a[ 1 ], a[ 1000 ]
   a := Array( 300000 )
   FOR i := 1 TO 300000
      a[ i ] := i % 3 == 0
   NEXT
   b := {}
   FOR i := 1 TO 2000
      AAdd( b, { i, Replicate( "x", i % 50 ) } )
   NEXT
   __GcCollect()
   n := 0
   FOR i := 1 TO 300000
      IF a[ i ]
         n++
      ENDIF
   NEXT
   ? n, b[ 2000, 1 ], Len( b[ 1999, 2 ] )
   RETURN

FUNCTION Sieve( n )
   LOCAL a := Array( n ), i, j, count := 0
   FOR i := 2 TO n
      a[ i ] := .T.
   NEXT
   a[ 1 ] := .F.
   i := 2
   DO WHILE i * i <= n
      IF a[ i ]
         FOR j := i * i TO n STEP i
            a[ j ] := .F.
         NEXT
      ENDIF
      i++
   ENDDO
   FOR i := 1 TO n
      IF a[ i ]
         count++
      ENDIF
   NEXT
   RETURN count

FUNCTION Total( a )
   LOCAL s := 0, i
   FOR i := 1 TO Len( a )
      s := s + a[ i ]
   NEXT
   RETURN s

FUNCTION Sum( a )
   LOCAL s := 0, x
   FOR EACH x IN a
      s += x
   NEXT
   RETURN s

PROCEDURE Twice( v )
   v := v * 2
   RETURN

PROCEDURE Name( v )
   v := "three"
   RETURN
