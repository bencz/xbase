// Every operator a class can overload, in one class, each spelling of each (OPERATOR "op" [ARG ...] INLINE and METHOD
// name OPERATOR "op"): + - * / % ^ (and **), = == != (and <> #), < <= > >= $, ! (and .NOT.), ++ and --, and [] read and
// assigned: the index, then the value (NIL when reading); o[i] += x reads, then assigns; o[i]++ the same; o[i][j]
// indexes what o[i] gives. A collection of the program inherits Enumerable, gives its elements by GetEnumerator() (a
// YIELD), and its [] reads and writes them: FOR EACH and Enumerable's methods go through the enumerator, an index out
// of its bounds raises the class's own error. Only the left (or only) operand's class is asked: 1 + o is the plain
// operator's error; unary minus has no operator.
PROCEDURE Main()
   LOCAL a := TAllOps():New( 6 ), b := TAllOps():New( 4 ), c, oList, x, cSeen := "", oErr
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
      ? -a
   RECOVER USING oErr
      ? "unary minus:", oErr:subCode, oErr:description, oErr:operation
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
ENDCLASS

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

STATIC FUNCTION Value( x )
RETURN IIf( ValType( x ) == "O", x:n, x )

STATIC FUNCTION Shown( x )
RETURN IIf( ValType( x ) == "N", LTrim( Str( x ) ), IIf( x == NIL, "NIL", x ) )

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
