// AWAIT FOR EACH at its edges, each beside FOR EACH where both apply: nested loops (over other variables, and over the
// same variable, which each puts back as it found it); a PRIVATE loop variable left by RETURN, put back for the
// caller; an array that grows or shrinks during the walk and a hash that loses keys (the steps FOR EACH takes); an
// element task that fails (the error reaches the loop's handler, the variable put back); an object whose
// GetAsyncEnumerator() gives something that is no asynchronous enumerator, and a source that is no collection (the
// errors FOR EACH gives); an empty array, hash and string (no step).
MEMVAR p

PROCEDURE Main()
   LOCAL x := "x0", y := "y0", a, h, c, e
   PRIVATE p := "p0"

   c := ""
   AWAIT FOR EACH x IN { 1, 2 }
      AWAIT FOR EACH y IN { "a", "b" }
         c += LTrim( Str( x ) ) + y + " "
      NEXT
   NEXT
   ? "nested:", c, x, y
   c := ""
   AWAIT FOR EACH x IN { 1, 2 }
      AWAIT FOR EACH x IN { "a", "b" }
         c += x
      NEXT
      c += ValType( x ) + LTrim( Str( x ) ) + " "
   NEXT
   ? "same variable:", c, x

   ? "private:", Leave():Result, p

   ? "grows:", Grow( .T. ):Result, Grow( .F. ):Result
   ? "shrinks:", Shrink( .T. ):Result, Shrink( .F. ):Result
   ? "hash loses keys:", HashShrink( .T. ):Result, HashShrink( .F. ):Result

   c := ""
   TRY
      AWAIT FOR EACH x IN { TaskFromResult( 1 ), Fails(), TaskFromResult( 3 ) }
         c += LTrim( Str( x ) )
      NEXT
   CATCH e
      c += " caught " + e:description
   END
   ? "element fails:", c, x

   ? "not async:", Try( {|| Walk( NotAsync():New() ):Result } )
   ? "no collection:", Try( {|| Walk( 5 ):Result } ), Try( {|| Plain( 5 ) } )
   ? "empty:", Walk( {} ):Result, Walk( { => } ):Result, Walk( "" ):Result
RETURN

ASYNC FUNCTION Leave()
   AWAIT FOR EACH p IN { "a", "b" }
      IF p == "b"
         RETURN "left at " + p
      ENDIF
   NEXT
RETURN "end"

// The values FOR EACH (lAwait .F.) or AWAIT FOR EACH sees in an array that grows by one while the walk is at 2.
ASYNC FUNCTION Grow( lAwait )
   LOCAL a := { 1, 2, 3 }, x, c := ""
   IF lAwait
      AWAIT FOR EACH x IN a
         c += LTrim( Str( x ) )
         IF x == 2
            AAdd( a, 4 )
         ENDIF
      NEXT
   ELSE
      FOR EACH x IN a
         c += LTrim( Str( x ) )
         IF x == 2
            AAdd( a, 4 )
         ENDIF
      NEXT
   ENDIF
RETURN c

ASYNC FUNCTION Shrink( lAwait )
   LOCAL a := { 1, 2, 3, 4 }, x, c := ""
   IF lAwait
      AWAIT FOR EACH x IN a
         c += LTrim( Str( x ) )
         IF x == 2
            ASize( a, 2 )
         ENDIF
      NEXT
   ELSE
      FOR EACH x IN a
         c += LTrim( Str( x ) )
         IF x == 2
            ASize( a, 2 )
         ENDIF
      NEXT
   ENDIF
RETURN c

ASYNC FUNCTION HashShrink( lAwait )
   LOCAL h := { "a" => 1, "b" => 2, "c" => 3 }, x, k, c := ""
   IF lAwait
      AWAIT FOR EACH k => x IN h
         c += k + LTrim( Str( x ) )
         IF k == "a"
            HDel( h, "c" )
         ENDIF
      NEXT
   ELSE
      FOR EACH k => x IN h
         c += k + LTrim( Str( x ) )
         IF k == "a"
            HDel( h, "c" )
         ENDIF
      NEXT
   ENDIF
RETURN c

ASYNC FUNCTION Fails()
   AWAIT TaskYield()
RETURN 1 + "a"

ASYNC FUNCTION Walk( xSource )
   LOCAL x, c := ""
   AWAIT FOR EACH x IN xSource
      c += ValType( x )
   NEXT
RETURN "[" + c + "]"

STATIC FUNCTION Plain( xSource )
   LOCAL x, c := ""
   FOR EACH x IN xSource
      c += ValType( x )
   NEXT
RETURN "[" + c + "]"

STATIC FUNCTION Try( b )
   LOCAL x
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      x := Eval( b )
   RECOVER USING x
      x := x:subsystem + "/" + LTrim( Str( x:subCode ) ) + " " + x:description + " " + x:operation
   END SEQUENCE
RETURN x

CLASS NotAsync
   METHOD GetAsyncEnumerator() INLINE { 1, 2 }
ENDCLASS
