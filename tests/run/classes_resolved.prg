// Sends to objects whose class the compiler knows (made by the class function and New() in the function), which it
// resolves as the runtime would: each one must give what the runtime's dispatch gives (the same program at -O0
// resolves nothing). Overrides at several depths; multiple inheritance (the first parent's member, its variables in
// their own areas, a diamond whose common ancestor comes once); Super chains three deep; a method's message to Self
// reaching a subclass's override, its own HIDDEN member over a subclass's, ACCESS and ASSIGN over an inherited
// variable; HIDDEN, PROTECTED and READONLY members from outside (errors), from the class and from a subclass; a FRIEND
// CLASS; class variables, SHARED ones too; New() with a method of its own (which returns something else), Init with
// arguments; reads with arguments, assignments with none or two; fewer and more arguments than parameters; arguments
// passed by reference (@x, @a[ i ]) to methods (which change them) and to an assignment (which takes the value), to an
// object whose class is known and to one whose class is not, to an array's method and to a codeblock's Eval; a message
// no member answers; an error inside methods (the call stack when it happens); a recursive method; Self assigned in a
// method; objects whose class is not known (from an array) at the same sends; a class of the line named New (New() is
// then a cast); a message with an argument by reference to a value that is no object (its error's arguments hold the
// value).
PROCEDURE Main()
   LOCAL o, p, x, a, b
   ErrorBlock( {| e | Break( { e, Stack() } ) } )

   o := TLeaf():New()
   ? "overrides", o:Who(), o:Base(), o:Mid(), o:Leaf(), o:Chain()
   o := TMid():New()
   ? "mid", o:Who(), o:Chain()

   o := TMulti():New()
   ? "multiple", o:Who(), o:v, o:TB:v, o:Both(), o:OnlyB()
   o:v := 10
   ? "multiple set", o:v, o:TB:v, o:TA:v
   o := TDiamond():New()
   ? "diamond", o:Count(), o:d, o:top, o:Who()

   o := TFrom():New()
   ? "from", o:Run(), o:Call()
   o := TFromSub():New()
   ? "from sub", o:Run(), o:Call(), o:secret

   o := TTemp():New()
   o:Celsius := 100
   ? "access", o:Celsius, o:Fahrenheit, o:nC

   o := TScope():New()
   ? "scope", Try( {|| o:pub } ), Try( {|| o:hid } ), Try( {|| o:pro } ), Try( {|| o:ro } ), ;
      Try( {|| o:ro := 5 } ), Try( {|| o:_hid( 1 ) } ), o:Inside(), o:ro
   p := TScopeSub():New()
   ? "scope sub", p:FromSub(), Try( {|| p:hid } ), Try( {|| p:pro } )
   ? "friend", TFriendly():New():Peek( o )

   o := TCounted():New()
   p := TCounted():New()
   o:Bump()
   p:Bump()
   ? "class vars", o:n, p:n, o:total, p:total, TCountedSub():New():total

   o := TMade():New( 3, 4 )
   ? "init", o:a, o:b, o:Sum()
   x := TOwnNew():New()
   ? "own new", ValType( x ), x

   o := TArgs():New()
   ? "arguments", o:v( 1, 2 ), Try( {|| o:_v() } ), o:v, Try( {|| o:_v( 7, 8 ) } ), o:v
   ? "counts", o:Two( 5 ), o:Two( 5, 6, 7 ), o:Two()
   x := 1
   b := { 10, 20 }
   o:Inc( @x )
   o:Inc( @b[ 2 ] )
   o:Inc( @x )
   o:_v( @x ) // the value, not the reference
   x := 100
   ? "by reference", x, b[ 2 ], o:v, ByRef( { o }, @x ), x, NotObject()
   o := TNamed():New() // a class of the line named New: New() is a cast to it
   ? "cast by New", o:v, o:ClassName(), o:Who()

   ? "not found", Try( {|| o:Nothing() } ), Try( {|| o:nothing := 1 } )
   ? "error", Try( {|| o:Fail( 2 ) } )
   ? "recursive", TMath():New():Fact( 10 ), TMath():New():Fib( 20 )
   o := TSelf():New()
   ? "self", o:Swap(), o:tag

   a := { TLeaf():New(), TMid():New(), TBase():New() }
   b := ""
   FOR EACH x IN a
      b += x:Who() + "/" + x:Chain() + " "
   NEXT
   ? "unknown", b
RETURN

// The error a block raises (with the functions running when it happened), or its value.
STATIC FUNCTION Try( b )
   LOCAL r, x
   BEGIN SEQUENCE
      x := Eval( b )
   RECOVER USING r
      RETURN "E" + LTrim( Str( r[ 1 ]:SubCode ) ) + ":" + r[ 1 ]:Operation + ":" + r[ 1 ]:Description + r[ 2 ]
   END SEQUENCE
RETURN x

// The functions running when the error block is called, from the one that failed to the block's caller of Try().
STATIC FUNCTION Stack()
   LOCAL c := "", k := 2
   DO WHILE ! Empty( ProcName( k ) ) .AND. ! ProcName( k ) == "TRY"
      c += "<" + ProcName( k ) + "(" + LTrim( Str( ProcLine( k ) ) ) + ")"
      k++
   ENDDO
RETURN c

CLASS TBase
   METHOD Who() INLINE "base"
   METHOD Base() INLINE "B"
   METHOD Chain() INLINE "b"
ENDCLASS

CLASS TMid INHERIT TBase
   METHOD Who() INLINE "mid"
   METHOD Mid() INLINE "M" + ::Who()
   METHOD Chain() INLINE "m" + ::Super:Chain()
ENDCLASS

CLASS TLeaf INHERIT TMid
   METHOD Who() INLINE "leaf"
   METHOD Leaf() INLINE "L" + ::Base()
   METHOD Chain() INLINE "l" + ::Super:Chain()
ENDCLASS

CLASS TA
   VAR v INIT "a"
   METHOD Who() INLINE "A"
   METHOD Both() INLINE "A:" + ::v
ENDCLASS

CLASS TB
   VAR v INIT "b"
   METHOD Who() INLINE "B"
   METHOD Both() INLINE "B:" + ::v
   METHOD OnlyB() INLINE "onlyB:" + ::v
ENDCLASS

CLASS TMulti INHERIT TA, TB
ENDCLASS

CLASS TTop
   VAR top INIT 0
   METHOD Count() INLINE ++::top
   METHOD Who() INLINE "top"
ENDCLASS

CLASS TLeft INHERIT TTop
   METHOD Who() INLINE "left"
ENDCLASS

CLASS TRight INHERIT TTop
   METHOD Who() INLINE "right"
ENDCLASS

CLASS TDiamond INHERIT TLeft, TRight
   VAR d INIT "d"
ENDCLASS

CLASS TFrom
   HIDDEN:
   VAR secret INIT "own"
   EXPORTED:
   METHOD Run() INLINE ::secret + "/" + ::Kind()
   METHOD Kind() INLINE "from"
   METHOD Call() INLINE ::Run()
ENDCLASS

CLASS TFromSub INHERIT TFrom
   VAR secret INIT "sub"
   METHOD Kind() INLINE "sub"
ENDCLASS

CLASS TTemp
   VAR nC INIT 0
   ACCESS Celsius INLINE ::nC
   ASSIGN Celsius( v ) INLINE ::nC := v
   ACCESS Fahrenheit INLINE ::nC * 9 / 5 + 32
ENDCLASS

CLASS TScope
   EXPORTED:
   VAR pub INIT "pub"
   VAR ro INIT "ro" READONLY
   METHOD Inside() INLINE ::hid + ::pro + ( ::ro := "RO" )
   HIDDEN:
   VAR hid INIT "hid"
   PROTECTED:
   VAR pro INIT "pro"
   FRIEND CLASS TFriendly
ENDCLASS

CLASS TScopeSub INHERIT TScope
   METHOD FromSub()
ENDCLASS

METHOD FromSub() CLASS TScopeSub
   LOCAL o := Self
RETURN ::pro + Try( {|| o:hid } )

CLASS TFriendly
   METHOD Peek( o ) INLINE o:hid + o:pro
ENDCLASS

CLASS TCounted
   VAR n INIT 0
   CLASSVAR total INIT 0 SHARED
   METHOD Bump() INLINE ::n++, ::total++
ENDCLASS

CLASS TCountedSub INHERIT TCounted
ENDCLASS

CLASS TMade
   VAR a
   VAR b
   METHOD Init( x, y ) INLINE ::a := x, ::b := y, Self
   METHOD Sum() INLINE ::a + ::b
ENDCLASS

CLASS TOwnNew
   METHOD New() INLINE "made by New"
ENDCLASS

CLASS TArgs
   VAR v INIT 0
   METHOD Two( p, q ) INLINE ValType( p ) + ValType( q )
   METHOD Inc( n ) INLINE n++
   METHOD Fail( k ) INLINE iif( k == 0, ::v + "x", ::Fail( k - 1 ) )
ENDCLASS

CLASS TMath
   METHOD Fact( n )
   METHOD Fib( n )
ENDCLASS

METHOD Fact( n ) CLASS TMath
   IF n <= 1
      RETURN 1
   ENDIF
RETURN n * ::Fact( n - 1 )

METHOD Fib( n ) CLASS TMath
   IF n < 2
      RETURN n
   ENDIF
RETURN ::Fib( n - 1 ) + ::Fib( n - 2 )

CLASS TSelf
   VAR tag INIT "first"
   METHOD Swap()
ENDCLASS

METHOD Swap() CLASS TSelf
   LOCAL o := Self
   Self := TOther():New()
RETURN ::tag + "/" + o:tag

CLASS TOther
   VAR tag INIT "other"
ENDCLASS

// Arguments by reference where the object's class is not known (from an array): the method changes the caller's
// variable; an array's Add and a codeblock's Eval take the reference too; a class variable assigned takes the value.
STATIC FUNCTION ByRef( aBox, n )
   LOCAL a := {}, b := {| k | k += 1000 }
   aBox[ 1 ]:Inc( @n )
   a:Add( @n )
   Eval( b, @n )
   b:Eval( @n )
   TCounted():New():_total( @n )
RETURN LTrim( Str( n ) ) + "/" + LTrim( Str( a[ 1 ] ) ) + "/" + LTrim( Str( TCounted():New():total ) )

// A message with @y to a number, which answers none: the error's arguments hold the value, never a reference.
STATIC FUNCTION NotObject()
   LOCAL y := 21, n := 3, a, b := ErrorBlock( {| e | Break( e:args ) } )
   BEGIN SEQUENCE
      n:Foo( @y )
   RECOVER USING a
   END SEQUENCE
   ErrorBlock( b )
RETURN ValType( a[ 2 ] ) + LTrim( Str( a[ 2 ] * 2 ) )

CLASS New
   VAR v INIT "base"
   METHOD Who() INLINE "base who"
ENDCLASS

CLASS TNamed INHERIT New
   VAR v INIT "child"
   METHOD Who() INLINE "child who"
ENDCLASS
