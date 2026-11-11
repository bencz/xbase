// Extended codeblocks, compared with the reference: {| params | at the end of a line, then statements, then a line
// starting with }: LOCALs of their own, RETURN (anywhere; NIL without one), loops and IF, the enclosing function's
// LOCALs captured and changed, inside a command (?) and as an argument, nested ones, and one evaluated many times.
// Also FOR EACH, TRY and WITH OBJECT inside one. Beyond the reference (which rejects them): a codeblock inside one
// capturing its LOCALs (also WITH OBJECT's), and its LOCALs passed by reference.
PROCEDURE Main()
   LOCAL n := 10, b, c, a := { 3, 1, 2 }
   b := {| x, y |
      LOCAL z := x + y
      IF z > n
         RETURN "big " + Str( z, 3 )
      ENDIF
      n++
      RETURN z
      }
   ? Eval( b, 1, 2 ), n, Eval( b, 20, 30 ), n
   c := {||
      LOCAL i, s := 0
      FOR i := 1 TO 4
         s += i
      NEXT
      RETURN s
   }
   ? Eval( c )
   ? Eval( {| q |
      QOut( "no return", q )
      }, 7 )
   ASort( a, , , {| x, y |
      RETURN x > y
   } )
   ? a[ 1 ], a[ 2 ], a[ 3 ]
   ? Eval( Eval( {| k |
      RETURN {| m |
         RETURN m * 7
      }
   }, 6 ), 7 )
   AEval( a, {| x, i |
      LOCAL e
      FOR EACH e IN { x, i }
         ?? " " + LTrim( Str( e ) )
      NEXT
   } )
   ? Eval( {||
      LOCAL e
      TRY
         Throw( "thrown inside" )
      CATCH e
         RETURN e
      END
      RETURN "not here"
   } )
   ? Eval( {| o |
      WITH OBJECT o
         RETURN :ClassName()
      END
      RETURN NIL
   }, ErrorNew() )
   ? "by reference:", Eval( {| p |
      LOCAL z := 1
      Bump( @z )
      RETURN Str( z, 2 ) + Str( z + p, 4 )
   }, 100 )
   ? "captured:", Eval( {| p |
      LOCAL k := p * 2, inner
      inner := {|| k++ }
      Eval( inner )
      RETURN k + Eval( inner )
   }, 5 )
   ? "with object:", Eval( {| o |
      LOCAL r
      WITH OBJECT o
         r := Eval( {|| :cargo } )
      END
      RETURN r
   }, TCargo():New() )
   n := 0
   FOR c := 1 TO 1000
      Eval( b, 0, 0 )
   NEXT
   ? "many:", n
RETURN

STATIC PROCEDURE Bump( v )
   v++
RETURN

CLASS TCargo
   VAR cargo INIT 5
ENDCLASS
