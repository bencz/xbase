// What a call of a function of the module gives, as the operators and ValType() after it see it: a string, a number
// of either kind, a logical, NIL for a procedure or a bare RETURN, a value of a type chosen at run time, a result an
// error's block substituted (of another type than the operation gives), a recursive function, and a function versioned
// for references reached with and without them, one making codeblocks, and an object's operator.
PROCEDURE Main()
   LOCAL n := 0, x, c
   ? TF( .T. ) + TF( .F. ), TF( .F. ) = "F", ValType( TF( .T. ) ), Len( TF( .T. ) + "x" )
   ? Twice( 21 ) + 0.5, Twice( 2.25 ) * 2, Twice( 5 ) = 10, ValType( Twice( 1 ) )
   ? Nothing(), ValType( Nothing() ), Bare( 1 ), ValType( Bare( 0 ) ), Bare( 0 ) == NIL
   ? Pick( 1 ) + Pick( 1 ), Pick( 2 ) + Pick( 2 ), Pick( 3 ), ValType( Pick( 4 ) ), Pick( 5 )
   ErrorBlock( {| e | iif( e:canSubstitute, "substituted", Break( e ) ) } )
   ? Div( 10, 4 ), Div( 1, 0 ), Div( 1, 0 ) + "!", ValType( Div( 1, 0 ) ), ValType( Div( 3, 2 ) )
   ? Text( 3 ) + "|", Text( 0 ) + "|", ValType( Text( 0 ) )
   ? Fact( 10 ), Fact( 20 ), Fact( 21 ), ValType( Fact( 25 ) )
   x := 5
   ? Bump( x ), x, Bump( @x ), x, Bump( 1 ) + Bump( 2 )
   c := "ab"
   ? Grow( c ) + "|" + c, Grow( @c ) + "|" + c
   AEval( { 1, 2, 3 }, {| v | n += Twice( v ) } )
   ? n, Eval( {| v | Twice( v ) + 1 }, 4 ), Eval( {| v | Bump( @v ) }, 7 )
   ? Scaled( 3 ), Scaled( 2.5 ), ValType( Scaled( 1 ) ), Same( W():New() ), ValType( Same( W():New() ) )
RETURN

CLASS W
   OPERATOR "==" ARG x INLINE "same"
ENDCLASS

// An object's operator gives what its method gives: not a logical here.
STATIC FUNCTION Same( o )
RETURN o == NIL

STATIC FUNCTION Scaled( n )
   LOCAL b := {| x | x * n }
RETURN Eval( b, 2 ) + Eval( {| x | {| y | x + y + n } }, 10 ):Eval( 1 )

STATIC FUNCTION TF( l )
RETURN iif( l, "T", "F" )

STATIC FUNCTION Twice( n )
RETURN n * 2

STATIC PROCEDURE Nothing()
RETURN

STATIC FUNCTION Bare( n )
   IF n > 0
      RETURN "one"
   ENDIF
RETURN

STATIC FUNCTION Pick( n )
   DO CASE
   CASE n == 1
      RETURN 7
   CASE n == 2
      RETURN "s"
   CASE n == 3
      RETURN .T.
   CASE n == 4
      RETURN {}
   ENDCASE
RETURN Date() - Date()

STATIC FUNCTION Div( a, b )
RETURN a / b

STATIC FUNCTION Text( n )
RETURN Replicate( "x", n ) + Space( 1 )

STATIC FUNCTION Fact( n )
RETURN iif( n <= 1, 1, n * Fact( n - 1 ) )

STATIC FUNCTION Bump( n )
   n++
RETURN n

STATIC FUNCTION Grow( c )
   c += "!"
RETURN c
