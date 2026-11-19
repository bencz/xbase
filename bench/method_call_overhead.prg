// What a method call costs: 5,000,000 rounds of calls with no argument, one and three arguments, an INLINE method and
// a method that calls another through Self; on an object of a class the compiler knows (it may resolve and inline the
// calls), then on one made by a macro, whose class it cannot know, so that each call is a send through its cache.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), o := TCalc():New(), u := &( "TCalc():New()" ), i, n := 0
   FOR i := 1 TO 5000000
      n += o:Zero() + o:One( i ) + o:Three( i, 2, 3 ) + o:Twice( i ) + o:Chain( i )
      n += u:Zero() + u:One( i ) + u:Three( i, 2, 3 ) + u:Twice( i ) + u:Chain( i )
   NEXT
   // each round adds 2 * ( 6 * i + 2 )
   ? "method_call_overhead", ElapsedMs() - t, IIf( n == 150000050000000, "ok", "WRONG" )
RETURN

CLASS TCalc
   VAR nBase INIT 1
   METHOD Zero()
   METHOD One( x )
   METHOD Three( x, y, z )
   METHOD Twice( x ) INLINE x + x
   METHOD Chain( x )
ENDCLASS

METHOD Zero() CLASS TCalc
RETURN ::nBase

METHOD One( x ) CLASS TCalc
RETURN x

METHOD Three( x, y, z ) CLASS TCalc
RETURN x + y + z - 5

METHOD Chain( x ) CLASS TCalc
RETURN ::One( x ) + ::One( x ) + 1
