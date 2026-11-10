// Len(), Asc(), Chr(), SubStr( c, n, 1 ) and Int() are done in place by compiled code when their arguments allow
// it, and Asc( SubStr( c, n, 1 ) ) reads the byte itself: the results must be the functions' own, on the fast paths
// and off them (other types, positions outside the string, counts other than 1, a double beyond 64 bits or NaN for
// Int()), errors included. One-byte strings are shared, whatever made them.
PROCEDURE Main()
   LOCAL s := "abc", e := "", a := { 1, 2, 3 }, h := { "x" => 1 }, n := 2, d := 2.0, one := 1, two := 2, i, k, t

   ? "len", Len( s ), Len( e ), Len( a ), Len( {} ), Len( h ), Len( Space( 1000 ) )
   ? "asc", Asc( s ), Asc( e ), Asc( "é" ), Asc( Chr( 255 ) ), Asc( Chr( 0 ) )
   ? "chr", Chr( 65 ) + Chr( 66.9 ) + Chr( 256 + 67 ) + Chr( -191 ), Len( Chr( 0 ) ), Asc( Chr( n + 64 ) )
   ? "substr", SubStr( s, 1, 1 ), SubStr( s, n, 1 ), SubStr( s, 3, one ), "[" + SubStr( s, 4, 1 ) + "]", ;
     "[" + SubStr( s, 0, 1 ) + "]", SubStr( s, -1, 1 ), SubStr( s, d, 1 ), SubStr( s, 2, two ), SubStr( s, 2 ), ;
     "[" + SubStr( e, 1, 1 ) + "]", SubStr( s, 2, 1.0 ), "[" + SubStr( s, 2, 0 ) + "]"
   ? "fused", Asc( SubStr( s, 1, 1 ) ), Asc( SubStr( s, n, 1 ) ), Asc( SubStr( s, 9, 1 ) ), Asc( SubStr( s, -1, 1 ) ), ;
     Asc( SubStr( s, 2, 2 ) ), Asc( SubStr( s, d, 1 ) ), Asc( SubStr( e, 1, 1 ) )
   t := 0
   FOR i := 1 TO Len( s )
      t := t * 256 + Asc( SubStr( s, i, 1 ) )
   NEXT
   k := ""
   FOR i := Len( s ) TO 1 STEP -1
      k += SubStr( s, i, 1 )
   NEXT
   ? "loops", t, k, SubStr( s, 2, 1 ) == Chr( 98 ), SubStr( s, 2, 1 ) == "b"
   // The fused pair's result going into a variable, an element and a captured variable.
   i := Asc( SubStr( s, 3, 1 ) )
   a[ 2 ] := Asc( SubStr( s, n, 1 ) )
   k := {|| t }
   t := Asc( SubStr( s, 1, 1 ) )
   ? "targets", i, a[ 2 ], Eval( k )

   // Off the fast paths the functions raise their own errors.
   ErrorBlock( {| err | Break( err ) } )
   FOR EACH k IN { {|| Len( 5 ) }, {|| Asc( 5 ) }, {|| Chr( "a" ) }, {|| SubStr( 5, 1, 1 ) }, ;
                   {|| SubStr( s, "1", 1 ) }, {|| Asc( SubStr( 5, 1, 1 ) ) }, {|| Asc( SubStr( s, 1, "x" ) ) } }
      BEGIN SEQUENCE
         Eval( k )
      RECOVER USING e
         ? "error", e:subCode, e:description, e:operation
      END
   NEXT
   i := 7
   BEGIN SEQUENCE
      i := Asc( SubStr( 5, 1, 1 ) )
   RECOVER USING e
      ? "recovered", i, e:subCode
   END
   k := { 7, 2.9, -2.9, -0.5, 10 ^ 25, 2 ^ 62, Log( 0.0 ) - Log( 0.0 ), 9.0 }
   ? "int", Int( k[ 1 ] ), Int( k[ 2 ] ), Int( k[ 3 ] ), Int( k[ 4 ] ), Int( k[ 5 ] ), Int( k[ 6 ] ), Int( k[ 7 ] ), ;
     Int( k[ 8 ] ) + 1, Len( Str( Int( k[ 2 ] ) ) )
   ErrorBlock( {| err | "?" } )
   ? "substituted", Asc( SubStr( 5, 1, 1 ) ), Len( 5 ), Chr( .T. ), Int( "x" )
   i := Asc( SubStr( s, "x", 1 ) )
   ?? "", i
RETURN
