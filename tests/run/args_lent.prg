// A call through the dynamic entry lends its arguments to the callee (xb_ctx.lent): a compiled function takes those it
// has parameters for, so that `x := NIL` there lets go of what only the argument held. Checked by the live objects
// __GcCollect() counts, at -O0 and -O2 (run0.*): a method's argument, the send made 3 times (the first fills the send's
// cache, the others take its fast path), through Super:, and an argument copied into the call's arguments (a value made
// before the blocks of an .AND.: the slot it leaves is let go too). And what must stay: a method that reads its
// arguments as received (PCount(), PValue() after another send), a function that passes `...` on twice, arguments past
// the parameters, a method that sends to Self with its own arguments, and the caller's LOCAL passed as the argument,
// read after the call, each checked by its result.
PROCEDURE Main()
   LOCAL o := Holder():New(), k, cOut := ""
   FOR k := 1 TO 3
      cOut += IIf( o:Take( Filled( 50000 ) ), "T", "F" )
   NEXT
   ? "method:", cOut, Child():New():TakeNew(), o:Take( Filled( 50000 ), k > 0 .AND. k < 10 )
   ? "as received:", o:Counted( { 1, 2 }, "x" ), o:Counted( { 1 } )
   ? "spread:", Spread( { 1, 2 }, { 3 } )
   ? "past the parameters:", o:First( { 7 }, { 8, 9 }, "z" )
   ? "to Self:", o:Again( { 1, 2, 3 } )
   ? "caller's local:", CallerKeeps( o )
RETURN

// An array of n arrays of their own.
STATIC FUNCTION Filled( n )
   LOCAL a := Array( n ), i
   FOR i := 1 TO n
      a[ i ] := { i }
   NEXT
RETURN a

STATIC FUNCTION Spread( ... )
RETURN Sum( ... ) + Sum( ... ) * 100

STATIC FUNCTION Sum( a, b )
   LOCAL n := 0
   BEGIN SEQUENCE
      n := Len( a ) + Len( b ) * 10
   END SEQUENCE
RETURN n

STATIC FUNCTION CallerKeeps( o )
   LOCAL a := { 1, 2, 3, 4 }
   o:Size( a )
RETURN Len( a ) + o:Size( a ) * 10

CLASS Holder
   METHOD New() INLINE Self
   METHOD Take( a, l )
   METHOD Counted( a, x )
   METHOD First( a )
   METHOD Again( a )
   METHOD Size( a )
ENDCLASS

METHOD Take( a, l ) CLASS Holder
   LOCAL n0, nLen
   n0 := __GcCollect()
   nLen := Len( a )
   a := NIL
RETURN n0 - __GcCollect() >= 50000 .AND. nLen == 50000 .AND. ( l == NIL .OR. l )

METHOD Counted( a, x ) CLASS Holder
   LOCAL n := ::Size( a )
RETURN Str( PCount(), 1 ) + Str( n, 1 ) + ValType( PValue( 1 ) ) + ValType( PValue( 2 ) )

METHOD First( a ) CLASS Holder
   ::Size( a )
RETURN Len( a ) * 10 + ::Size( a )

METHOD Again( a ) CLASS Holder
RETURN ::Size( a ) + Len( a ) * 10

METHOD Size( a ) CLASS Holder
RETURN Len( a )

CLASS Child INHERIT Holder
   METHOD TakeNew()
ENDCLASS

METHOD TakeNew() CLASS Child
RETURN ::Super:Take( Filled( 50000 ) )
