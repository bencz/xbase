// A codeblock or reference that takes a LOCAL FOR EACH variable during an iteration keeps that iteration's element;
// the variable itself moves on and gets its value back after the loop.
PROCEDURE Main()
   LOCAL a := { 10, 20, 30 }, m := {}, v := "orig", r := {}, k := 0
   FOR EACH v IN a
      AAdd( m, {| n | IIf( n == NIL, v, v := n ) } )
      IF ++k == 2
         ?? "old block in iter 2:", Eval( m[ 1 ] )
         Eval( m[ 1 ], 11 )
         ?? " v now:", v
      ENDIF
      AAdd( r, Keep( @v ) )
   NEXT
   ? "after", v, Eval( m[ 1 ] ), Eval( m[ 2 ] ), Eval( m[ 3 ] ), a[ 1 ], a[ 2 ], a[ 3 ]
   v := 5
   ? "v := 5", Eval( m[ 1 ] ), Eval( m[ 3 ] ), Eval( r[ 1 ] ), Eval( r[ 3 ] )
   Eval( m[ 3 ], 99 )
   ? "write m[3]", v, a[ 3 ]
   RETURN

FUNCTION Keep( p )
   RETURN {|| p }
