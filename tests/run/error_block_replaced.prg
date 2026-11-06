// An error block that replaces itself (ErrorBlock() inside it) keeps running:
// the collections its work triggers must not free it or its cells.
PROCEDURE Main()
   LOCAL x
   Install()
   x := 1 + "a"
   ? x
RETURN

STATIC FUNCTION Install()
   ErrorBlock( MakeHandler() )
RETURN NIL

STATIC FUNCTION MakeHandler()
   LOCAL c := "zzzzz", d := { 1, 2, 3 }
RETURN {| e | Replace(), Churn(), c + "!" + Str( Len( d ) ) }

STATIC FUNCTION Replace()
   ErrorBlock( {| e | 0 } )
RETURN NIL

STATIC FUNCTION Churn()
   LOCAL i, a := {}, k
   __GcCollect()
   FOR i := 1 TO 50000
      k := "q" + Str( i )
      AAdd( a, {|| k } )
      AAdd( a, { 9, 9, 9, 9 } )
   NEXT
RETURN NIL
