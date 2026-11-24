// Sends whose object's class the compiler does not know (objects made by a macro), performed from their inline cache
// by compiled code, each made again once the runtime filled its cache: one send that sees ten classes (more than the
// cache keeps: the oldest go, and come back) with methods that return numbers and strings, with as many parameters as
// the send's arguments (the method's direct entry), fewer and more; one send whose classes' methods take as many
// parameters, more and fewer, and return a number or a string; a variable read and assigned through one send in
// classes that keep it at different slots; an argument by reference (the caller's variable changes); a method that
// reads PCount(); an argument only the call holds, which the method lets go of (its destructor runs inside it); Super
// in a method, a cast, and arguments passed on with `...`; an error raised in a method (the levels its handler sees);
// a method of a class of another module; and one send on several threads.
STATIC s_nGone := 0

PROCEDURE Main()
   LOCAL aObjs := {}, i, o, n := 0, c := "", x, oErr, aTasks, nTotal

   FOR i := 1 TO 10
      AAdd( aObjs, &( "TShape" + LTrim( Str( i ) ) + "():New( " + LTrim( Str( i ) ) + " )" ) )
   NEXT
   FOR i := 1 TO 3
      FOR EACH o IN aObjs
         n += o:Area() + o:Scaled( 2 ) + o:Pair( 1, 2 )
         c += o:Name()
      NEXT
   NEXT
   ? "ten classes:", n, Len( c ), Left( c, 12 ), Right( c, 8 )
   o := aObjs[ 4 ]
   FOR i := 1 TO 3 // the first through the runtime, which fills the caches
      c := Str( o:Pair( 5 ) ) + Str( o:Scaled( 3, "unused", 9 ) ) + Str( o:Area( 1, 2 ) )
   NEXT
   ? "fewer and more arguments:", c

   n := 0
   FOR i := 1 TO 3
      FOR EACH o IN aObjs
         o:nTag := o:nTag + i
         n += o:nTag
      NEXT
   NEXT
   ? "variables at different slots:", n, aObjs[ 1 ]:nTag, aObjs[ 10 ]:nTag

   x := 10
   o := aObjs[ 2 ]
   ? "by reference:", o:Bump( @x ), x, o:Bump( x ), x
   FOR i := 1 TO 3
      c := LTrim( Str( o:Bump( @x ) ) )
   NEXT
   ? "by reference again:", c, x

   c := ""
   FOR i := 1 TO 3
      FOR EACH o IN aObjs
         x := o:Mixed( 1, 2 )
         c += IIf( ValType( x ) == "C", x, LTrim( Str( x ) ) ) + " "
      NEXT
   NEXT
   ? "mixed:", c
   o := aObjs[ 3 ]
   FOR i := 1 TO 3
      c := Str( o:Twice() ) + Str( o:TShape:Area() ) + Str( o:Area() ) + Str( Forward( o, 1, 2 ) )
   NEXT
   ? "Super, cast, ...:", c
   o := aObjs[ 2 ]
   ? "PCount:", o:Counted(), o:Counted( 1 ), o:Counted( 1, 2 )

   s_nGone := 0
   FOR i := 1 TO 3 // the first through the runtime, which fills the cache
      c := LTrim( Str( o:Drop( &( "TGone():New()" ) ) ) )
   NEXT
   ? "let go inside:", c, s_nGone

   BEGIN SEQUENCE WITH {| e | e:cargo := ProcName( 1 ) + "/" + ProcName( 2 ), Break( e ) }
      o:Failing( "a" )
   RECOVER USING oErr
      ? "error:", oErr:subCode, oErr:operation, oErr:cargo
   END SEQUENCE

   o := &( "TFar():New()" )
   ? "other module:", o:Far( 4 ), o:Far( 5 ), o:Name()

   aTasks := {}
   FOR i := 1 TO 4
      AAdd( aTasks, TaskRun( {|| Sum( aObjs, 2000 ) } ) )
   NEXT
   nTotal := 0
   FOR EACH o IN aTasks
      nTotal += o:Result
   NEXT
   ? "threads:", nTotal
RETURN

// The areas of the shapes, `nRounds` times over, through one send.
STATIC FUNCTION Forward( o, ... )
RETURN o:Pair( ... )

STATIC FUNCTION Sum( aObjs, nRounds )
   LOCAL i, o, n := 0
   FOR i := 1 TO nRounds
      FOR EACH o IN aObjs
         n += o:Area()
      NEXT
   NEXT
RETURN n

PROCEDURE Gone()
   s_nGone++
RETURN

CLASS TShape
   VAR nSide INIT 0
   VAR nTag INIT 0
   METHOD New( n ) INLINE ::nSide := n, Self
   METHOD Area() INLINE ::nSide * ::nSide
   METHOD Scaled( k ) INLINE ::nSide * k
   METHOD Pair( a, b ) INLINE IIf( b == NIL, -a, a * 10 + b )
   METHOD Name() INLINE "shape"
   METHOD Bump( v ) INLINE v := v + 1, v
   METHOD Counted( ... ) INLINE PCount()
   METHOD Drop( x )
   METHOD Failing( x ) INLINE x + 1
   METHOD Mixed( a, b ) INLINE a + b
   METHOD Twice() INLINE ::Area() * 2
ENDCLASS

METHOD Drop( x ) CLASS TShape
   x := NIL
   __GcCollect()
RETURN s_nGone

CLASS TShape1 INHERIT TShape
ENDCLASS
CLASS TShape2 INHERIT TShape
   VAR cExtra INIT "two"
   METHOD Area() INLINE ::nSide * 2
   METHOD Name() INLINE "second"
   METHOD Mixed( a, b, c ) INLINE IIf( c == NIL, "no c", "c" )
ENDCLASS
CLASS TShape3 INHERIT TShape
   METHOD Area() INLINE ::nSide + 3
   METHOD Mixed( a ) INLINE a * 100
   METHOD Twice() INLINE ::Super:Area() * 2
ENDCLASS
CLASS TShape4 INHERIT TShape
   VAR a, b, c
   METHOD Name() INLINE "fourth"
   METHOD Mixed( a, b ) INLINE "s" + LTrim( Str( a + b ) )
ENDCLASS
CLASS TShape5 INHERIT TShape
   METHOD Area() INLINE 5
ENDCLASS
CLASS TShape6 INHERIT TShape
   METHOD Scaled( k ) INLINE k
ENDCLASS
CLASS TShape7 INHERIT TShape
   METHOD Pair( a, b ) INLINE a + b
ENDCLASS
CLASS TShape8 INHERIT TShape
   METHOD Area() INLINE 8.5
ENDCLASS
CLASS TShape9 INHERIT TShape
   METHOD Name() INLINE "ninth"
ENDCLASS
CLASS TShape10
   VAR nTag INIT 100
   VAR nSide INIT 0
   METHOD New( n ) INLINE ::nSide := n, Self
   METHOD Area() INLINE 10
   METHOD Scaled( k ) INLINE k * 10
   METHOD Pair( a, b ) INLINE a - b
   METHOD Name() INLINE "tenth"
   METHOD Mixed( a, b ) INLINE a - b
ENDCLASS

CLASS TGone
   DESTRUCTOR Bye()
ENDCLASS

METHOD PROCEDURE Bye CLASS TGone
   Gone()
RETURN
