// Operator overloading and scalar classes, compared with the reference. OPERATOR "op" [ARG ...] INLINE and METHOD name
// OPERATOR "op": + - * / % ^ (also **), = == != (also <> #) < <= > >= $, ! (also .NOT.), ++ and --, [] read and
// assigned (the index, then the value); only the left operand's class is asked (1 + o is an error), == calls the
// operator even for the same object, and unary minus has none. ASSOCIATE CLASS ... WITH TYPE: values of the type
// answer the class's methods and class variables as Self, ClassName() is the class's, an unknown message is the
// class's error. An operator on a cast to a class without it is the plain operator's error. Deliberate difference: a
// scope violation names the class (the reference names the type there).
// Also ObjMethods leaving operators out, and ClassAssociate's errors.
PROCEDURE Main()
   LOCAL a := TV():New( 1 ), b := TV():New( 2 ), c, e
   ? "arith:", ( a + b ):n, ( a - 1 ):n, ( a * 3 ):n, ( a / 2 ):n, ( a % 2 ):n, ( a ^ 2 ):n, ( a ** 3 ):n
   ? "compare:", a == b, a = b, a != b, a <> b, a # b, a < b, a <= b, a > b, a >= b, a $ b, a == a
   ? "not:", !a, .NOT. a
   c := a
   c++
   ? "inc:", c:n, a:n, ( a + 0 ):n
   c--
   ? "dec:", c:n
   ? "index:", a[ 5 ], a:log
   a[ 2 ] := 7
   a[ 3 ] += 1
   ? "assign:", a:log
   ? "named:", ( a + 10 ):Who, ArrayText( ObjMethods( a ) )

   ASSOCIATE CLASS TStr WITH TYPE CHARACTER
   ASSOCIATE CLASS TNum WITH TYPE NUMERIC
   ? "scalar:", "abc":Upper2(), "abc":ClassName(), ( 3 ):Twice(), ( 2.5 ):Twice(), "x":Count, Len( "abc":Self2() ), ;
      ( {1, 2} ):ClassName(), ValType( "abc":Self2() ), "abc":Shout()
   ? "associate:", ClassAssociate( TStr(), "TIMESTAMP" ), ClassAssociate( TStr(), "character" )

   ErrorBlock( {| oErr | QOut( "error:", oErr:subCode, oErr:description, oErr:operation ), Break( oErr ) } )
   FOR EACH e IN { {|| 1 + a }, {|| -a }, {|| TOpChild():New():TOpBase + 1 }, {|| "abc":Nope() }, ;
                   {|| ( 1 ):Nope() }, {|| "abc":Hidden() }, ;
                   {|| ClassAssociate( TV():New( 1 ), "DATE" ) }, {|| ClassAssociate( "x", "DATE" ) } }
      BEGIN SEQUENCE
         Eval( e )
      END SEQUENCE
   NEXT
RETURN

STATIC FUNCTION ArrayText( a )
   LOCAL c := "", s
   FOR EACH s IN a
      c += IIF( Empty( c ), "", "," ) + s
   NEXT
RETURN c

CLASS TV
   VAR n
   VAR log INIT ""
   METHOD New( n ) INLINE ::n := n, Self
   METHOD Who() INLINE "TV " + LTrim( Str( ::n ) )
   METHOD Plus( x ) OPERATOR "+"
   OPERATOR "-" ARG x INLINE TV():New( ::n - IIF( ValType( x ) == "O", x:n, x ) )
   OPERATOR "*" ARG x INLINE TV():New( ::n * x )
   OPERATOR "/" ARG x INLINE TV():New( ::n / x )
   OPERATOR "%" ARG x INLINE TV():New( ::n % x )
   OPERATOR "**" ARG x INLINE TV():New( ::n ^ x )
   OPERATOR "==" ARG x INLINE "eqeq"
   OPERATOR "=" ARG x INLINE "eq"
   OPERATOR "<>" ARG x INLINE "ne"
   OPERATOR "<" ARG x INLINE "lt"
   OPERATOR "<=" ARG x INLINE "le"
   OPERATOR ">" ARG x INLINE "gt"
   OPERATOR ">=" ARG x INLINE "ge"
   OPERATOR "$" ARG x INLINE "in"
   OPERATOR ".not." INLINE "not"
   OPERATOR "++" INLINE TV():New( ::n + 10 )
   OPERATOR "--" INLINE TV():New( ::n - 100 )
   OPERATOR "[]" ARG i, v INLINE ::log += "[" + Show( i ) + "," + Show( v ) + "]", i * 100
ENDCLASS

METHOD Plus( x ) CLASS TV
RETURN TV():New( ::n + IIF( ValType( x ) == "O", x:n, x ) )

STATIC FUNCTION Show( x )
RETURN IIF( x == NIL, "NIL", LTrim( Str( x ) ) )

CLASS TOpBase
ENDCLASS

CLASS TOpChild INHERIT TOpBase
   OPERATOR "+" ARG x INLINE "child +"
ENDCLASS

CLASS TStr
   CLASSVAR Count INIT 5
   METHOD Upper2() INLINE Upper( Self ) + "!"
   METHOD Self2() INLINE Self
   METHOD Shout() INLINE ::Upper2() + ::Hidden()
   HIDDEN:
   METHOD Hidden() INLINE "?"
ENDCLASS

CLASS TNum
   METHOD Twice() INLINE Self * 2
ENDCLASS
