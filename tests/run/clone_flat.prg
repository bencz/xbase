// Deep copies of arrays and hashes that hold no array or hash (copied whole, without following anything) next to ones
// that do: each copy independent of its source, of every element type and array form, with holes in a hash, and the
// copies a class makes for each new object from INIT {} and INIT {=>}.
PROCEDURE Main()
   LOCAL a, b, h, g, o1, o2, i, x

   a := { 1, 2.5, "s", .T., NIL, SToD( "20240229" ), {|| 7 }, TBox():New() }
   b := AClone( a )
   b[ 1 ] := 99
   b[ 3 ] := "t"
   ? Len( b ), a[ 1 ], b[ 1 ], a[ 3 ], b[ 3 ], a[ 2 ], b[ 6 ], Eval( b[ 7 ] ), b[ 8 ] == a[ 8 ]

   a := Array( 5 )
   AFill( a, 3 )        // integers: a compact form
   b := AClone( a )
   b[ 2 ] := "x"
   ? a[ 2 ], b[ 2 ], b[ 5 ]
   a := { .T., .F., .T. } // logicals
   b := AClone( a )
   b[ 1 ] := .F.
   ? a[ 1 ], b[ 1 ], b[ 3 ]
   a := {}
   b := AClone( a )
   AAdd( b, 1 )
   ? Len( a ), Len( b )

   a := { 1, { 2, 3 }, 4 } // nested: the inner array copied too
   b := AClone( a )
   b[ 2, 1 ] := 20
   ? a[ 2, 1 ], b[ 2, 1 ]

   h := { "a" => 1, "b" => "two", 3 => 3.5 }
   g := HClone( h )
   g[ "a" ] := 10
   g[ "new" ] := .T.
   ? Len( h ), Len( g ), h[ "a" ], g[ "a" ], g[ "b" ], g[ 3 ]
   HDel( h, "b" ) // a hole
   g := HClone( h )
   ? Len( g ), HHasKey( g, "b" ), g[ "a" ], g[ 3 ]
   h := { => }
   g := HClone( h )
   g[ 1 ] := 1
   ? Len( h ), Len( g )
   h := { "in" => { 1, 2 } } // nested
   g := HClone( h )
   g[ "in", 1 ] := 100
   ? h[ "in", 1 ], g[ "in", 1 ]

   o1 := TBox():New()
   o2 := TBox():New()
   AAdd( o1:aItems, "only in o1" )
   o1:hMap[ "k" ] := 1
   o2:aNested[ 1, 1 ] := "changed"
   ? Len( o1:aItems ), Len( o2:aItems ), Len( o1:hMap ), Len( o2:hMap ), o1:aNested[ 1, 1 ], o2:aNested[ 1, 1 ]
   x := 0
   FOR i := 1 TO 1000
      o1 := TBox():New()
      AAdd( o1:aItems, i )
      x += Len( o1:aItems ) + Len( o1:aStart )
   NEXT
   ? x, Len( TBox():New():aItems )
   RETURN

CLASS TBox
   VAR aItems INIT {}
   VAR aStart INIT { 1, "two", 3 }
   VAR hMap INIT { => }
   VAR aNested INIT { { "x" } }
ENDCLASS
