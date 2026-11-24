// =, == and != (also <> and #) with NIL, done by compiled code without the runtime when an operand is surely NIL: NIL
// against each type on either side, as a value and as a condition (IF, .NOT., .AND.), from a value of known and of
// unknown type (an element, a parameter, a macro), NIL as the constant, a LOCAL never assigned and a call's result; an
// object whose class defines the operators is asked on the left, before anything else (deliberately: the reference does
// not ask it for NIL; on the right, NIL is the left operand and the object is not asked); an object without them gives
// the rule. Also literals of NILs (they hold items: read, written with numbers and logicals, grown), one too large for
// the thread's hole, and trees of them kept through collections.
PROCEDURE Main()
   LOCAL aVals := { NIL, 0, 1.5, "", "x", .T., .F., {}, { 1 }, { => }, {|| 1 }, SToD( "20260101" ), @Upper(), ;
                    TPlain():New() }, x, c, a, n, i, aTree, u

   c := ""
   FOR EACH x IN aVals
      c += ValType( x ) + Flags( x ) + " "
   NEXT
   ? "values:", c
   c := ""
   FOR EACH x IN aVals
      c += Conds( x )
   NEXT
   ? "conditions:", c
   ? "unknown type:", Flags( &( "NIL" ) ), Flags( &( "1" ) ), Flags( aVals[ 1 ] ), Flags( aVals[ 5 ] )
   x := TOps():New()
   ? "operators:", x == NIL, x = NIL, x != NIL, x <> NIL, x # NIL, NIL == x, NIL != x, x:nAsked
   ? "surely NIL:", u == NIL, Nothing() == NIL, Nothing() != 0, u == "x", x == u, u == x, Nothing() == x, ;
      Nothing() != TPlain():New(), x:nAsked

   a := { NIL, NIL, NIL }
   ? "literal of NILs:", Len( a ), a[ 1 ] == NIL, ValType( a[ 3 ] ), a[ 2 ]
   a[ 1 ] := 5
   a[ 2 ] := .T.
   AAdd( a, "s" )
   ? "written:", a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ], Len( a )
   a := { NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL, NIL }
   a[ 20 ] := 20
   ? "large:", Len( a ), a[ 1 ], a[ 19 ], a[ 20 ]
   n := 0
   FOR i := 1 TO 40
      aTree := Make( 8 )
      n += Check( aTree )
      IF i % 10 == 0
         __GcCollect()
      ENDIF
   NEXT
   ? "trees:", n
RETURN

// Each comparison of x with NIL, either side: 1 for .T., 0 for .F.
STATIC FUNCTION Flags( x )
RETURN B( x == NIL ) + B( x = NIL ) + B( x != NIL ) + B( x <> NIL ) + B( x # NIL ) + ;
       B( NIL == x ) + B( NIL = x ) + B( NIL != x )

STATIC FUNCTION Conds( x )
   LOCAL c := ""
   IF x == NIL
      c += "n"
   ELSE
      c += "v"
   ENDIF
   IF ! ( x != NIL )
      c += "N"
   ENDIF
   IF x != NIL .AND. ValType( x ) == "C"
      c += "c"
   ENDIF
RETURN c

STATIC FUNCTION Nothing()
RETURN NIL

STATIC FUNCTION B( l )
RETURN IIf( l, "1", "0" )

STATIC FUNCTION Make( d )
   IF d == 0
      RETURN { NIL, NIL }
   ENDIF
RETURN { Make( d - 1 ), Make( d - 1 ) }

STATIC FUNCTION Check( aNode )
   IF aNode[ 1 ] == NIL
      RETURN 1
   ENDIF
RETURN 1 + Check( aNode[ 1 ] ) + Check( aNode[ 2 ] )

CLASS TPlain
ENDCLASS

CLASS TOps
   VAR nAsked INIT 0
   OPERATOR "==" ARG y INLINE ::nAsked++, .T.
   OPERATOR "=" ARG y INLINE ::nAsked++, .T.
   OPERATOR "!=" ARG y INLINE ::nAsked++, .T.
ENDCLASS
