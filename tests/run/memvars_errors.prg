// Memory variable errors and lifetimes, compared with the reference: a missing variable is error 1003 (retried when
// the block returns .T.), the PRIVATEs of a function that unwinds end, PARAMETERS with an element passed by reference
// and fewer arguments than names, recursion, and many PRIVATEs through collections.
MEMVAR nothere, a, b, c, deep, big
PROCEDURE Main()
   LOCAL arr := { 1, 2 }, e, k, s
   ErrorBlock( {| err | Handler( err ) } )
   ? "read missing", nothere
   BEGIN SEQUENCE
      Unwinds()
   RECOVER USING e
      ? "recovered", e
   END SEQUENCE
   ? "after unwind", Probe()
   ElemParams( @arr[ 2 ], "only" )
   ? "elem", arr[ 2 ]
   ? "recursion", Rec( 5 )
   PRIVATE big := {}
   FOR k := 1 TO 20000
      PRIVATE tmp := Replicate( "x", k % 50 )
      AAdd( big, tmp )
      IF k % 5000 == 0
         __GcCollect( .T. )
      ENDIF
   NEXT
   s := 0
   FOR k := 1 TO Len( big )
      s += Len( big[ k ] )
   NEXT
   ? "gc", Len( big ), s
RETURN

STATIC FUNCTION Handler( e )
   STATIC n := 0
   n++
   ? "err", e:subCode, e:operation, e:canRetry, e:tries, ValType( e:args )
   IF n == 1
      PUBLIC nothere := "made by the handler"
      RETURN .T.
   ENDIF
   Break( "broke" )
RETURN NIL

PROCEDURE Unwinds()
   PRIVATE a := "in unwinds"
   ? a, M->missing_one
RETURN

FUNCTION Probe()
   LOCAL ok := .T.
   BEGIN SEQUENCE
      ? M->a
   RECOVER
      ok := .F.
   END SEQUENCE
RETURN ok

PROCEDURE ElemParams
   PARAMETERS b, c, deep
   ? "params", b, c, ValType( deep ), PCount()
   b := "changed"
RETURN

FUNCTION Rec( n )
   PRIVATE deep := n
   IF n > 0
      Rec( n - 1 )
   ENDIF
RETURN deep

