// Every operator a class can overload, in one class, each spelling of each (OPERATOR "op" [ARG ...] INLINE and METHOD
// name OPERATOR "op"): + - * / % ^ (and **), = == != (and <> #), < <= > >= $, ! (and .NOT.), ++ and --, [] read and
// assigned: the index, then the value (NIL when reading); o[i] += x reads, then assigns; o[i]++ the same; o[i][j]
// indexes what o[i] gives. xbase's own: unary minus ("-" UNARY), the bit operators & | ^^ << >> ~, and .AND. and
// .OR., which receive the right side's value, evaluated once, only when the left side is an object with the operator
// (in a condition the operator's result is tested; a logical left side still decides alone, the right side then not
// evaluated; through a macro alike; an object without the operator is the condition's error). A left side that is a
// condition by its form (a comparison, a negation, another .AND. or .OR.) is tested as a condition, whatever its
// operator gave. A collection of the
// program inherits Enumerable, gives its elements by GetEnumerator() (a YIELD), and its [] reads and writes them: FOR
// EACH and Enumerable's methods go through the enumerator, an index out of its bounds raises the class's own error.
// Only the left (or only) operand's class is asked: 1 + o is the plain operator's error, as is an operator a class
// does not overload.
PROCEDURE Main()
   LOCAL a := TAllOps():New( 6 ), b := TAllOps():New( 4 ), c, oList, x, cSeen := "", oErr
   PRIVATE cSide := "", a2
   ? "arithmetic:", ( a + b ):n, ( a - b ):n, ( a * 2 ):n, ( a / 4 ):n, ( a % 4 ):n, ( a ^ 2 ):n, ( a ** 3 ):n
   ? "equality:", a = b, a == b, a != b, a <> b, a # b, a == a
   ? "order:", a < b, a <= b, a > b, a >= b
   ? "contains:", a $ b, a $ 3
   ? "not:", !a, .NOT. a
   c := a
   c++
   ? "increment:", c:n, a:n
   c--
   ? "decrement:", c:n
   ? "index read:", a[ 3 ], a[ "k" ]
   a[ 1 ] := "one"
   a[ 2 ] += 5
   a[ 4 ]++
   ? "index assigned:", a:Log()
   ? "nested:", a[ 7 ][ 2 ]
   ? "unary minus:", ( -a ):n
   ? "bits:", ( a & 3 ):n, ( a | 1 ):n, ( a ^^ 5 ):n, ( a << 2 ):n, ( a >> 1 ):n, ( ~a ):n
   ? "and/or:", a .AND. Side( "r" ), a .OR. Side( "s" ), cSide
   ? "in a condition:", IIf( a .OR. .F., "yes", "no" ), IIf( b .OR. .F., "yes", "no" )
   IF b .OR. .T.
      ? "IF:", "taken"
   ENDIF
   cSide := ""
   ? "logical left:", .F. .AND. Side( "x" ), .T. .OR. Side( "y" ), .T. .AND. 5, cSide
   ? "run-time logical left:", Flag( .F. ) .AND. Side( "x" ), Flag( .T. ) .OR. Side( "y" ), ;
      &( "Flag( .F. ) .AND. Side( 'z' )" ), cSide
   IF !a
      ? "IF !a:", "taken"
   ELSE
      ? "IF !a:", "not taken"
   ENDIF
   ? "IIf( !b ):", IIf( !b, "yes", "no" ), IIf( !a .AND. .T., "yes", "no" )
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      ? 5 .AND. .T.
   RECOVER USING oErr
      ? "number on the left:", oErr:subCode, oErr:description
   END SEQUENCE
   a2 := a
   ? "macro:", &( "a2 .AND. 1" ), &( "a2 .OR. .F." ), &( ".F. .OR. 2" ), &( ".F. .AND. a2" )
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      ? TList():New( {} ) .AND. .T.
   RECOVER USING oErr
      ? "no operator:", oErr:subCode, oErr:description
   END SEQUENCE
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      ? ( a = b ) .AND. .T.
   RECOVER USING oErr
      ? "comparison on the left:", oErr:subCode, oErr:description
   END SEQUENCE

   oList := TList():New( { 10, 20, 30 } )
   ? "list read:", oList[ 1 ], oList[ 3 ], oList:Count()
   oList[ 2 ] := 25
   oList[ 1 ] += 1
   FOR EACH x IN oList
      cSeen += LTrim( Str( x ) ) + " "
   NEXT
   ? "list for each:", cSeen
   ? "list enumerable:", oList:Where( {| n | n > 20 } ):Count(), oList:Select( {| n | n * 2 } ):Sum(), oList:Sum()
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      ? oList[ 4 ]
   RECOVER USING oErr
      ? "list bound:", oErr:subCode, oErr:description, oErr:operation
   END SEQUENCE

   BEGIN SEQUENCE WITH {| e | Break( e ) }
      ? 1 + a
   RECOVER USING oErr
      ? "right operand:", oErr:subCode, oErr:description, oErr:operation
   END SEQUENCE
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      ? -oList
   RECOVER USING oErr
      ? "no unary minus:", oErr:subCode, oErr:description, oErr:operation
   END SEQUENCE
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      ? oList & 1
   RECOVER USING oErr
      ? "no &:", oErr:subCode, oErr:description, oErr:operation
   END SEQUENCE
RETURN

CLASS TAllOps
   VAR n
   VAR aLog INIT {}
   METHOD New( n ) INLINE ::n := n, Self
   METHOD Log()
   METHOD Plus( x ) OPERATOR "+"
   OPERATOR "-" ARG x INLINE TAllOps():New( ::n - Value( x ) )
   OPERATOR "*" ARG x INLINE TAllOps():New( ::n * Value( x ) )
   OPERATOR "/" ARG x INLINE TAllOps():New( ::n / Value( x ) )
   OPERATOR "%" ARG x INLINE TAllOps():New( ::n % Value( x ) )
   OPERATOR "^" ARG x INLINE TAllOps():New( ::n ^ Value( x ) )
   OPERATOR "=" ARG x INLINE "= " + LTrim( Str( Value( x ) ) )
   OPERATOR "==" ARG x INLINE IIf( ValType( x ) == "O" .AND. x:n == ::n, "same", "== " + LTrim( Str( Value( x ) ) ) )
   OPERATOR "!=" ARG x INLINE ::n != Value( x )
   OPERATOR "<" ARG x INLINE ::n < Value( x )
   OPERATOR "<=" ARG x INLINE ::n <= Value( x )
   OPERATOR ">" ARG x INLINE ::n > Value( x )
   OPERATOR ">=" ARG x INLINE ::n >= Value( x )
   OPERATOR "$" ARG x INLINE "$ " + LTrim( Str( Value( x ) ) )
   OPERATOR "!" INLINE ::n == 0
   OPERATOR "++" INLINE TAllOps():New( ::n + 1 )
   OPERATOR "--" INLINE TAllOps():New( ::n - 1 )
   METHOD Element( i, v ) OPERATOR "[]"
   OPERATOR "-" UNARY INLINE TAllOps():New( - ::n )
   OPERATOR "&" ARG x INLINE TAllOps():New( BitAnd( ::n, x ) )
   OPERATOR "|" ARG x INLINE TAllOps():New( BitOr( ::n, x ) )
   OPERATOR "^^" ARG x INLINE TAllOps():New( BitXor( ::n, x ) )
   OPERATOR "<<" ARG x INLINE TAllOps():New( BitShl( ::n, x ) )
   OPERATOR ">>" ARG x INLINE TAllOps():New( BitShr( ::n, x ) )
   OPERATOR "~" INLINE TAllOps():New( BitNot( ::n ) )
   OPERATOR ".AND." ARG x INLINE "and " + Shown( x )
   METHOD Either( x ) OPERATOR ".OR."
ENDCLASS

METHOD Either( x ) CLASS TAllOps
RETURN ::n > 5 .OR. x

METHOD Plus( x ) CLASS TAllOps
RETURN TAllOps():New( ::n + Value( x ) )

// [] read (v NIL) and assigned (v the value): every access logged; a read of 7 gives an array, which [] indexes again.
METHOD Element( i, v ) CLASS TAllOps
   AAdd( ::aLog, "[" + Shown( i ) + "]" + IIf( PCount() > 1, ":=" + Shown( v ), "" ) )
   IF ValType( i ) == "N" .AND. i == 7
      RETURN { "a", "b", "c" }
   ENDIF
RETURN IIf( ValType( i ) == "N", i * 100, "key " + i )

METHOD Log() CLASS TAllOps
   LOCAL c := "", s
   FOR EACH s IN ::aLog
      c += s + " "
   NEXT
RETURN c

// A logical the compiler cannot see.
FUNCTION Flag( l )
RETURN l

// A right side whose evaluation shows: it logs its letter.
FUNCTION Side( c )
   M->cSide += c
RETURN .T.

STATIC FUNCTION Value( x )
RETURN IIf( ValType( x ) == "O", x:n, x )

STATIC FUNCTION Shown( x )
   DO CASE
   CASE ValType( x ) == "N"
      RETURN LTrim( Str( x ) )
   CASE ValType( x ) == "L"
      RETURN IIf( x, ".T.", ".F." )
   CASE x == NIL
      RETURN "NIL"
   ENDCASE
RETURN x

CLASS TList INHERIT Enumerable
   VAR aItems
   METHOD New( a ) INLINE ::aItems := AClone( a ), Self
   METHOD GetEnumerator()
   METHOD Item( i, v ) OPERATOR "[]"
ENDCLASS

METHOD GetEnumerator() CLASS TList
   LOCAL x
   FOR EACH x IN ::aItems
      YIELD x
   NEXT
RETURN

// The element at i, or assigned v there; an index outside the list is the class's error (EG_BOUND, as an array's).
METHOD Item( i, v ) CLASS TList
   LOCAL oErr
   IF ValType( i ) != "N" .OR. i < 1 .OR. i > Len( ::aItems )
      oErr := ErrorNew()
      oErr:genCode := 5
      oErr:subCode := 1132
      oErr:description := "Bound error"
      oErr:operation := "TLIST[]"
      RETURN Eval( ErrorBlock(), oErr )
   ENDIF
   IF PCount() > 1
      ::aItems[ i ] := v
   ENDIF
RETURN ::aItems[ i ]
